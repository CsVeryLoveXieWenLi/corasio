/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: base64 编码（WebSocket 握手只用得到编码）
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <string>
#include <string_view>


namespace corasio::detail::base64 {
// 追加到 output 末尾
void encode(std::string_view input, std::string& output) noexcept;
} // namespace corasio::detail::base64
