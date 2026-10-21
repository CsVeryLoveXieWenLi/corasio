/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: WebSocket 握手：key 生成、accept 计算、两端校验
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "http/request.h"
#include "http/response.h"

#include <string>
#include <string_view>


namespace corasio::websocket::handshake {
// 16 字节随机 -> base64，填 Sec-WebSocket-Key
std::string make_key();

// Sec-WebSocket-Accept = base64(sha1(key + GUID))
std::string accept_for(std::string_view key);

// 服务端：这个请求能不能升级（头名要小写，解析器吐出来的就是小写）
bool is_upgrade(http::request& request) noexcept;

// 客户端：101 + accept 对不对
bool is_accepted(http::response& response, std::string_view key) noexcept;
} // namespace corasio::websocket::handshake
