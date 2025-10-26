#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <any>
#include <mutex>
#include <iostream>
#include <utility>

#include <uwebsockets/App.h>

#include <ILogger.hpp>

namespace KaitoTokyo {
namespace WebSocket {

class WebSocketServer {
public:
    // コンストラクタ
    WebSocketServer(const KaitoTokyo::BridgeUtils::ILogger& logger, int port)
        : logger_(logger),
          port_(port),
          app_(std::make_unique<uWS::App>()),
          running_(false),
          listen_attempted_(false) // listen試行済みフラグは残す
    {
        logger_.info("WebSocketServer instance created for port {}.", port_);
    }

    // デストラクタ
    ~WebSocketServer() {
        logger_.info("WebSocketServer instance destroying for port {}.", port_);
        stop();
    }

    // コピー禁止, ムーブ禁止
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;
    WebSocketServer(WebSocketServer&&) = delete;
    WebSocketServer& operator=(WebSocketServer&&) = delete;

    template <typename USERDATA>
    WebSocketServer& addWebSocketHandler(std::string pattern, uWS::App::WebSocketBehavior<USERDATA>&& behavior) {
        if (!app_) {
            logger_.error("Error: uWS::App instance is null in addWebSocketHandler for port {}.", port_);
            return *this;
        }
        if (listen_attempted_) {
            logger_.warn("Attempting to add WebSocket handler after listen was called for port {}.", port_);
        }
        logger_.info("Adding WebSocket handler for path '{}' on port {}.", pattern, port_);
        app_->ws<USERDATA>(pattern, std::move(behavior));
        return *this;
    }

    /**
     * @brief サーバーのリッスン操作を開始し、別スレッドでイベントループを開始する。
     * このメソッドはブロックせず、リッスンの成否も返さない。
     * サーバーの状態確認は isListening() で行う必要がある。
     */
    void run() {
        // 既に実行中、または listen 試行中の場合は早期リターン
        if (running_ || listen_attempted_.exchange(true)) {
            logger_.warn("WebSocketServer on port {} is already running or listen was already attempted.", port_);
            return; // 何もせずリターン
        }

        // ★★★ イベントループスレッドを開始 ★★★
        running_ = true; // 仮に実行中状態とする (listen 失敗/スレッド終了時に false に戻す)
        logger_.info("WebSocketServer starting event loop thread on port {}.", port_);
        serverThread_ = std::thread([this]() {
            try {
                uWS::Loop::get(); // スレッドローカルループ初期化

                // ★★★ listen をイベントループスレッド内で行う ★★★
                app_->listen(port_, [this](auto* token) {
                    this->listen_socket_ = token;
                    this->listening_ = (this->listen_socket_ != nullptr);
                    if (this->listening_) {
                        logger_.info("WebSocketServer listening on port {}.", port_);
                    } else {
                        logger_.error("WebSocketServer failed to listen on port {}.", port_);
                        // listen 失敗した場合、実行中フラグなどをリセット
                        this->running_ = false; // 実行中ではない
                        this->listen_attempted_ = false; // 再試行可能にする
                        // ★★★ listen に失敗したら、run() を呼び出さない ★★★
                    }
                    // Promise/Future を削除したので set_value は不要
                });

                // ★★★ listening_ フラグが true になった場合のみ run() を呼び出す ★★★
                // listen コールバックが完了するのを待つ単純な方法 (ただし確実ではない)
                // より確実にするには、listen コールバック内で run を呼ぶか、
                // condition_variable などで同期する必要がある。
                // ここでは listen が同期的か、すぐに完了すると仮定して進める。
                // listen コールバックが呼ばれる前に run() が呼ばれるリスクがある。
                // より安全なのは listen コールバック内で run を呼ぶことだが、
                // uWebSockets の内部実装によっては問題が起きる可能性もある。

                // --- listen コールバック内で run を呼ぶ代替案 ---
                // app_->listen(port_, [this](auto* token){
                //    ... (listening_ フラグ設定) ...
                //    if (this->listening_) {
                //        logger_.info("Starting app->run() for port {} inside listen callback.", port_);
                //        this->app_->run(); // ここでブロック
                //    } else {
                //        logger_.info("Exiting server thread due to listen failure on port {}.", port_);
                //    }
                // });
                // // この場合、以下の app_->run() は不要になる

                // --- 元の実装に近い形 (listen が同期的であると仮定) ---
                if (this->listening_) { // listen コールバックで設定されたフラグを確認
                     logger_.info("Entering app->run() for port {}.", port_);
                    this->app_->run(); // ここでスレッドをブロック
                } else {
                     // listen に失敗した場合、スレッドはすぐに終了
                     logger_.info("Exiting server thread due to listen failure on port {}.", port_);
                }


            } catch (const std::exception& e) {
                logger_.error("Exception caught in server thread for port {}: {}", port_, e.what());
                 this->listening_ = false;
                 this->running_ = false;
                 this->listen_attempted_ = false;
            } catch (...) {
                logger_.error("Unknown exception caught in server thread for port {}.", port_);
                 this->listening_ = false;
                 this->running_ = false;
                 this->listen_attempted_ = false;
            }
            // run() が終了した後の状態リセット
            logger_.info("WebSocketServer event loop stopped on port {}.", port_);
            this->listening_ = false;
            this->running_ = false;
            this->listen_attempted_ = false;
        });

        // スレッドを開始したらすぐにリターン (Future を返さない)
        logger_.debug("run() method initiated server thread for port {} and returned.", port_);
    }

    // stop: サーバーを停止 (変更なし)
    void stop() {
        if (running_.exchange(false)) {
            logger_.info("Stopping WebSocketServer on port {}.", port_);

            if (app_ && listen_socket_) {
                uWS::Loop* loop = nullptr;
                try {
                     loop = app_->getLoop();
                } catch (...) {
                     logger_.error("Could not get loop instance from app on port {}. Attempting direct close.", port_);
                }

                if (loop && serverThread_.joinable() && serverThread_.get_id() != std::this_thread::get_id()) {
                    loop->defer([this]() {
                        if (listen_socket_) {
                            logger_.debug("Closing listen socket on port {} (deferred)...", port_);
                            us_listen_socket_close(0, listen_socket_);
                            listen_socket_ = nullptr;
                        }
                        logger_.debug("Closing App on port {} (deferred)...", port_);
                        app_->close();
                        logger_.debug("App close initiated on port {} (deferred).", port_);
                    });
                } else {
                     logger_.debug("Attempting direct close for port {}...", port_);
                     if (listen_socket_) {
                        logger_.debug("Closing listen socket directly.");
                        us_listen_socket_close(0, listen_socket_);
                        listen_socket_ = nullptr;
                     }
                     if (app_) {
                         logger_.warn("Closing App directly on port {}. This might be unsafe if the event loop is running on another thread.", port_);
                        app_->close();
                         logger_.debug("App close initiated directly.");
                     }
                }
            } else {
                 logger_.warn("Warning: App instance or listen socket seems invalid during stop() on port {}.", port_);
            }

            if (serverThread_.joinable()) {
                 logger_.debug("Waiting for server thread to join for port {}...", port_);
                serverThread_.join();
                 logger_.debug("Server thread joined for port {}.", port_);
            } else {
                 logger_.debug("Server thread was not joinable for port {}.", port_);
            }
            listening_ = false;
            listen_attempted_ = false;
        } else {
             logger_.info("WebSocketServer on port {} was already stopped or not running/listening.", port_);
        }
    }

    // isListening: リッスン状態を取得
    bool isListening() const {
        return listening_;
    }

    // getPort: ポート番号を取得
    int getPort() const {
        return port_;
    }

    // getApp: uWS::App インスタンスへのポインタを取得
     uWS::App* getApp() const {
         return app_.get();
     }

private:
    const KaitoTokyo::BridgeUtils::ILogger& logger_;
    int port_;
    std::unique_ptr<uWS::App> app_;
    us_listen_socket_t *listen_socket_ = nullptr;
    std::atomic<bool> listening_ = false; // リッスン状態
    std::thread serverThread_;
    std::atomic<bool> running_ = false; // スレッドが動作中か (stop 制御用)
    std::atomic<bool> listen_attempted_ = false; // listen が試行されたか
};

} // namespace WebSocket
} // namespace KaitoTokyo
