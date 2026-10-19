--[[
Author: CsVeryLoveXieWenLi
Description: 
@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
--]]

-- 工具链
set_toolchains('mingw')


-- 编译等级
add_rules('mode.releasedbg')


-- 扩展指令集
add_vectorexts('sse4.2')


-- 优化等级
set_optimize('aggressive')


-- 语言版本
set_languages('clatest', 'c++20')