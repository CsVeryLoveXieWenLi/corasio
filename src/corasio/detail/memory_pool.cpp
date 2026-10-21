/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "corasio/detail/memory_pool.h"

#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>


namespace corasio::detail::memory_pool {
namespace {
thread_local std::vector<std::pair<std::size_t, char*>> pool;
} // namespace

char* get(std::size_t size) noexcept {
    for (auto it = pool.begin(); it != pool.end(); ++it) {
        if (it->first == size) {
            char* p = it->second;
            pool.erase(it);
            return p;
        }
    }
    return static_cast<char*>(std::malloc(size));
};

void put(char* p, std::size_t size) noexcept {
    if (p == nullptr) return;
    pool.emplace_back(size, p);
};
} // namespace corasio::detail::memory_pool
