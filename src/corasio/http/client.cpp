/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "corasio/http/client.h"

#include <asio.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>


namespace corasio::http {
// client 的实现编进库里；tls_client 是 header-only，谁用谁实例化（只有那边才需要 openssl）
template class basic_client<plain_stream>;
} // namespace corasio::http
