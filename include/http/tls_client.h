/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: TLS 客户端。openssl/libressl 只由包含这个头文件的人引入和链接
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "http/client.h"
#include "http/stream.h"

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <openssl/ssl.h>

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

    asio::awaitable<bool> handshake(tls_socket& socket, std::string_view host) noexcept {
        // SNI 要的名字必须活到握手结束，只能复制
        const std::string name(host);
        ::SSL_set_tlsext_host_name(socket.native_handle(), name.c_str());

        const auto result = co_await socket.async_handshake(asio::ssl::stream_base::client, asio::as_tuple);
        co_return std::get<0>(result) ? false : true;
    };

    asio::ssl::context context;
};

using tls_stream = basic_stream<tls_socket>;
using tls_client = basic_client<tls_stream>;
} // namespace corasio::http
