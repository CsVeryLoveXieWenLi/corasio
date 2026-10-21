/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <cstddef>
#include <string_view>
#include <utility>


namespace corasio::detail::text {
void to_lower(char* data, std::size_t size) noexcept;

// "example.com:8080" / "[::1]:8080" / "example.com" / "::1"
std::pair<std::string_view, std::string_view> split_hostname(std::string_view hostname) noexcept;

// "keep-alive, Upgrade" 这种逗号列表里找 token，大小写不敏感
bool has_token(std::string_view value, std::string_view token) noexcept;
} // namespace corasio::detail::text
