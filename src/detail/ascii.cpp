/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "detail/ascii.h"

#include <cstddef>


namespace corasio::detail::ascii {
void to_lower(char* data, std::size_t size) noexcept {
    for (std::size_t index = 0; index < size; ++index) {
        if (data[index] >= 'A' && data[index] <= 'Z') data[index] = static_cast<char>(data[index] - 'A' + 'a');
    }
}
} // namespace corasio::detail::ascii
