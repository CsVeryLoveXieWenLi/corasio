/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "corasio/websocket/client.h"

#include "corasio/detail/random.h"

#include <asio.hpp>
#include <llhttp.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>


namespace corasio::websocket {
// client 的实现编进库里；tls_client 是 header-only，谁用谁实例化（只有那边才需要 openssl）
template class basic_client<http::plain_stream>;
} // namespace corasio::websocket
