/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "corasio/detail/socket.h"

#include <cstdint>

#if defined(_WIN32)
#  include <winsock2.h>
#else
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif


namespace corasio::detail::socket {
namespace {
#if defined(_WIN32)
using handle_type = SOCKET;
using length_type = int;
#else
using handle_type = int;
using length_type = std::size_t;
#endif

// 非阻塞地看一眼有没有数据可读：select 零超时，不改阻塞模式
bool is_readable(handle_type handle) noexcept {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(handle, &readable);

    timeval zero{};
    zero.tv_sec = 0;
    zero.tv_usec = 0;

#if defined(_WIN32)
    return ::select(0, &readable, nullptr, nullptr, &zero) > 0;
#else
    return ::select(handle + 1, &readable, nullptr, nullptr, &zero) > 0;
#endif
}

// 只窥一个字节，不消费；返回 -1 表示出错
int peek(handle_type handle) noexcept {
    char byte = 0;

#if defined(_WIN32)
    return ::recv(handle, &byte, 1, MSG_PEEK);
#else
    return static_cast<int>(::recv(handle, &byte, 1, MSG_PEEK | MSG_DONTWAIT));
#endif
}
} // namespace

bool is_peer_closed(std::intptr_t handle) noexcept {
    if (handle <= 0) return true;

    const auto socket = static_cast<handle_type>(handle);

    // 不可读：连接还活着，只是没数据（健康空闲连接的常态，只花一次 select）
    if (!is_readable(socket)) return false;

    // 可读：要么有数据，要么是 EOF，peek 一次就能分辨，且不消费
    const auto got = peek(socket);
    if (got > 0) return false; // 有数据在等着
    if (got == 0) return true; // 读到 0 = 对端已经关了

#if defined(_WIN32)
    const auto error = ::WSAGetLastError();
    return error != WSAEWOULDBLOCK && error != WSAEINTR;
#else
    return errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR;
#endif
}
} // namespace corasio::detail::socket
