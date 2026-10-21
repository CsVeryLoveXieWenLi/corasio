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

#include "detail/base64.h"
#include "detail/buffer.h"
#include "detail/memory_pool.h"
#include "detail/random.h"
#include "detail/sha1.h"
#include "detail/text.h"
#include "http/enums.h"
#include "http/request_parser.h"
#include "http/server.h"
#include "websocket/enums.h"
#include "websocket/frame.h"

#include "tests.h"

#include <asio.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
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
        websocket::frame::header value;
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
            websocket::frame::header source;
            source.payload_size = length;
            source.op = websocket::enums::opcode::binary;
            source.masked = masked;
            source.mask_key = 0x3D21FA37;

            const auto written = websocket::frame::write(source, bytes);

            websocket::frame::header parsed;
            const auto read = websocket::frame::parse(std::string_view(bytes, written), parsed);

            if (read != written || parsed.payload_size != length || parsed.masked != masked || parsed.op != websocket::enums::opcode::binary ||
                !parsed.fin || (masked && parsed.mask_key != 0x3D21FA37)) {
                round_trip = false;
            }
        }
    }
    check(round_trip, "帧头编解码往返（各种长度 × 掩码）");

    auto is_invalid = [](std::string_view data) {
        websocket::frame::header parsed;
        return websocket::frame::parse(data, parsed) == -1;
    };

    websocket::frame::header partial;
    check(websocket::frame::parse(std::string_view("\x81", 1), partial) == 0, "半截帧头返回 0（还要更多数据）");
    check(is_invalid(std::string_view("\xC1\x00", 2)), "rsv 非 0 非法");
    check(is_invalid(std::string_view("\x83\x00", 2)), "保留 opcode 非法");
    check(is_invalid(std::string_view("\x09\x00", 2)), "控制帧不能分片");
    check(is_invalid(std::string_view("\x89\x7E\x01\x00", 4)), "控制帧 payload 不能超 125");

    // RFC 6455 §5.7 的掩码例子：key = 37 fa 21 3d，payload 是 "Hello"
    const std::string rfc = std::string("\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58", 11);

    websocket::frame::header parsed;
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
    test_text();
    test_request_parser();
    test_server();
    test_client();
    test_tls_client();
    test_proxy();
    test_tls_proxy();

    std::printf("\n%s (failures=%d)\n", failures == 0 ? "ALL PASS" : "FAILED", failures);
    std::fflush(stdout);

    // 服务器线程是 detached 的，直接退出进程，别让静态析构和它抢
    std::_Exit(failures == 0 ? 0 : 1);
}
