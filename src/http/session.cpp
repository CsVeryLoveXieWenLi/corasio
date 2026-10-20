/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "http/session.h"

#include <asio.hpp>


namespace corasio::http {
session::session(asio::ip::tcp::socket& socket, std::chrono::milliseconds& read_timeout, std::chrono::milliseconds& write_timeout)
: socket_(socket),
  read_timeout_(read_timeout),
  write_timeout_(write_timeout) {};

asio::awaitable<void> session::send(http::request& request, http::response& response) noexcept {
    // 每次发送前清空切片，否则会连着上一个响应一起发出去
    buffer_.clear();
    response.to(buffer_);

    // HEAD / 204 / 304 不能带 body：response::to() 把 body 放在最后一个切片，摘掉即可
    if (!response.get_body().empty()) {
        const auto code = response.get_status_code();
        if (request.get_method() == "HEAD" || code == "204" || code == "304") buffer_.pop_back();
    }

    // 用 as_tuple 而不是 use_awaitable：写失败（对端断开/写超时）不该抛异常，noexcept 里抛会直接 terminate
    const auto result = co_await asio::async_write(socket_, buffer_, asio::cancel_after(write_timeout_, asio::as_tuple));
    if (std::get<0>(result)) co_return;
};
} // namespace corasio::http