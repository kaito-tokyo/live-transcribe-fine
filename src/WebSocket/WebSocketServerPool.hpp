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

// #pragma once を追加 (ヘッダーガード)
#pragma once

#include <memory>              // std::shared_ptr, std::weak_ptr, std::make_shared
#include <unordered_map>       // std::unordered_map
#include <vector>              // std::vector
#include <mutex>               // std::mutex, std::lock_guard

#include <ILogger.hpp>         // ILogger インターフェース

#include "WebSocketServer.hpp" // WebSocketServer クラス定義

namespace KaitoTokyo {
namespace WebSocket {

class WebSocketServerPool {
public:
    // コンストラクタ: ILogger への参照を受け取る
    explicit WebSocketServerPool(const KaitoTokyo::BridgeUtils::ILogger &logger) : logger_(logger) {
        // explicit を追加して意図しない変換を防ぐ
        logger_.info("WebSocketServerPool initialized.");
    }

    // デストラクタ: 全てのサーバーを停止
    ~WebSocketServerPool() {
        // stopAll を呼び出すのでシンプルで良い
        stopAll();
    }

    // コピー禁止 (適切)
    WebSocketServerPool(const WebSocketServerPool&) = delete;
    WebSocketServerPool& operator=(const WebSocketServerPool&) = delete;

    // ムーブ禁止 (適切) - シングルトンや固定インスタンスで使う想定であれば問題なし
    WebSocketServerPool(WebSocketServerPool&&) = delete;
    WebSocketServerPool& operator=(WebSocketServerPool&&) = delete;

    /**
     * @brief 指定されたポートの WebSocketServer インスタンスを確保または作成し、その shared_ptr を返す。
     * サーバーが存在しない場合、または以前のインスタンスが無効になっている場合は、
     * 新しいインスタンスを作成して起動する。
     * @param port サーバーを起動するポート番号。
     * @return サーバーインスタンスへの shared_ptr。起動に失敗した場合は nullptr の可能性がある。
     */
    std::shared_ptr<WebSocketServer> ensureGetServer(int port) {
        std::lock_guard<std::mutex> lock(mutex_); // 書き込みロック
        std::shared_ptr<WebSocketServer> server_ptr = nullptr; // 返すポインタを初期化

        auto it = servers_.find(port);
        bool needs_creation = (it == servers_.end() || it->second.expired());

        if (needs_creation) {
            logger_.info("Creating or recreating WebSocketServer for port {}", port);
            // shared_ptr を作成
            // WebSocketServer のコンストラクタに logger を渡す必要があるか確認
            // server = std::make_shared<WebSocketServer>(port, logger_); // 例
            server_ptr = std::make_shared<WebSocketServer>(port);
            server_ptr->run(); // サーバーを起動
            // 注意: run() が非同期で listen を開始する場合、
            // この関数が返った直後に isListening() が true になっていない可能性がある。
            // 必要であれば、起動完了を待つメカニズムを追加する。
            servers_[port] = server_ptr; // weak_ptr を格納
            logger_.debug("Server instance created and started for port {}", port);
        } else {
            // 既に存在し、weak_ptr も有効な場合
            server_ptr = it->second.lock(); // 既存の weak_ptr から shared_ptr を取得
            if (server_ptr) {
                logger_.debug("WebSocketServer already exists and is active for port {}", port);
            } else {
                // lock() に失敗した場合 (他のスレッドで解放された直後など、非常に稀なケース)
                // 再度作成を試みる (上の needs_creation とほぼ同じロジック)
                logger_.warn("Recreating WebSocketServer for port {} (lock failed unexpectedly)", port);
                server_ptr = std::make_shared<WebSocketServer>(port);
                server_ptr->run();
                servers_[port] = server_ptr;
            }
        }
        // サーバーが正常にリッスンしているか確認するログを追加しても良い
        // if (server_ptr && !server_ptr->isListening()) {
        //     logger_.warn("Server for port {} was ensured but might not be listening yet.", port);
        // }
        return server_ptr; // 作成または取得した shared_ptr を返す
    }

    // 全てのサーバーを安全に停止
    void stopAll() {
        logger_.info("Stopping all WebSocket servers...");
        std::vector<std::shared_ptr<WebSocketServer>> servers_to_stop;
        { // mutex のスコープを限定
            std::lock_guard<std::mutex> lock(mutex_);
            // 停止前にマップが空でないか確認
            if (servers_.empty()) {
                logger_.info("No servers to stop.");
                return;
            }
            servers_to_stop.reserve(servers_.size());
            // イテレータを使って安全にループ処理
            for (auto it = servers_.begin(); it != servers_.end(); /* no increment */) {
                if (auto server = it->second.lock()) {
                    servers_to_stop.push_back(server);
                    ++it;
                } else {
                    // weak_ptr が既に切れているエントリは削除
                    logger_.debug("Removing expired server entry for port {}", it->first);
                    it = servers_.erase(it);
                }
            }
             servers_.clear(); // stop呼び出し前にマップをクリア (ここが良いかは設計次第)
             logger_.debug("Server map cleared.");
        } // mutex 解放

        // mutex の外で各サーバーの停止処理を呼び出す (デッドロック回避)
        logger_.debug("Calling stop() for {} server(s)...", servers_to_stop.size());
        for(auto& server : servers_to_stop) {
            // WebSocketServer に getPort() のようなメソッドがあればポート番号をログに出せる
            // logger_.debug("Stopping server on port {}...", server->getPort());
            server->stop();
        }
        logger_.info("All WebSocket servers stopped.");
    }

private:
    const KaitoTokyo::BridgeUtils::ILogger &logger_; // ロガーへの参照
    std::unordered_map<int, std::weak_ptr<WebSocketServer>> servers_; // サーバーインスタンスの管理
    std::mutex mutex_; // servers_ へのアクセスを保護するミューテックス
};

} // namespace WebSocket
} // namespace KaitoTokyo