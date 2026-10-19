--[[
Author: CsVeryLoveXieWenLi
Description: llhttp 9.4.3 —— 含 C 源码，编译成静态库供上层链接
@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
--]]

target("llhttp")
    -- 关键 1：有源码就编译成静态库；上层 add_deps("llhttp") 时会自动链接。
    set_kind("static")

    -- 关键 2：编译源码。第三方代码要显式覆盖语言标准，
    --         不要依赖根 xmake.lua 里的设置，这样这个文件可以单独复用。
    set_languages("c99")

    add_files("src/*.c")

    -- 关键 3：同样要 public，否则上层 #include <llhttp.h> 找不到文件。
    add_includedirs("include", {public = true})

    -- 第三方代码不参与你自己工程的警告等级（避免别人的代码刷屏）
    set_warnings("none")

    -- 如果常量表里某些接口需要额外宏，同样用 public：
    -- add_defines("LLHTTP_STRICT_MODE", {public = true})

    add_headerfiles("include/(llhttp.h)")
