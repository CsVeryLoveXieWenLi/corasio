/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "detail/memory_pool.h"

#include <cstddef>
#include <string_view>


namespace corasio::detail {
class buffer {
public:
    explicit buffer(std::size_t capacity) noexcept;
    ~buffer() = default;

    buffer(const buffer&) = delete;
    buffer& operator=(const buffer&) = delete;

    buffer(buffer&& other) noexcept;
    buffer& operator=(buffer&&) = delete;

public:
    std::string_view readable() noexcept;
    std::size_t readable_size() noexcept;

    char* write_pos() noexcept;
    std::size_t write_size() noexcept;

    void commit(std::size_t size) noexcept;
    void advance(std::size_t size) noexcept;
    void consume(std::size_t size) noexcept;

    std::size_t capacity() noexcept;

private:
    memory_pool::block block_;

    std::size_t begin_ = 0; // 第一个还没喂给解析器的字节
    std::size_t end_ = 0;   // 有效字节的末尾
};
} // namespace corasio::detail
