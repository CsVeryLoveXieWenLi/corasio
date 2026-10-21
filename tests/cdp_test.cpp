/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: CDP（Chrome DevTools Protocol）测试：需要本机 9222 开着远程调试，没开就跳过
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "tests.h"

#include "corasio/http/client.h"
#include "corasio/websocket/client.h"

#include <asio.hpp>

#include <chrono>
#include <cstdio>
#include <exception>
#include <string>
#include <string_view>

using namespace corasio;


namespace {
constexpr const char* cdp_host = "127.0.0.1";
constexpr const char* cdp_port = "9222";
constexpr const char* cdp_version_path = "/json/version";
constexpr const char* cdp_authority = "127.0.0.1:9222";
constexpr std::chrono::milliseconds cdp_timeout{5000};

struct ws_target {
    std::string host;
    std::string port;
    std::string path;
};

// 从 /json/version 的 JSON 里抠出 webSocketDebuggerUrl
std::string find_ws_url(const std::string& body) {
    const std::string marker = "\"webSocketDebuggerUrl\"";
    const auto at = body.find(marker);
    if (at == std::string::npos) return {};

    const auto begin = body.find('"', body.find(':', at));
    if (begin == std::string::npos) return {};

    const auto end = body.find('"', begin + 1);
    if (end == std::string::npos) return {};

    return body.substr(begin + 1, end - begin - 1);
}

// ws://host:port/path
bool split_ws_url(const std::string& url, ws_target& out) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos) return false;

    const auto slash = url.find('/', scheme + 3);
    if (slash == std::string::npos) return false;

    const auto authority = url.substr(scheme + 3, slash - scheme - 3);
    const auto colon = authority.rfind(':');
    if (colon == std::string::npos) return false;

    out.host = authority.substr(0, colon);
    out.port = authority.substr(colon + 1);
    out.path = url.substr(slash);
    return true;
}

// CDP 是 JSON-RPC：按 id 认回包，够测试用
template <typename Client>
asio::awaitable<std::string> read_reply(Client& client, int id) {
    const std::string marker = "\"id\":" + std::to_string(id);

    for (int attempt = 0; attempt < 8; ++attempt) {
        auto incoming = co_await client.read();
        if (!incoming) co_return std::string();

        if (incoming->payload.find(marker) != std::string::npos) co_return std::move(incoming->payload);
    }

    co_return std::string();
}
} // namespace

void test_cdp() {
    std::printf("\n[cdp 9222]\n");

    asio::io_context io;

    struct outcome {
        bool reachable = false;
        std::string url;
        bool connected = false;
        std::string status;
        std::string version_reply;
        std::string targets_reply;
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
            // 1) 先用 http 客户端拿 /json/version
            std::string body;

            {
                http::client client;
                client.set_read_timeout(cdp_timeout);
                client.set_write_timeout(cdp_timeout);

                auto& request = client.get_request();
                request.set_version(http::enums::version::v1_1);
                request.set_method("GET");
                request.set_path(cdp_version_path);
                request.set_header("host", cdp_authority);

                if (!co_await client.connect(cdp_host, cdp_port)) co_return;
                if (!co_await client.send()) co_return;

                body.assign(client.get_response().get_body());
            }

            result.reachable = true;
            result.url = find_ws_url(body);

            ws_target target;
            if (!split_ws_url(result.url, target)) co_return;

            // 2) 连 CDP 的 browser 端点
            websocket::client ws;
            ws.set_read_timeout(cdp_timeout);
            ws.set_write_timeout(cdp_timeout);
            ws.get_request().set_path(target.path);

            result.connected = co_await ws.connect(target.host, target.port);
            result.status.assign(ws.get_response().get_status_code());
            if (!result.connected) co_return;

            // 3) 一条连接上跑两条命令，验证按 id 对应
            if (!co_await ws.send(R"({"id":1,"method":"Browser.getVersion"})")) co_return;
            result.version_reply = co_await read_reply(ws, 1);

            if (!co_await ws.send(R"({"id":2,"method":"Target.getTargets"})")) co_return;
            result.targets_reply = co_await read_reply(ws, 2);

            co_await ws.close();
        },
        report);

    io.run();

    if (!result.reachable) {
        std::printf("    跳过：%s:%s 上没有调试端口（用 --remote-debugging-port=9222 启动 Chrome/Edge）\n", cdp_host, cdp_port);
        return;
    }

    std::printf("    url   =%s\n", result.url.c_str());
    std::printf("    status=%s\n", result.status.c_str());
    std::printf("    version=%s\n", result.version_reply.c_str());
    std::printf("    targets=%s\n", result.targets_reply.substr(0, 160).c_str());

    check(!result.url.empty(), "拿到 webSocketDebuggerUrl");
    check(result.connected, "ws 连上 CDP");
    check(result.status == "101", "CDP 握手回 101");
    check(result.version_reply.find("\"protocolVersion\"") != std::string::npos, "Browser.getVersion 有回包");
    check(result.version_reply.find("Chrome") != std::string::npos, "回包里带浏览器版本");
    check(result.targets_reply.find("\"targetInfos\"") != std::string::npos, "同一连接上 Target.getTargets 也有回包");
}
