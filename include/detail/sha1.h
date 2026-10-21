/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: SHA-1（WebSocket 握手要算 Sec-WebSocket-Accept）
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <array>
#include <cstdint>
#include <string_view>


namespace corasio::detail::sha1 {
std::array<std::uint8_t, 20> hash(std::string_view data) noexcept;
} // namespace corasio::detail::sha1
