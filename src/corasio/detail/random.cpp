/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "corasio/detail/random.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>


namespace corasio::detail::random {
namespace {
// 64 位 LCG，掩码 key 不需要密码学强度
constexpr std::uint64_t multiplier = 6364136223846793005ULL;
constexpr std::uint64_t increment = 1442695040888963407ULL;

std::uint64_t seed() noexcept {
    const auto clock = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto thread = static_cast<std::uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));

    return clock ^ (thread * 0x9E3779B97F4A7C15ULL);
}
} // namespace

void fill(unsigned char* data, std::size_t size) noexcept {
    thread_local std::uint64_t state = seed();

    for (std::size_t index = 0; index < size; ++index) {
        state = state * multiplier + increment;
        data[index] = static_cast<unsigned char>(state >> 33); // 取高位，LCG 低位质量差
    }
}
} // namespace corasio::detail::random
