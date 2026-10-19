--[[
Author: CsVeryLoveXieWenLi
Description: asio 1.38.2 —— 纯头文件库（header-only），不产生任何编译产物
@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
--]]

target("asio")
    -- 关键 1：kind 必须是 headeronly，这样 xmake 不会去编译它，
    --         只在"依赖它的 target"上做配置传递。
    set_kind("headeronly")

    -- 关键 2：头文件目录要 public，才会自动传递给所有 add_deps 了它的 target。
    --         不写 public 的话，调用方还得自己再写一遍 add_includedirs。
    add_includedirs("include", {public = true})

    -- 头文件库里"需要用到的宏"同样要 public，否则调用方看到的是另一套头文件分支。
    -- 例：禁掉 asio 的废弃 API（如果你以后要用 asio::io_service 这类老接口，就注释掉这行）
    add_defines("ASIO_NO_DEPRECATED", {public = true})

    -- 头文件库也可能需要系统库。Windows 下 asio 用到 Winsock，
    -- 必须 public 才能一路传到最终 exe 的链接命令里。
    if is_plat("windows") then
        add_syslinks("ws2_32", "mswsock", {public = true})
    elseif is_plat("linux") then
        add_syslinks("pthread", {public = true})
    end

    -- 以下仅为支持 xmake install / xmake package 导出头文件，不影响编译。
    -- 注意 .hpp 和 .ipp 两种后缀都要写，asio 的 impl 都在 .ipp 里。
    add_headerfiles("include/(asio.hpp)")
    add_headerfiles("include/(asio/**.hpp)")
    add_headerfiles("include/(asio/**.ipp)")
