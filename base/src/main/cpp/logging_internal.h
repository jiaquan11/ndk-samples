// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdarg.h>

#include <string>

namespace ndksamples::base::internal {

// 日志实现与性能基准共用的私有格式化入口。该头文件不位于公开 include
// 目录，因此不会随 Prefab 暴露给普通消费方，也不属于稳定公共 API。
std::string StringVPrintf(const char* format, va_list args);

}  // namespace ndksamples::base::internal
