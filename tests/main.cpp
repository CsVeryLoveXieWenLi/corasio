/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: 测试入口。所有测试都放 tests/ 下，这里保持唯一的 main()
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

#include "corasio/detail/base64.h"
#include "corasio/detail/buffer.h"
#include "corasio/detail/memory_pool.h"
#include "corasio/detail/random.h"
#include "corasio/detail/sha1.h"
#include "corasio/detail/text.h"
#include "corasio/http/enums.h"
#include "corasio/http/request_parser.h"
#include "corasio/http/response_parser.h"
#include "corasio/http/server.h"
#include "corasio/websocket/enums.h"
#include "corasio/websocket/frame.h"
#include "corasio/websocket/client.h"
#include "corasio/websocket/tls_client.h"
#include "corasio/websocket/handshake.h"
#include "corasio/websocket/session.h"

#include <llhttp.h>
#include <memory>

#include "tests.h"

#include <asio.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <tuple>

using namespace corasio;

int failures = 0;

void check(bool cond, const char* what) {
    if (cond) {
        std::printf("  ok   %s\n", what);
        return;
    }
    ++failures;
    std::printf("  FAIL %s\n", what);
}

namespace {
constexpr const char* test_host = "127.0.0.1";
constexpr const char* test_port = "18711";

// 读一条完整响应（头部 + content-length 指定的 body）
bool read_response(asio::ip::tcp::socket& socket, asio::streambuf& buf, std::string& head, std::string& body, asio::error_code& ec) {
    const std::size_t head_len = asio::read_until(socket, buf, "\r\n\r\n", ec);
    if (ec) return false;

    head.assign(asio::buffers_begin(buf.data()), asio::buffers_begin(buf.data()) + head_len);
    std::size_t content_length = 0;
    const auto pos = head.find("content-length:");
    if (pos != std::string::npos) content_length = std::strtoul(head.c_str() + pos + 15, nullptr, 10);
    buf.consume(head_len);

    while (buf.size() < content_length) {
        buf.commit(socket.read_some(buf.prepare(content_length - buf.size()), ec));
        if (ec) return false;
    }

    body.assign(asio::buffers_begin(buf.data()), asio::buffers_begin(buf.data()) + content_length);
    buf.consume(content_length);
    return true;
}

void test_memory_pool() {
    std::printf("[memory_pool]\n");

    char* small = detail::memory_pool::get(1000);
    check(small != nullptr, "get(1000)");
    detail::memory_pool::put(small, 1000);
    check(detail::memory_pool::get(1000) == small, "同大小复用同一块");

    char* big = detail::memory_pool::get(2000);
    check(big != small, "不同大小分桶");
    detail::memory_pool::put(big, 2000);

    char* recycled = nullptr;
    {
        detail::memory_pool::block block(4096);
        recycled = block.data();
        check(recycled != nullptr && block.size() == 4096, "block 从池子里取块");
    }
    detail::memory_pool::block again(4096);
    check(again.data() == recycled, "block 析构自动归还");
}

void test_buffer() {
    std::printf("\n[buffer]\n");

    detail::buffer buf(1024);
    check(buf.readable_size() == 0 && buf.write_size() == 1024, "初始状态");

    // 粘包：一次塞两条请求
    const std::string two = "GET /a HTTP/1.1\r\n\r\nGET /b HTTP/1.1\r\n\r\n";
    const std::size_t first = std::strlen("GET /a HTTP/1.1\r\n\r\n");
    std::memcpy(buf.write_pos(), two.data(), two.size());
    buf.commit(two.size());
    check(buf.readable_size() == two.size(), "commit 后可读");

    buf.consume(first);
    check(std::string(buf.readable()) == "GET /b HTTP/1.1\r\n\r\n", "consume 后剩余搬到开头");
    check(buf.write_pos() == buf.readable().data() + buf.readable_size(), "写位置紧跟有效数据");

    buf.consume(std::strlen("GET /b HTTP/1.1\r\n\r\n"));
    check(buf.readable_size() == 0 && buf.write_size() == 1024, "全消费后整块可复用");

    // 拆包：advance 只记账，不搬数据（解析器可能还指着它们）
    const std::string head = "GET /a HTTP/1.1\r\n";
    std::memcpy(buf.write_pos(), head.data(), head.size());
    buf.commit(head.size());
    buf.advance(head.size());
    check(buf.readable_size() == 0, "advance 后可读清零");
    check(buf.write_size() == 1024 - head.size(), "advance 不搬数据");
}

void test_request_parser() {
    std::printf("\n[request_parser]\n");

    const std::string data = "GET /a?x=1 HTTP/1.1\r\nHost: h\r\n\r\nGET /b HTTP/1.1\r\nHost: h\r\n\r\n";
    detail::buffer buf(data.size());
    std::memcpy(buf.write_pos(), data.data(), data.size());
    buf.commit(data.size());

    http::request_parser parser;
    std::string paths;
    int count = 0;

    while (!buf.readable().empty()) {
        const auto view = buf.readable();
        if (parser.execute(view) != HPE_PAUSED) break;

        const std::size_t total = static_cast<std::size_t>(parser.pos() - view.data());
        paths += std::string(parser.get().get_path()) + ";";
        ++count;

        parser.reset();
        buf.consume(total);
    }

    check(count == 2, "粘包切出两条请求");
    check(paths == "/a;/b;", "顺序正确");
    check(buf.readable_size() == 0, "缓冲区清空");

    // keep_alive
    {
        std::string raw = "GET /k HTTP/1.1\r\nConnection: close\r\n\r\n";
        http::request_parser p;
        p.execute(raw);
        check(!p.get().get_keep_alive(), "Connection: close -> keep_alive = false");
        p.reset();
        std::string raw2 = "GET /k2 HTTP/1.1\r\nHost: h\r\n\r\n";
        p.execute(raw2);
        check(p.get().get_keep_alive(), "reset 后不残留，默认 keep-alive = true");
    }
}

void test_digest() {
    std::printf("\n[sha1 / base64]\n");

    auto to_hex = [](const auto& value) {
        static constexpr char digits[] = "0123456789abcdef";

        std::string text;
        for (auto byte : value) {
            text.push_back(digits[byte >> 4]);
            text.push_back(digits[byte & 0x0F]);
        }
        return text;
    };

    auto to_base64 = [](std::string_view input) {
        std::string text;
        detail::base64::encode(input, text);
        return text;
    };

    check(to_hex(detail::sha1::hash("")) == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "sha1 空串");
    check(to_hex(detail::sha1::hash("abc")) == "a9993e364706816aba3e25717850c26c9cd0d89d", "sha1(\"abc\")");
    check(to_hex(detail::sha1::hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
              "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
          "sha1 跨 64 字节块 + padding");

    const std::string million(1000 * 1000, 'a');
    check(to_hex(detail::sha1::hash(million)) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f", "sha1 一百万个 a（多块循环）");

    check(to_base64("").empty(), "base64 空串");
    check(to_base64("f") == "Zg==", "base64 补两个 =");
    check(to_base64("fo") == "Zm8=", "base64 补一个 =");
    check(to_base64("foo") == "Zm9v", "base64 刚好 3 字节");
    check(to_base64("foobar") == "Zm9vYmFy", "base64 两组的倍数");

    const std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
    const std::string source = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    const auto accepted = detail::sha1::hash(source);
    const auto raw = std::string_view(reinterpret_cast<const char*>(accepted.data()), accepted.size());
    check(to_base64(raw) == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", "RFC 6455 的 Sec-WebSocket-Accept");
}

void test_text() {
    std::printf("\n[text]\n");
    using detail::text::split_hostname;

    auto [host, port] = split_hostname("example.com:8080");
    check(host == "example.com" && port == "8080", "host:port 拆分");

    std::tie(host, port) = split_hostname("example.com");
    check(host == "example.com" && port.empty(), "没有端口时不拆");

    std::tie(host, port) = split_hostname("[::1]:443");
    check(host == "::1" && port == "443", "[IPv6]:port 拆分");

    std::tie(host, port) = split_hostname("::1");
    check(host == "::1" && port.empty(), "裸 IPv6 整段当 host");

    char buffer[] = "Host-Name";
    detail::text::to_lower(buffer, sizeof(buffer) - 1);
    check(std::string(buffer) == "host-name", "to_lower 就地小写");
}

void test_frame() {
    std::printf("\n[websocket::frame]\n");

    char bytes[14] = {};

    auto header_size = [&bytes](std::uint64_t length, bool masked) {
        websocket::header value;
        value.payload_size = length;
        value.masked = masked;
        value.mask_key = 0x3D21FA37;
        return websocket::frame::write(value, bytes);
    };

    check(header_size(125, false) == 2, "长度 <= 125 用 7 bit");
    check(header_size(126, false) == 4 && header_size(65535, false) == 4, "长度 126..65535 用 16 bit");
    check(header_size(65536, false) == 10, "长度 > 65535 用 64 bit");
    check(header_size(125, true) == 6 && header_size(65536, true) == 14, "掩码 key 跟在长度后面");

    bool round_trip = true;
    for (const auto length : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{125}, std::uint64_t{126}, std::uint64_t{4096},
                              std::uint64_t{65535}, std::uint64_t{65536}}) {
        for (const bool masked : {false, true}) {
            websocket::header source;
            source.payload_size = length;
            source.op = websocket::enums::opcode::binary;
            source.masked = masked;
            source.mask_key = 0x3D21FA37;

            const auto written = websocket::frame::write(source, bytes);

            websocket::header parsed;
            const auto read = websocket::frame::parse(std::string_view(bytes, written), parsed);

            if (read != written || parsed.payload_size != length || parsed.masked != masked || parsed.op != websocket::enums::opcode::binary ||
                !parsed.fin || (masked && parsed.mask_key != 0x3D21FA37)) {
                round_trip = false;
            }
        }
    }
    check(round_trip, "帧头编解码往返（各种长度 × 掩码）");

    auto is_invalid = [](std::string_view data) {
        websocket::header parsed;
        return websocket::frame::parse(data, parsed) == -1;
    };

    websocket::header partial;
    check(websocket::frame::parse(std::string_view("\x81", 1), partial) == 0, "半截帧头返回 0（还要更多数据）");
    check(is_invalid(std::string_view("\xC1\x00", 2)), "rsv 非 0 非法");
    check(is_invalid(std::string_view("\x83\x00", 2)), "保留 opcode 非法");
    check(is_invalid(std::string_view("\x09\x00", 2)), "控制帧不能分片");
    check(is_invalid(std::string_view("\x89\x7E\x01\x00", 4)), "控制帧 payload 不能超 125");

    // RFC 6455 §5.7 的掩码例子：key = 37 fa 21 3d，payload 是 "Hello"
    const std::string rfc = std::string("\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58", 11);

    websocket::header parsed;
    check(websocket::frame::parse(rfc, parsed) == 6, "RFC 掩码帧的帧头 6 字节");
    check(parsed.masked && parsed.op == websocket::enums::opcode::text && parsed.payload_size == 5, "RFC 掩码帧的字段");

    std::string payload = rfc.substr(6);
    websocket::frame::mask(payload.data(), payload.size(), parsed.mask_key, 0);
    check(payload == "Hello", "解掩码得到 Hello");

    // 和标量实现逐字节对齐，同时验证可逆
    auto reference = [](std::string& data, std::uint32_t key, std::size_t offset) {
        for (std::size_t index = 0; index < data.size(); ++index) {
            const auto position = offset + index;
            data[index] = static_cast<char>(data[index] ^ static_cast<char>((key >> ((position & 3) * 8)) & 0xFF));
        }
    };

    bool mask_matches = true;
    for (std::size_t size = 0; size <= 40 && mask_matches; ++size) {
        for (std::size_t offset = 0; offset < 4 && mask_matches; ++offset) {
            std::string original(size, '\0');
            for (std::size_t index = 0; index < size; ++index) original[index] = static_cast<char>(index * 7 + 1);

            auto expected = original;
            reference(expected, 0x3D21FA37, offset);

            auto actual = original;
            websocket::frame::mask(actual.data(), actual.size(), 0x3D21FA37, offset);
            if (actual != expected) mask_matches = false;

            websocket::frame::mask(actual.data(), actual.size(), 0x3D21FA37, offset);
            if (actual != original) mask_matches = false;
        }
    }
    check(mask_matches, "SSE 掩码与标量参考一致、可逆（size 0..40 × offset 0..3）");

    std::string whole(50, '\0');
    for (std::size_t index = 0; index < whole.size(); ++index) whole[index] = static_cast<char>(index * 11 + 3);

    auto one_shot = whole;
    websocket::frame::mask(one_shot.data(), one_shot.size(), 0x3D21FA37, 0);

    auto split = whole;
    websocket::frame::mask(split.data(), 7, 0x3D21FA37, 0);
    websocket::frame::mask(split.data() + 7, split.size() - 7, 0x3D21FA37, 7);
    check(one_shot == split, "分段掩码和一次掩码结果相同（offset 接得上）");

    unsigned char first[5] = {};
    unsigned char second[5] = {};
    detail::random::fill(first, sizeof(first));
    detail::random::fill(second, sizeof(second));
    check(std::memcmp(first, second, sizeof(first)) != 0, "随机字节两次不一样");
}

void test_handshake() {
    std::printf("\n[websocket::handshake]\n");

    using websocket::handshake::accept_for;

    check(accept_for("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", "RFC 6455 的 accept");

    const auto key = websocket::handshake::make_key();
    check(key.size() == 24 && key.substr(22) == "==", "make_key 是 16 字节 base64");
    check(key != websocket::handshake::make_key(), "两次 make_key 不一样");

    http::request request;
    request.set_method("GET");
    request.set_path("/chat");
    request.set_header("upgrade", "websocket");
    request.set_header("connection", "keep-alive, Upgrade");
    request.set_header("sec-websocket-version", "13");
    request.set_header("sec-websocket-key", key);
    check(websocket::handshake::is_upgrade(request), "标准升级请求");

    request.set_header("sec-websocket-version", "8");
    check(!websocket::handshake::is_upgrade(request), "版本不是 13 拒绝");
    request.set_header("sec-websocket-version", "13");

    request.set_header("connection", "keep-alive");
    check(!websocket::handshake::is_upgrade(request), "没有 connection: upgrade 拒绝");
    request.set_header("connection", "Upgrade");

    request.set_header("upgrade", "h2c");
    check(!websocket::handshake::is_upgrade(request), "upgrade 不是 websocket 拒绝");
    request.set_header("upgrade", "WebSocket");

    http::response response;
    response.set_status_code("101");
    response.set_header("sec-websocket-accept", accept_for(key));
    check(websocket::handshake::is_accepted(response, key), "101 + accept 正确");

    // set_header 只存 view，值必须自己留着
    const std::string wrong = accept_for("别的 key");
    response.set_header("sec-websocket-accept", wrong);
    check(!websocket::handshake::is_accepted(response, key), "accept 不对拒绝");

    const std::string right = accept_for(key);
    response.set_header("sec-websocket-accept", right);
    response.set_status_code("200");
    check(!websocket::handshake::is_accepted(response, key), "状态码不是 101 拒绝");
}
namespace {
// 一条本地连接：一端给 session，另一端当客户端手工收发
struct ws_sockets {
    asio::ip::tcp::socket server;
    asio::ip::tcp::socket peer;
};

ws_sockets make_ws_sockets(asio::io_context& io) {
    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

    asio::ip::tcp::socket peer(io);
    peer.connect(acceptor.local_endpoint());

    asio::ip::tcp::socket server(io);
    acceptor.accept(server);

    return ws_sockets{std::move(server), std::move(peer)};
}

asio::awaitable<void> ws_peer_send(asio::ip::tcp::socket& socket, std::string_view payload, websocket::enums::opcode op, bool fin, bool masked) {
    websocket::header header;
    header.payload_size = payload.size();
    header.op = op;
    header.fin = fin;
    header.masked = masked;
    header.mask_key = 0x3D21FA37;

    std::string body(payload);
    if (masked) websocket::frame::mask(body.data(), body.size(), header.mask_key, 0);

    char head[14] = {};
    const auto head_size = websocket::frame::write(header, head);

    std::vector<asio::const_buffer> buffers;
    buffers.push_back(asio::buffer(head, head_size));
    buffers.push_back(asio::buffer(body));

    co_await asio::async_write(socket, buffers, asio::use_awaitable);
}

asio::awaitable<std::string> ws_peer_read(asio::ip::tcp::socket& socket, bool& masked) {
    std::string raw;
    char chunk[1024];

    while (true) {
        websocket::header header;
        const auto size = websocket::frame::parse(raw, header);

        if (size > 0 && raw.size() >= static_cast<std::size_t>(size) + header.payload_size) {
            std::string payload = raw.substr(static_cast<std::size_t>(size), header.payload_size);
            masked = header.masked;
            if (header.masked) websocket::frame::mask(payload.data(), payload.size(), header.mask_key, 0);
            co_return payload;
        }

        if (size == -1) co_return std::string();

        const auto got = co_await socket.async_read_some(asio::buffer(chunk), asio::as_tuple);
        if (std::get<0>(got) || std::get<1>(got) == 0) co_return std::string();

        raw.append(chunk, std::get<1>(got));
    }
}

asio::awaitable<void> ws_case_text(asio::io_context& io) {
    auto sockets = make_ws_sockets(io);
    detail::buffer read_buffer(4096);
    websocket::session session(std::move(sockets.server), std::move(read_buffer));

    co_await ws_peer_send(sockets.peer, "hello", websocket::enums::opcode::text, true, true);

    const auto result = co_await session.read();
    check(result.has_value() && result->op == websocket::enums::opcode::text && result->payload == "hello", "读一条完整文本消息");
}

asio::awaitable<void> ws_case_fragments(asio::io_context& io) {
    auto sockets = make_ws_sockets(io);
    detail::buffer read_buffer(4096);
    websocket::session session(std::move(sockets.server), std::move(read_buffer));

    co_await ws_peer_send(sockets.peer, "he", websocket::enums::opcode::text, false, true);
    co_await ws_peer_send(sockets.peer, "ll", websocket::enums::opcode::continuation, false, true);
    co_await ws_peer_send(sockets.peer, "o", websocket::enums::opcode::continuation, true, true);

    const auto result = co_await session.read();
    check(result.has_value() && result->payload == "hello", "三个分片拼成一条消息");
}

asio::awaitable<void> ws_case_ping(asio::io_context& io) {
    auto sockets = make_ws_sockets(io);
    detail::buffer read_buffer(4096);
    websocket::session session(std::move(sockets.server), std::move(read_buffer));

    co_await ws_peer_send(sockets.peer, "p", websocket::enums::opcode::ping, true, true);
    co_await ws_peer_send(sockets.peer, "x", websocket::enums::opcode::text, true, true);

    const auto result = co_await session.read();
    check(result.has_value() && result->payload == "x", "ping 内部消化，继续读到下一条");

    bool masked = true;
    const auto pong = co_await ws_peer_read(sockets.peer, masked);
    check(pong == "p" && !masked, "自动回 pong（不带掩码）");
}

asio::awaitable<void> ws_case_send(asio::io_context& io) {
    auto sockets = make_ws_sockets(io);
    detail::buffer read_buffer(4096);
    websocket::session session(std::move(sockets.server), std::move(read_buffer));

    check(co_await session.send("hi"), "send 成功");

    bool masked = true;
    const auto payload = co_await ws_peer_read(sockets.peer, masked);
    check(payload == "hi" && !masked, "服务端发的帧不带掩码");
}

asio::awaitable<void> ws_case_unmasked(asio::io_context& io) {
    auto sockets = make_ws_sockets(io);
    detail::buffer read_buffer(4096);
    websocket::session session(std::move(sockets.server), std::move(read_buffer));

    co_await ws_peer_send(sockets.peer, "oops", websocket::enums::opcode::text, true, false);

    const auto result = co_await session.read();
    check(!result.has_value(), "客户端帧没掩码就断开");
}

asio::awaitable<void> ws_case_close(asio::io_context& io) {
    auto sockets = make_ws_sockets(io);
    detail::buffer read_buffer(4096);
    websocket::session session(std::move(sockets.server), std::move(read_buffer));

    const char code[2] = {0x03, static_cast<char>(0xE8)};
    co_await ws_peer_send(sockets.peer, std::string_view(code, sizeof(code)), websocket::enums::opcode::close, true, true);

    const auto result = co_await session.read();
    check(result.has_value() && result->op == websocket::enums::opcode::close, "读到 close");

    bool masked = true;
    const auto echoed = co_await ws_peer_read(sockets.peer, masked);
    check(echoed == std::string(code, sizeof(code)), "回了一个同样的 close");
}
} // namespace

void test_websocket_session() {
    std::printf("\n[websocket::session]\n");

    asio::io_context io;

    // detached 会吞异常，这里打出来
    auto report = [](std::exception_ptr error) {
        if (!error) return;

        try {
            std::rethrow_exception(error);
        } catch (const std::exception& exception) {
            std::printf("    [异常] %s\n", exception.what());
        }
    };

    asio::co_spawn(io, ws_case_text(io), report);
    asio::co_spawn(io, ws_case_fragments(io), report);
    asio::co_spawn(io, ws_case_ping(io), report);
    asio::co_spawn(io, ws_case_send(io), report);
    asio::co_spawn(io, ws_case_unmasked(io), report);
    asio::co_spawn(io, ws_case_close(io), report);

    io.run();
}
void test_upgrade() {
    std::printf("\n[http::session::upgrade]\n");

    auto server = std::make_shared<http::server>();
    server->set_read_timeout(std::chrono::milliseconds(3000));
    server->set_write_timeout(std::chrono::milliseconds(3000));
    server->set_handler([](http::request& request, http::response& response, http::session& session) -> asio::awaitable<void> {
        auto socket = co_await session.upgrade(request);
        if (!socket) {
            response.set_version(http::enums::version::v1_1);
            response.set_status_code(http::enums::status_code::bad_request);
            response.set_status_message(http::enums::status_message::bad_request);
            response.set_header("content-length", "0");
            co_await session.send(request, response);
            co_return;
        }

        co_await socket->send("welcome");
    });

    std::thread([server] { server->run("127.0.0.1", "18721", 1); }).detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    asio::io_context io;

    // 一个字节一个字节读，别把升级后的帧一起吞掉
    auto read_head = [](asio::ip::tcp::socket& socket) {
        std::string head;
        char byte = 0;

        while (head.find("\r\n\r\n") == std::string::npos) {
            const auto got = socket.read_some(asio::buffer(&byte, 1));
            if (got == 0) break;
            head.push_back(byte);
        }

        return head;
    };

    {
        asio::ip::tcp::socket socket(io);
        socket.connect({asio::ip::make_address("127.0.0.1"), 18721});

        const std::string key = websocket::handshake::make_key();
        const std::string request = "GET /chat HTTP/1.1\r\nhost: 127.0.0.1:18721\r\nupgrade: websocket\r\nconnection: Upgrade\r\n"
                                    "sec-websocket-key: " + key + "\r\nsec-websocket-version: 13\r\n\r\n";
        asio::write(socket, asio::buffer(request));

        const auto head = read_head(socket);

        http::response_parser parser;
        check(parser.execute(head) == HPE_PAUSED, "101 响应能解析完整");
        check(websocket::handshake::is_accepted(parser.get(), key), "101 + accept 正确");

        std::string raw;
        char chunk[64];
        websocket::header header;

        while (true) {
            const auto size = websocket::frame::parse(raw, header);
            if (size > 0 && raw.size() >= static_cast<std::size_t>(size) + header.payload_size) break;

            const auto got = socket.read_some(asio::buffer(chunk));
            if (got == 0) break;
            raw.append(chunk, got);
        }

        const auto size = static_cast<std::size_t>(websocket::frame::parse(raw, header));
        check(header.op == websocket::enums::opcode::text && !header.masked, "升级后第一帧是文本、不带掩码");
        check(raw.substr(size, header.payload_size) == "welcome", "收到 welcome");
    }

    {
        asio::ip::tcp::socket socket(io);
        socket.connect({asio::ip::make_address("127.0.0.1"), 18721});

        asio::write(socket, asio::buffer(std::string("GET / HTTP/1.1\r\nhost: h\r\n\r\n")));
        check(read_head(socket).rfind("HTTP/1.1 400", 0) == 0, "不是升级请求就回 400");
    }
}
void test_websocket_client() {
    std::printf("\n[websocket::client]\n");

    auto server = std::make_shared<http::server>();
    server->set_read_timeout(std::chrono::milliseconds(3000));
    server->set_write_timeout(std::chrono::milliseconds(3000));
    server->set_handler([](http::request& request, http::response& response, http::session& session) -> asio::awaitable<void> {
        auto socket = co_await session.upgrade(request);
        if (!socket) co_return;

        while (auto incoming = co_await socket->read()) {
            if (incoming->op == websocket::enums::opcode::close) co_return;

            const std::string reply = "echo:" + incoming->payload;
            if (!co_await socket->send(reply)) co_return;
        }
    });

    std::thread([server] { server->run("127.0.0.1", "18722", 1); }).detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    asio::io_context io;

    struct outcome {
        bool connected = false;
        bool sent = false;
        bool received = false;
        std::string status;
        std::string payload;
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
            websocket::client client;
            client.set_read_timeout(std::chrono::milliseconds(3000));
            client.set_write_timeout(std::chrono::milliseconds(3000));
            client.get_request().set_path("/echo");

            result.connected = co_await client.connect("127.0.0.1:18722");
            if (!result.connected) co_return;

            result.status.assign(client.get_response().get_status_code());

            result.sent = co_await client.send("hi");

            if (auto incoming = co_await client.read()) {
                result.received = true;
                result.payload = std::move(incoming->payload);
            }

            co_await client.close();
        },
        report);

    // tls_client：没起服务端，连接应当干净失败（顺带确认 TLS 版能实例化、能跑）
    bool tls_failed = false;
    asio::co_spawn(
        io,
        [&tls_failed]() -> asio::awaitable<void> {
            websocket::tls_client client;
            tls_failed = !co_await client.connect("127.0.0.1:18799");
        },
        report);

    io.run();

    std::printf("    status=%s payload=\"%s\"\n", result.status.c_str(), result.payload.c_str());
    check(tls_failed, "tls_client 连不上时干净返回 false");
    check(result.connected, "client 握手成功");
    check(result.status == "101", "握手响应是 101");
    check(result.sent, "client send 成功");
    check(result.received && result.payload == "echo:hi", "收到服务端回显");
}
void test_server() {
    std::printf("\n[server]\n");

    asio::io_context io;
    http::server srv;
    srv.set_buffer_size(8 * 1024);
    srv.set_read_timeout(std::chrono::milliseconds(3000));
    srv.set_write_timeout(std::chrono::milliseconds(3000));
    srv.set_handler([](http::request& req, http::response& res, http::session& sess) -> asio::awaitable<void> {
        const std::string body = "method=" + std::string(req.get_method()) + " path=" + std::string(req.get_path());
        const std::string content_length = std::to_string(body.size());

        res.set_version(http::enums::version::v1_1);
        res.set_status_code(http::enums::status_code::ok);
        res.set_status_message(http::enums::status_message::ok);
        res.set_header("content-length", content_length);
        res.set_body(body);

        co_await sess.send(req, res);
    });

    std::thread([&srv] { srv.run(test_host, test_port, 2); }).detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    asio::error_code ec;

    std::printf("  普通请求\n");
    {
        asio::ip::tcp::socket socket(io);
        socket.connect({asio::ip::make_address(test_host), static_cast<asio::ip::port_type>(std::atoi(test_port))}, ec);
        asio::write(socket, asio::buffer(std::string("GET /hello?x=1 HTTP/1.1\r\nHost: h\r\n\r\n")), ec);

        asio::streambuf buf;
        std::string head;
        std::string body;
        check(read_response(socket, buf, head, body, ec), "读到响应");
        check(head.rfind("HTTP/1.1 200 OK", 0) == 0, "状态行 200");
        check(body == "method=GET path=/hello", "body 正确");
    }

    std::printf("  粘包 + keep-alive\n");
    {
        asio::ip::tcp::socket socket(io);
        socket.connect({asio::ip::make_address(test_host), static_cast<asio::ip::port_type>(std::atoi(test_port))}, ec);
        asio::streambuf buf;
        asio::write(socket, asio::buffer(std::string("GET /a HTTP/1.1\r\nHost: h\r\n\r\nGET /b HTTP/1.1\r\nHost: h\r\n\r\n")), ec);

        std::string head;
        std::string body;
        check(read_response(socket, buf, head, body, ec) && body == "method=GET path=/a", "粘包第一个 /a");
        check(read_response(socket, buf, head, body, ec) && body == "method=GET path=/b", "粘包第二个 /b");

        asio::write(socket, asio::buffer(std::string("GET /c HTTP/1.1\r\nHost: h\r\n\r\n")), ec);
        check(read_response(socket, buf, head, body, ec) && body == "method=GET path=/c", "同一连接第三个 /c");
    }

    std::printf("  拆包\n");
    {
        asio::ip::tcp::socket socket(io);
        socket.connect({asio::ip::make_address(test_host), static_cast<asio::ip::port_type>(std::atoi(test_port))}, ec);
        asio::streambuf buf;
        asio::write(socket, asio::buffer(std::string("GET /spl")));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        asio::write(socket, asio::buffer(std::string("it HTTP/1.1\r\nHost:")));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        asio::write(socket, asio::buffer(std::string(" h\r\n\r\n")));

        std::string head;
        std::string body;
        check(read_response(socket, buf, head, body, ec) && body == "method=GET path=/split", "跨三次写入的请求");
    }

    std::printf("  HEAD 不带 body\n");
    {
        asio::ip::tcp::socket socket(io);
        socket.connect({asio::ip::make_address(test_host), static_cast<asio::ip::port_type>(std::atoi(test_port))}, ec);
        asio::write(socket, asio::buffer(std::string("HEAD /head HTTP/1.1\r\nHost: h\r\n\r\n")), ec);

        asio::streambuf buf;
        asio::read_until(socket, buf, "\r\n\r\n", ec);
        std::string head(asio::buffers_begin(buf.data()), asio::buffers_begin(buf.data()) + buf.size());
        check(!ec && head.rfind("HTTP/1.1 200 OK", 0) == 0, "读到头部");
        check(head.find("content-length: ") != std::string::npos, "保留 content-length");
        buf.consume(buf.size());

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        char extra[32] = {};
        asio::error_code read_ec;
        const auto got = socket.read_some(asio::buffer(extra), read_ec);
        check(read_ec || got == 0, "没有多发 body");
    }

    std::printf("  非法报文直接断开\n");
    {
        asio::ip::tcp::socket socket(io);
        socket.connect({asio::ip::make_address(test_host), static_cast<asio::ip::port_type>(std::atoi(test_port))}, ec);
        asio::write(socket, asio::buffer(std::string("GET /x HTTP/1.1\r\nThis Is Not A Header\r\n\r\n")), ec);

        asio::streambuf buf;
        asio::error_code read_ec;
        asio::read_until(socket, buf, "\r\n\r\n", read_ec);
        check(read_ec == asio::error::eof, "连接被关闭");
    }

    std::printf("  多条连接（验证轮询分配）\n");
    {
        int good = 0;
        for (int i = 0; i < 8; ++i) {
            asio::ip::tcp::socket socket(io);
            socket.connect({asio::ip::make_address(test_host), static_cast<asio::ip::port_type>(std::atoi(test_port))}, ec);
            asio::streambuf buf;
            const std::string index = std::to_string(i);
            asio::write(socket, asio::buffer("GET /p" + index + " HTTP/1.1\r\nHost: h\r\n\r\nGET /q" + index + " HTTP/1.1\r\nHost: h\r\n\r\n"), ec);

            std::string head;
            std::string body;
            if (read_response(socket, buf, head, body, ec) && body == "method=GET path=/p" + index &&
                read_response(socket, buf, head, body, ec) && body == "method=GET path=/q" + index) {
                ++good;
            }
        }
        std::printf("    %d/8 条连接正常\n", good);
        check(good == 8, "8 条连接全部正确");
    }
}
} // namespace

int main() {
    // 源码是 UTF-8，Windows 控制台默认 CP936，不切的话中文全是乱码
#ifdef _WIN32
    const UINT previous_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    std::printf("[console] output code page: %u -> %u\n", previous_cp, GetConsoleOutputCP());
#endif

    setvbuf(stdout, nullptr, _IONBF, 0);

    test_memory_pool();
    test_buffer();
    test_digest();
    test_frame();
    test_handshake();
    test_websocket_session();
    test_upgrade();
    test_websocket_client();
    test_websocket_live();
    test_cdp();
    test_text();
    test_request_parser();
    test_server();
    test_client();
    test_client_reconnect();
    test_tls_client();
    test_proxy();
    test_tls_proxy();

    std::printf("\n%s (failures=%d)\n", failures == 0 ? "ALL PASS" : "FAILED", failures);
    std::fflush(stdout);

    // 服务器线程是 detached 的，直接退出进程，别让静态析构和它抢
    std::_Exit(failures == 0 ? 0 : 1);
}
