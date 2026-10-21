/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: WebSocket over TLS —— openssl 只由包含这个头文件的人引入和链接
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "http/tls_client.h"
#include "websocket/client.h"


namespace corasio::websocket {
using tls_client = basic_client<http::tls_stream>;
} // namespace corasio::websocket
