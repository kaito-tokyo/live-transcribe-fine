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

#include <string>
#include <thread>

#include <uwebsockets/App.h>

#include <ILogger.hpp>

namespace KaitoTokyo {
namespace WebSocket {

class BroadcastWebSocketServer {
public:
    BroadcastWebSocketServer(const BridgeUtils::ILogger &logger, int port) : logger_(logger) {
        serverThread_ = std::thread([this, port]() {
            uWS::App::WebSocketBehavior<void> behavior;

            behavior.compression = uWS::DISABLED;
            behavior.maxPayloadLength = 16 * 1024 * 1024;
            behavior.idleTimeout = 0;
            behavior.maxBackpressure = 1 * 1024 * 1024;
            behavior.closeOnBackpressureLimit = false;
            behavior.resetIdleTimeoutOnSend = true;
            behavior.sendPingsAutomatically = true;
            behavior.open = [this](auto *ws) {
                ws->subscribe(broadcastTopic);
            };
        });
    }

    ~BroadcastWebSocketServer() {

    }

    void broadcast(const std::string& message) {

    }

private:
    const BridgeUtils::ILogger &logger_;
    std::thread serverThread_;

    const char *broadcastTopic = "broadcast"
};

} // namespace WebSocket
} // namespace KaitoTokyo
