/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: TLS 客户端。openssl/libressl 只由包含这个头文件的人引入和链接
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "corasio/detail/text.h"
#include "corasio/http/client.h"
#include "corasio/http/stream.h"

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <openssl/ssl.h>

#include <chrono>
#include <string>
#include <string_view>
#include <utility>


namespace corasio::http {
using tls_socket = asio::ssl::stream<asio::ip::tcp::socket>;

template <>
struct stream_traits<tls_socket> {
    static constexpr std::string_view default_port = "443";

    stream_traits() : context(asio::ssl::context::tls_client) {
        // 最低 TLS 1.2
        context.set_options(asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3 | asio::ssl::context::no_tlsv1 |
                            asio::ssl::context::no_tlsv1_1);
        context.set_verify_mode(asio::ssl::verify_none);

        // 性能相关：会话复用、TLS1.3 ticket、关压缩、写完就放 buffer
        ::SSL_CTX_set_session_cache_mode(context.native_handle(), SSL_SESS_CACHE_CLIENT);
        ::SSL_CTX_set_num_tickets(context.native_handle(), 2);
        ::SSL_CTX_set_mode(context.native_handle(), SSL_MODE_RELEASE_BUFFERS | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
        ::SSL_CTX_set_options(context.native_handle(), SSL_OP_NO_COMPRESSION);
    };

    template <typename Slot>
    void make(asio::any_io_executor executor, Slot& slot) {
        slot.emplace(std::move(executor), context);
    };


    // 让代理建隧道：CONNECT host:port，等到 2xx
    asio::awaitable<bool> connect_tunnel(tls_socket& socket, std::string_view host, std::string_view port, std::chrono::milliseconds timeout) noexcept {
        const std::string authority = std::string(host) + ":" + std::string(port);
        const std::string request = "CONNECT " + authority + " HTTP/1.1\r\nHost: " + authority + "\r\n\r\n";

        const auto written = co_await asio::async_write(socket.next_layer(), asio::buffer(request), asio::cancel_after(timeout, asio::as_tuple));
        if (std::get<0>(written)) co_return false;

        // 一次读一个字节，避免把隧道后面的 TLS 数据吃掉
        std::string head;
        char byte = 0;
        while (head.find("\r\n\r\n") == std::string::npos) {
            const auto result = co_await socket.next_layer().async_read_some(asio::buffer(&byte, 1), asio::cancel_after(timeout, asio::as_tuple));
            if (std::get<0>(result) || std::get<1>(result) == 0) co_return false;

            head.push_back(byte);
            if (head.size() > 1024) co_return false;
        }

        // "HTTP/1.1 200 ..."
        co_return head.size() > 12 && head[9] == '2';
    };

    asio::awaitable<bool> handshake(tls_socket& socket, std::string_view host, std::chrono::milliseconds timeout) noexcept {
        // SNI 要的名字必须活到握手结束，只能复制
        const std::string name(host);
        ::SSL_set_tlsext_host_name(socket.native_handle(), name.c_str());

        const auto result = co_await socket.async_handshake(asio::ssl::stream_base::client, asio::cancel_after(timeout, asio::as_tuple));
        co_return std::get<0>(result) ? false : true;
    };

    asio::ssl::context context;
};

using tls_stream = basic_stream<tls_socket>;

class tls_client : public basic_client<tls_stream> {
public:
    // hostname 形如 "host:port"；hostname_proxy 为空表示直连，非空则先 CONNECT 建隧道
    asio::awaitable<bool> connect(std::string_view hostname_main, std::string_view hostname_proxy = {}) noexcept {
        const auto target = detail::text::split_hostname(hostname_main);
        if (target.first.empty()) co_return false;

        const auto target_port = target.second.empty() ? tls_stream::default_port : target.second;

        if (hostname_proxy.empty()) co_return co_await do_connect(target.first, target_port, {}, {});

        const auto proxy = detail::text::split_hostname(hostname_proxy);
        if (proxy.first.empty()) co_return false;

        const auto proxy_port = proxy.second.empty() ? std::string_view("80") : proxy.second;
        co_return co_await do_connect(proxy.first, proxy_port, target.first, target_port);
    };
};
} // namespace corasio::http

