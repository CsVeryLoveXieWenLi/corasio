/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: ASCII 小写化（HTTP field-name 大小写不敏感，解析后统一转小写）
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <cstddef>


namespace corasio::detail::ascii {
void to_lower(char* data, std::size_t size) noexcept;
} // namespace corasio::detail::ascii
