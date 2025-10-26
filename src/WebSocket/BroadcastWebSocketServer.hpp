/*
WebSocket
Copyright (C) 2025 Kaito Udagawa umireon@kaito.tokyo

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <atomic>
#include <condition_variable> // For startup synchronization
#include <future>         // For std::promise, std::future
#include <mutex>
#include <string_view>
#include <string>
#include <thread>

#include <libusockets.h> // Include libusockets for us_listen_socket_close
#include <uwebsockets/App.h>
#include <uwebsockets/Loop.h> // For uWS::Loop::defer

#include <ILogger.hpp>

namespace KaitoTokyo {
namespace WebSocket {

/**
 * @class BroadcastWebSocketServer
 * @brief A simplified WebSocket server dedicated to broadcasting messages to all connected clients.
 *
 * This server runs its uWebSockets event loop in a separate thread and provides
 * a thread-safe method (`broadcast`) to send messages to all subscribed clients.
 * It manages its own uWS::App instance internally within the server thread.
 *
 * @note This class is non-copyable and non-movable.
 */
class BroadcastWebSocketServer {
public:
    /**
     * @brief Constructs the BroadcastWebSocketServer. Does not start the server thread yet.
     * @param logger Reference to the logger implementation.
     * @param port The port number the server will attempt to listen on.
     */
    BroadcastWebSocketServer(const BridgeUtils::ILogger &logger, int port)
        : logger_(logger),
          port_(port),
          running_(false),
          listen_success_(false),
          listen_socket_(nullptr),
          app_(nullptr),
          eventLoop_(nullptr)
    {
        logger_.info("Initializing BroadcastWebSocketServer for port {}.", port_);
    }

    /**
     * @brief Destructor. Ensures the server thread is stopped safely.
     */
    ~BroadcastWebSocketServer()
    {
        logger_.info("Destroying BroadcastWebSocketServer for port {}.", port_);
        stop(); // Ensure stop is called upon destruction
    }

    // Delete copy constructor and assignment operator
    BroadcastWebSocketServer(const BroadcastWebSocketServer&) = delete;
    BroadcastWebSocketServer& operator=(const BroadcastWebSocketServer&) = delete;

    // Delete move constructor and assignment operator
    BroadcastWebSocketServer(BroadcastWebSocketServer&&) = delete;
    BroadcastWebSocketServer& operator=(BroadcastWebSocketServer&&) = delete;

    /**
     * @brief Starts the server thread which initializes and runs the uWebSockets app.
     * This method blocks until the listen operation completes (either successfully or with failure).
     * @return True if the server started listening successfully, false otherwise.
     */
    bool start() {
        // Prevent starting if already running
        if (running_) {
            logger_.warn("Server on port {} is already running or starting.", port_);
            return listen_success_;
        }

        std::promise<bool> listen_promise;
        std::future<bool> listen_future = listen_promise.get_future();

        running_ = true; // Set running flag before creating the thread
        serverThread_ = std::thread([this, p = std::move(listen_promise)]() mutable {
            // Initialize thread-local event loop for this thread
            uWS::Loop::get();

            uWS::App app; // uWS::App instance on the server thread's stack
            uWS::Loop* loop = app.getLoop(); // Get the loop associated with this app

            { // Scope for locking the mutex
                std::lock_guard<std::mutex> lock(appMutex_);
                app_ = &app;
                eventLoop_ = loop; // Store the loop pointer for defer calls
            }

            // Configure WebSocket behavior
            uWS::App::WebSocketBehavior<void> behavior;
            behavior.compression = uWS::DISABLED; // Compression often less effective for broadcast
            behavior.maxPayloadLength = 16 * 1024 * 1024; // 16MB limit
            behavior.idleTimeout = 0; // Disable idle timeout
            behavior.maxBackpressure = 1 * 1024 * 1024; // 1MB backpressure limit
            behavior.closeOnBackpressureLimit = false; // Don't close on limit, just drop messages implicitly
            behavior.resetIdleTimeoutOnSend = false; // Not relevant with idleTimeout = 0
            behavior.sendPingsAutomatically = false; // Not relevant with idleTimeout = 0

            // Handler for new WebSocket connections
            behavior.open = [this](auto *ws) {
                if (ws) {
                    ws->subscribe(broadcastTopic); // Subscribe client to the broadcast topic
                    logger_.info("WebSocket client connected (port {}) and subscribed to '{}'.", port_, broadcastTopic);
                } else {
                     logger_.warn("WebSocket open handler called with null ws pointer (port {}).", port_);
                }
            };

            // Handler for WebSocket disconnections
            behavior.close = [this](auto* ws, int code, std::string_view message) {
                 // ws pointer is valid for logging/identification but not I/O here
                 logger_.info("WebSocket client disconnected (port {}) with code {}.", port_, code);
            };

            // Handler for incoming messages (typically ignored in broadcast-only server)
            behavior.message = [this](auto* ws, std::string_view message, uWS::OpCode opCode){
                // logger_.debug("Received message on broadcast server (port {}), ignoring.", port_);
            };

            // Handler for when backpressure has drained (optional)
            behavior.drain = [this](auto* ws){
                // logger_.debug("WebSocket drain event (port {}), buffered: {}", port_, ws->getBufferedAmount());
            };

            // Register the WebSocket behavior for all paths
            app.ws<void>("/*", std::move(behavior)); // Move behavior struct into the app

            bool success = false;
            // Attempt to listen on the specified port
            app.listen(port_, [this, &success, &p](auto *token) {
                 // Lock mutex before accessing shared listen_socket_ and potentially app_/eventLoop_
                 std::lock_guard<std::mutex> lock(appMutex_);
                listen_socket_ = token; // Store the listen socket token
                success = (token != nullptr);
                if (!success) {
                    // If listen failed, reset pointers immediately
                    app_ = nullptr;
                    eventLoop_ = nullptr;
                }

                // Notify the main thread about the listen result via promise
                try {
                     p.set_value(success); // Set the promise value
                     if (success) {
                         logger_.info("BroadcastWebSocketServer listening on port {}.", port_);
                     } else {
                         logger_.error("Failed to listen on port {}.", port_);
                     }
                } catch (const std::future_error& e) {
                     // This might happen if start() is called multiple times very quickly,
                     // though the outer running_ check should prevent it.
                     logger_.warn("Promise already satisfied during listen callback (port {}): {}", port_, e.what());
                }
            });

            // Only run the event loop if listen was successful
            if (success) {
                // This call blocks until the loop is stopped (e.g., by app.close())
                app.run();
                logger_.info("Event loop finished for port {}.", port_);
            } else {
                 // If listen failed, log and the thread will exit
                 logger_.error("Server thread exiting for port {} because listen failed.", port_);
            }

            // Cleanup before thread exits
            {
                std::lock_guard<std::mutex> lock(appMutex_);
                app_ = nullptr;
                listen_socket_ = nullptr;
                eventLoop_ = nullptr;
            }
             // Reset state flags
             running_ = false; // Mark as not running
             listen_success_ = false; // Mark as not listening

        }); // End of server thread lambda

        // Wait for the listen operation to complete in the server thread
        logger_.debug("Waiting for listen result for port {}...", port_);
        listen_success_ = listen_future.get(); // Blocks until promise is set
        logger_.debug("Listen result received for port {}: {}", port_, listen_success_);

        // If listen failed, the server thread will exit quickly. Join it now.
        if (!listen_success_) {
            running_ = false; // Ensure running flag is false if listen failed
             if (serverThread_.joinable()) {
                 serverThread_.join();
             }
             logger_.error("Server thread exited prematurely due to listen failure on port {}.", port_);
        }

        return listen_success_; // Return the success status of the listen operation
    }

    /**
     * @brief Stops the server thread safely.
     * This method is thread-safe and idempotent (calling it multiple times has no effect after the first call).
     * It defers the actual closing operations to the server's event loop thread.
     */
    void stop() {
        // Use atomic exchange to ensure stop logic runs only once
        if (running_.exchange(false)) { // If running was true, set it to false and proceed
            logger_.info("Stopping BroadcastWebSocketServer for port {}...", port_);

            us_listen_socket_t* tokenToClose = nullptr;
            uWS::Loop* loopToDefer = nullptr;
            uWS::App* appToClose = nullptr;

            { // Scope for mutex lock
                 std::lock_guard<std::mutex> lock(appMutex_);
                 // Copy pointers needed for defer lambda while holding the lock
                 tokenToClose = listen_socket_;
                 loopToDefer = eventLoop_;
                 appToClose = app_;

                 // Set member pointers to null to prevent further use
                 listen_socket_ = nullptr;
                 eventLoop_ = nullptr;
                 app_ = nullptr;
            }

            if (loopToDefer) {
                // Defer the closing operations to the event loop thread
                loopToDefer->defer([this, token = tokenToClose, app = appToClose]() {
                    logger_.debug("Closing listen socket and app on port {} (deferred)...", port_);
                     // Close the listen socket if it was successfully opened
                     if(token) {
                         // The first argument '0' indicates non-SSL context for uWS::App
                         us_listen_socket_close(0, token);
                     }
                     // Close the uWS::App instance, which stops the event loop
                     if (app) {
                         app->close();
                     } else {
                          // This might happen if stop is called very quickly after a failed start
                          logger_.warn("App pointer was already null inside deferred stop for port {}.", port_);
                     }
                });
                 logger_.debug("Deferred stop request for port {}.", port_);
            } else {
                 // This case might occur if start() failed before initializing the loop,
                 // or if stop() is called after the thread has already terminated.
                 logger_.warn("Event loop pointer was null during stop() for port {}. Cannot defer close. Thread might have already stopped or failed to start.", port_);
            }

            // Wait for the server thread to complete
            if (serverThread_.joinable()) {
                logger_.debug("Waiting for server thread to join for port {}...", port_);
                try {
                     serverThread_.join(); // Blocks until the thread finishes execution
                     logger_.debug("Server thread joined for port {}.", port_);
                } catch (const std::system_error& e) {
                     // Handle potential errors during join (e.g., thread already joined)
                     logger_.error("Error joining server thread for port {}: {}", port_, e.what());
                }
            } else {
                 // If the thread wasn't joinable, it might have already finished or never started properly.
                 logger_.debug("Server thread was not joinable during stop() for port {}. It might have already finished.", port_);
            }

            logger_.info("BroadcastWebSocketServer stopped for port {}.", port_);
             listen_success_ = false; // Update listening status
        } else {
            // If running_ was already false when exchange was called
            logger_.info("BroadcastWebSocketServer on port {} was already stopped or not running.", port_);
            // If the thread object still exists and is joinable (e.g., stop called multiple times), try joining again.
             if (serverThread_.joinable()) {
                 try {
                     serverThread_.join();
                 } catch (const std::system_error& e) {
                     logger_.warn("Tried joining already stopped/non-running thread for port {}: {}", port_, e.what());
                 }
             }
        }
    }


    /**
     * @brief Broadcasts a text message to all clients subscribed to the broadcast topic.
     * This method is thread-safe. It defers the actual publish operation to the server's event loop thread.
     * @param message The message content to broadcast.
     */
    void broadcast(const std::string &message) {
        uWS::Loop* loopToDefer = nullptr;
        uWS::App* appToPublish = nullptr;

        { // Scope for mutex lock
            std::lock_guard<std::mutex> lock(appMutex_);
            // Get pointers safely
            loopToDefer = eventLoop_;
            appToPublish = app_;
        }

        // Check if the server loop is available and the server is marked as running
        if (loopToDefer && running_) {
             // Copy the message to ensure its lifetime until the deferred lambda executes
             std::string msg_copy = message;
             // Defer the publish operation to the event loop thread
             loopToDefer->defer([this, app = appToPublish, msg = std::move(msg_copy)]() {
                 // Double-check pointers and running state inside the deferred lambda,
                 // as the server might have been stopped between the defer call and its execution.
                 if (app && running_) {
                     app->publish(broadcastTopic, msg, uWS::OpCode::TEXT);
                     // Avoid logging every broadcast message in production unless necessary
                     // logger_.debug("Broadcasted message (deferred): {}", msg);
                 } else {
                      logger_.warn("App or server became invalid before deferred broadcast could execute on port {}.", port_);
                 }
             });
        } else if (!running_) {
             // Log a warning if trying to broadcast while the server is not running
             logger_.warn("WebSocket server on port {} is not running. Cannot broadcast message.", port_);
        } else {
            // Log an error if the loop pointer is unexpectedly null (shouldn't happen if start succeeded)
            logger_.error("WebSocket event loop pointer is null (port {}). Cannot broadcast message.", port_);
        }
    }

     /**
      * @brief Checks if the server is currently listening on its port.
      * @return True if the server successfully started listening, false otherwise.
      */
     bool isListening() const {
         // Read the atomic boolean flag
         return listen_success_;
     }

private:
    // Dependencies
    const BridgeUtils::ILogger &logger_; ///< Reference to the logging interface.

    // Configuration
    const int port_;                     ///< Port number to listen on.

    // Threading and State
    std::thread serverThread_;           ///< The thread running the uWebSockets event loop.
    std::atomic<bool> running_;          ///< Flag indicating if the server is running or in the process of starting/stopping.
    std::atomic<bool> listen_success_;   ///< Flag indicating if the server successfully started listening.

    // uWebSockets Objects (valid only within the server thread or under mutex protection)
    us_listen_socket_t* listen_socket_;  ///< Raw pointer to the underlying listen socket (managed by uSockets).
    uWS::App *app_;                      ///< Pointer to the uWS::App instance (lives on serverThread_ stack).
    uWS::Loop* eventLoop_;               ///< Pointer to the event loop running on serverThread_.

    // Synchronization
    std::mutex appMutex_;                ///< Mutex protecting access to app_, eventLoop_, and listen_socket_ pointers from different threads.

    // Constants
    static constexpr const char *broadcastTopic = "broadcast"; ///< Topic name used for broadcasting messages.
};

} // namespace WebSocket
} // namespace KaitoTokyo
