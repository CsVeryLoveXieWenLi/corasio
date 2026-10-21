/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: 线程局部内存池，按大小取还；block 负责自动归还
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <cstddef>


namespace corasio::detail::memory_pool {
char* get(std::size_t size) noexcept;
void put(char* p, std::size_t size) noexcept;

class block {
public:
    explicit block(std::size_t size) noexcept : data_(get(size)), size_(size) {};
    ~block() {
        if (data_ != nullptr) put(data_, size_);
    };

    block(const block&) = delete;
    block& operator=(const block&) = delete;

    block(block&& other) noexcept : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    };
    block& operator=(block&&) = delete;

public:
    char* data() const noexcept { return data_; };
    std::size_t size() const noexcept { return size_; };

private:
    char* data_ = nullptr;
    std::size_t size_ = 0;
};
} // namespace corasio::detail::memory_pool
