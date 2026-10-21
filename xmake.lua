--[[
Author: CsVeryLoveXieWenLi
Description: corasio 工程入口
@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
--]]

-- ============================================================
-- 0) 安装（类似 cmake install）
--
--    scripts\install_sdk.ps1                    -> build\install
--    scripts\install_sdk.ps1 -Prefix D:\sdk\corasio
--
--    装出来的结构（corasio 自己的头套一层 corasio\，库内布局不动）：
--      include\asio.hpp, asio\**                  asio 1.38.2（header-only）
--      include\llhttp.h                           llhttp 9.4.3
--      include\corasio\{http,websocket,detail}\   corasio 的公共头
--      lib\corasio.lib, llhttp.lib
--
--    使用方：
--      #include <corasio/http/client.h>
--      g++ ... -I<prefix>/include -I<prefix>/include/corasio -L<prefix>/lib -lcorasio -llhttp
--
--    第二个 -I 是因为库内的头按 "http/xxx.h" 相对引用自己（树内布局不变）。
--    TLS（tls_client）不装 openssl —— 谁用 TLS 谁自己带 libressl。
-- ============================================================

-- ============================================================
-- 1) 根作用域：整个工程的公共配置
--    只放"确实该全局统一"的东西：工具链、语言标准、构建模式、编译开关。
--    这里写的优化/指令集等设置，会被下面 includes() 引入的第三方 target 继承，
--    所以第三方库如果在意自己的编译选项，就在它自己的 xmake.lua 里写死。
-- ============================================================

-- 工具链
set_toolchains('mingw')


-- 编译等级
-- 三个模式都注册上，才能用 `xmake f -m debug` / `-m release` 切换；
-- 只注册一个的话，切模式时没有任何编译选项生效。
add_rules('mode.releasedbg')
add_rules('mode.debug')
add_rules('mode.release')


-- 扩展指令集
add_vectorexts('sse4.2')


-- 优化等级
-- 加 is_mode 判断，避免 `xmake f -m debug` 时被 -O3 顶掉 -g（单步调试会跳行）
if is_mode('releasedbg', 'release') then
    set_optimize('aggressive')
end


-- MinGW 默认走 emutls：asio 在多个 TU 里各实例化一份"模板静态 thread_local"时
-- TLS 槽位不会被正确合并，运行期会踩到别的变量（表现为 invalid_service_owner / SEGV）
-- 加 -mthreads 改走原生 TLS 即可
if is_plat('windows') then
    add_cxflags('-mthreads')
    add_ldflags('-mthreads')
end


-- 语言版本
set_languages('clatest', 'c++20')


-- 目标 Windows 版本（不定义时 asio 会 warn 并假设 Win7）
-- 这是"工程支持的最低系统版本"决策，所以放在根作用域而不是 asio 里
if is_plat('windows') then
    add_defines('_WIN32_WINNT=0x0601')     -- 0x0601 = Win7, 0x0A00 = Win10
end


-- 构建时自动刷新 compile_commands.json（clangd / clang-tidy 直接读它）
add_rules('plugin.compile_commands.autoupdate', {outputdir = '.'})


-- ============================================================
-- 2) 引入第三方依赖（vendored，源码就在 external/ 下）
--    includes() 只是把对方的 xmake.lua 执行一遍，把里面的 target
--    注册到本工程中；这一步不编译任何东西。真正的编译发生在构建时，
--    而且是"被依赖到才编译"（asio 是 headeronly，永远不编译）。
-- ============================================================
includes('external/asio-1.38.2')
includes('external/llhttp-9.4.3')


-- ============================================================
-- 3) 自己的 target
-- ============================================================
target('corasio')
    set_kind('static')                  -- 库工程改成 'static' 或 'shared'，二进制可执行文件 'binary'

    add_files('src/**.cpp')

    -- 依赖一写，头文件路径（public includedirs）和
    -- 静态库（llhttp）就自动带上了，不需要手写 -I / -l
    -- public：corasio 的公共头里直接 include 了 asio / llhttp 的头，
    --         所以依赖 corasio 的 target 也应该自动拿到它们的头文件路径
    add_deps('asio', 'llhttp', {public = true})

    -- 编译期头文件路径（树内保持 #include "http/client.h" 这种写法）
    add_includedirs('include', {public = true})

    -- 供 `xmake install` 用；scripts\install_sdk.ps1 会把这三个目录挪进 include\corasio\
    add_headerfiles('include/(http/**.h)')
    add_headerfiles('include/(websocket/**.h)')
    add_headerfiles('include/(detail/**.h)')


-- ============================================================
-- 4) 测试
--    所有测试都写在 tests/ 下，tests/main.cpp 是唯一入口
--    （一个可执行文件只能有一个 main，其它 .cpp 只放辅助代码）
--
--    xmake build corasio_tests    只构建测试
--    xmake run corasio_tests      构建并运行
--    xmake test                   跑所有注册的测试（看退出码）
-- ============================================================
target('corasio_tests')
    set_kind('binary')

    -- 平时不构建，避免拖慢库的构建；需要时再显式构建
    set_default(false)

    -- xmake run 默认在 target 输出目录里跑；测试要读 tests/ 下的证书，切回工程根目录
    set_rundir('$(projectdir)')

    add_files('tests/**.cpp')

    -- corasio 的 asio/llhttp 依赖是 public 的，这里不用再写一遍
    add_deps('corasio')

    -- TLS 客户端（tls_client）只有测试用得到：库本身不 include 也不链接 libressl。
    -- tls_client 是 header-only，谁包含它谁自己带 openssl 头 + 库
    if is_plat('windows') then
        local libressl = 'C:/MyData/Libs/StaticMingw/libressl-4.3.2/x64'
        add_includedirs(path.join(libressl, 'include'))
        add_linkdirs(path.join(libressl, 'lib'))
        add_links('ssl', 'crypto')
        add_syslinks('ws2_32', 'crypt32', 'userenv', 'gdi32', 'bcrypt')
    end

    -- 注册给 `xmake test`
    add_tests('default')
