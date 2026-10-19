--[[
Author: CsVeryLoveXieWenLi
Description: corasio 工程入口
@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
--]]

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
    add_deps('asio', 'llhttp')

    -- 等你有了自己的公共头目录，再打开这行
    add_includedirs('include', {public = true})
