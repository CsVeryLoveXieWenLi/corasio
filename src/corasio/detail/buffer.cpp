/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "corasio/detail/buffer.h"

#include <utility>

#include <cstddef>
#include <cstring>
#include <string_view>


namespace corasio::detail {
buffer::buffer(std::size_t capacity) noexcept : block_(capacity) {};

buffer::buffer(buffer&& other) noexcept : block_(std::move(other.block_)), begin_(other.begin_), end_(other.end_) {
    other.begin_ = 0;
    other.end_ = 0;
};

std::string_view buffer::readable() noexcept { return std::string_view(block_.data() + begin_, end_ - begin_); };
std::size_t buffer::readable_size() noexcept { return end_ - begin_; };

char* buffer::write_pos() noexcept { return block_.data() + end_; };
std::size_t buffer::write_size() noexcept { return block_.size() - end_; };

void buffer::commit(std::size_t size) noexcept { end_ += size; };
void buffer::advance(std::size_t size) noexcept { begin_ += size; };

void buffer::consume(std::size_t size) noexcept {
    begin_ += size;

    if (begin_ == end_) {
        begin_ = 0;
        end_ = 0;
        return;
    }

    // 后面还有下一条报文的字节：搬到开头，好接着往后写
    std::size_t rest = end_ - begin_;
    std::memmove(block_.data(), block_.data() + begin_, rest);
    begin_ = 0;
    end_ = rest;
};

std::size_t buffer::capacity() noexcept { return block_.size(); };
} // namespace corasio::detail
