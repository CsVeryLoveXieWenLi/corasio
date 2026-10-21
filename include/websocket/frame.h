/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: WebSocket 帧头编解码 + 掩码（掩码走 SSE）
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "websocket/header.h"

#include <cstddef>
#include <cstdint>
#include <string_view>


namespace corasio::websocket::frame {
// 返回帧头占多少字节；数据还不够返回 0；非法帧返回 -1
std::ptrdiff_t parse(std::string_view data, header& out) noexcept;

// out 至少留 14 字节，返回写出的字节数
std::size_t write(const header& value, char* out) noexcept;

// 原地 XOR：[data, data + size)，offset 是这段在 payload 里的起始位置
void mask(char* data, std::size_t size, std::uint32_t key, std::size_t offset = 0) noexcept;
} // namespace corasio::websocket::frame
