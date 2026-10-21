/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: 随机字节（握手 key、每帧的掩码 key）
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <cstddef>


namespace corasio::detail::random {
void fill(unsigned char* data, std::size_t size) noexcept;
} // namespace corasio::detail::random
