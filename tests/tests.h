/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: 测试共用声明。定义在 main.cpp 里，各测试文件只包含它
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

// 失败计数和断言，定义在 tests/main.cpp
extern int failures;
void check(bool cond, const char* what);

// 各测试文件里的入口
void test_client();
void test_tls_client();
