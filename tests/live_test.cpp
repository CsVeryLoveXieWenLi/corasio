/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: 连真实服务器的冒烟测试，没网时自动跳过
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "tests.h"

#include "corasio/websocket/client.h"
#include "corasio/websocket/tls_client.h"

#include <asio.hpp>

#include <chrono>
#include <cstdio>
#include <exception>
#include <string>
#include <string_view>

using namespace corasio;


namespace {
constexpr const char* plain_host = "ws.vi-server.org";
constexpr const char* plain_path = "/mirror";
constexpr const char* secure_host = "echo.websocket.org";
constexpr const char* redirect_host = "echo.websocket.org";
constexpr std::string_view payload = "corasio";
constexpr std::chrono::milliseconds live_timeout{15000};

// 发一条、收到自己发的内容为止（有的服务端会先推 banner）
template <typename Client>
asio::awaitable<std::string> round_trip(Client& client, std::string_view text) {
    if (!co_await client.send(text)) co_return std::string();

    for (int attempt = 0; attempt < 4; ++attempt) {
        auto incoming = co_await client.read();
        if (!incoming) break;

        if (incoming->payload == text) co_return std::move(incoming->payload);
    }

    co_return std::string();
}
} // namespace

void test_websocket_live() {
    std::printf("\n[websocket::client 真实服务器]\n");

    asio::io_context io;

    struct outcome {
        std::string plain_status;  // ws://ws.vi-server.org/mirror
        std::string plain_echo;
        std::string secure_status; // wss://echo.websocket.org
        std::string secure_echo;
        std::string redirect_status; // ws://echo.websocket.org（现在只回 301）
        bool redirect_connected = false;
    } result;

    auto report = [](std::exception_ptr error) {
        if (!error) return;

        try {
            std::rethrow_exception(error);
        } catch (const std::exception& exception) {
            std::printf("    [异常] %s\n", exception.what());
        }
    };

    asio::co_spawn(
        io,
        [&result]() -> asio::awaitable<void> {
            {
                websocket::client client;
                client.set_read_timeout(live_timeout);
                client.set_write_timeout(live_timeout);
                client.get_request().set_path(plain_path);

                if (!co_await client.connect(plain_host, "80")) {
                    result.plain_status.assign(client.get_response().get_status_code());
                    co_return;
                }

                result.plain_status.assign(client.get_response().get_status_code());
                result.plain_echo = co_await round_trip(client, payload);
                co_await client.close();
            }

            {
                websocket::tls_client client;
                client.set_read_timeout(live_timeout);
                client.set_write_timeout(live_timeout);

                if (!co_await client.connect(secure_host, "443")) {
                    result.secure_status.assign(client.get_response().get_status_code());
                    co_return;
                }

                result.secure_status.assign(client.get_response().get_status_code());
                result.secure_echo = co_await round_trip(client, payload);
                co_await client.close();
            }

            {
                // 80 端口现在只回 301 跳 https，握手应当被判失败
                websocket::client client;
                client.set_read_timeout(live_timeout);
                client.set_write_timeout(live_timeout);

                result.redirect_connected = co_await client.connect(redirect_host, "80");
                result.redirect_status.assign(client.get_response().get_status_code());
            }
        },
        report);

    io.run();

    if (result.plain_status.empty() && result.secure_status.empty() && result.redirect_status.empty()) {
        std::printf("    跳过：连不上真实服务器（大概没网）\n");
        return;
    }

    std::printf("    ws  %s%s status=%s echo=\"%s\"\n", plain_host, plain_path, result.plain_status.c_str(), result.plain_echo.c_str());
    std::printf("    wss %s status=%s echo=\"%s\"\n", secure_host, result.secure_status.c_str(), result.secure_echo.c_str());
    std::printf("    ws  %s status=%s\n", redirect_host, result.redirect_status.c_str());

    check(result.plain_status == "101", "ws://ws.vi-server.org/mirror 握手成功");
    check(result.plain_echo == payload, "明文 echo 收到自己发出去的内容");
    check(result.secure_status == "101", "wss://echo.websocket.org 握手成功");
    check(result.secure_echo == payload, "TLS echo 收到自己发出去的内容");
    check(result.redirect_connected == (result.redirect_status == "101"), "只有 101 才算握手成功（80 端口回 301）");
}
