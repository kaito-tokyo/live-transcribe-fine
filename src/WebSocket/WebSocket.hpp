/*
Websocket
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

#include <memory>

#include <uwebsockets/App.h>

namespace KaitoTokyo {
namespace WebSocket {

// uWebSocketsを使ってクライアントとのWebSocket接続を管理するクラス群
// WebSocketPoolクラスは複数のポートのWebSocketサーバーを管理するルートクラス
// WebSocketServerクラスは特定のポートでWebSocketサーバーを立ち上げ、接続を管理する
// WebSocketContextクラスは個々のWebSocketエンドポイントを管理し、メッセージの送受信を行う

// WebSocketとのやりとりに関する振る舞いはこのクラスで全て定義される
class WebSocketContext {
public:
    WebSocketContext(std::string path, std::string password);
    void broadcastText(const std::string& message);
};

class WebSocketServer {
public:
    WebSocketServer(int port);

    // 指定されたパスでWebSocketエンドポイントを作成し、対応するコンテキストを返す。
    std::shared_ptr<class WebSocketContext> createContext(const std::string& path);
};

class WebSocketPool {
public:
    // portに対応するWebSocketServerを確実に立ち上がっている状態にする。
    void ensurePort(int port);

    // portに対応するWebSocketServerをshared_ptrとして返す。
    std::shared_ptr<WebSocketServer> getPortServer(int port);

    // WebSocketPoolではサーバーをweak_ptrで管理する。
    // WebSocketServerはサービスが管理するが、そのポートで発信するサービスがなくなったら自動的に解放する。
};

}
}
