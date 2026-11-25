// Copyright (C) 2015 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cerrno>

// LOG_TAG 是调用方唯一允许配置的源码级 Tag。该内部转接宏由本头文件统一
// 管理，后续供 LOG*/CHECKF 使用；若外部提前定义，立即报错，避免
// 调用方绕过公开配置入口并改变内部语义。
#ifdef NDKSAMPLES_LOG_TAG_INTERNAL
#error "NDKSAMPLES_LOG_TAG_INTERNAL must not be defined"
#endif

// Clang/GCC 支持 __FILE_NAME__ 时直接把文件名写入二进制，避免 __FILE__
// 的构建机绝对路径即使经过运行期裁剪，仍以字符串字面量残留在发布 so。
// 其它编译器回退到 __FILE__，后端仍会在展示前提取 basename。
#if defined(__FILE_NAME__)
#define NDKSAMPLES_SOURCE_FILE_INTERNAL __FILE_NAME__
#else
#define NDKSAMPLES_SOURCE_FILE_INTERNAL __FILE__
#endif

// 调用单元可以在首次包含本头文件前定义 LOG_TAG。未定义时传入 nullptr，
// 由后端依次使用进程名和 "ndk-samples" 作为防御兜底 Tag。
#ifdef LOG_TAG
#define NDKSAMPLES_LOG_TAG_INTERNAL LOG_TAG
#else
#define NDKSAMPLES_LOG_TAG_INTERNAL nullptr
#endif

namespace ndksamples::base {

enum LogSeverity {
  VERBOSE,
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  FATAL,
};

// 设置当前链接映像允许输出的最低日志等级，默认值为 VERBOSE。超出枚举
// 有效范围的值会被限制到 VERBOSE～FATAL。
// 普通 LOG 宏会在 Tag 和格式化参数求值前检查该阈值，CHECKF 不受
// 普通过滤规则影响。
void SetMinimumLogSeverity(LogSeverity severity) noexcept;

// 原子读取并返回当前链接映像正在使用的最低日志等级。
LogSeverity GetMinimumLogSeverity() noexcept;

// 判断指定等级当前是否允许输出。该接口使用原子状态，可被多个线程并发
// 调用；包括 LOGF 在内的普通日志统一按照最低等级判断。
bool IsLoggable(LogSeverity severity) noexcept;

// 将自定义日志函数标记为 printf 风格接口，使 Clang/GCC 在编译期检查
// 格式占位符与可变参数是否匹配。两个参数都是从 1 开始的函数参数序号：
// format_index 指向格式字符串，first_argument 指向第一个可变参数。
#if defined(__clang__) || defined(__GNUC__)
#define NDKSAMPLES_PRINTF_FORMAT(format_index, first_argument) \
  __attribute__((format(printf, format_index, first_argument)))
#else
// 其他编译器不支持 GNU format 属性时只跳过静态检查，不影响函数声明和运行。
#define NDKSAMPLES_PRINTF_FORMAT(format_index, first_argument)
#endif

void LogPrint(LogSeverity severity, const char* tag, const char* file,
              unsigned int line, const char* format, ...)
    NDKSAMPLES_PRINTF_FORMAT(5, 6);

// 由 CHECKF 宏统一调用，不应绕过宏直接使用。
void CheckFailed(const char* condition, const char* tag, const char* file,
                 unsigned int line, const char* format, ...)
    NDKSAMPLES_PRINTF_FORMAT(5, 6);

// 该宏只服务于以上函数声明；属性展开后立即清理，避免污染包含方的宏环境。
#undef NDKSAMPLES_PRINTF_FORMAT

namespace detail {

// errno 是 <cerrno> 提供的当前线程错误状态入口。该对象在调用线程中构造时，
// 无需外部传参即可保存线程此刻的 errno；离开局部作用域时由析构函数自动恢复。
// LOG* 只依赖对象生命周期完成保存和恢复；saved_errno() 供需要在同一保护
// 范围内重新读取原始错误码的内部格式化逻辑使用。
class ErrnoRestorer {
 public:
  ErrnoRestorer() : saved_errno_(errno) {}
  ~ErrnoRestorer() { errno = saved_errno_; }

  // 禁止复制，避免同一个保护范围出现多个负责恢复 errno 的对象。
  ErrnoRestorer(const ErrnoRestorer&) = delete;
  ErrnoRestorer& operator=(const ErrnoRestorer&) = delete;

  int saved_errno() const { return saved_errno_; }

 private:
  const int saved_errno_;
};

}  // namespace detail

}  // namespace ndksamples::base

#define NDKSAMPLES_LOG_PRINT(severity_value, tag_value, ...)              \
  do {                                                                    \
    ::ndksamples::base::detail::ErrnoRestorer ndksamples_errno_restorer_; \
    const auto ndksamples_log_severity_ = (severity_value);               \
    if (::ndksamples::base::IsLoggable(ndksamples_log_severity_)) {       \
      ::ndksamples::base::LogPrint(ndksamples_log_severity_, (tag_value), \
                                   NDKSAMPLES_SOURCE_FILE_INTERNAL,        \
                                   __LINE__, __VA_ARGS__);                 \
    }                                                                     \
  } while (false)

#define LOGV_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::VERBOSE, tag, __VA_ARGS__)
#define LOGD_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::DEBUG, tag, __VA_ARGS__)
#define LOGI_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::INFO, tag, __VA_ARGS__)
#define LOGW_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::WARNING, tag, __VA_ARGS__)
#define LOGE_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::ERROR, tag, __VA_ARGS__)
// LOGF 使用 Android Fatal 优先级记录日志，但不会自动终止进程；
// 条件失败后必须终止时，应使用 CHECKF。
#define LOGF_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::FATAL, tag, __VA_ARGS__)

#define LOGV(...) LOGV_TAG(NDKSAMPLES_LOG_TAG_INTERNAL, __VA_ARGS__)
#define LOGD(...) LOGD_TAG(NDKSAMPLES_LOG_TAG_INTERNAL, __VA_ARGS__)
#define LOGI(...) LOGI_TAG(NDKSAMPLES_LOG_TAG_INTERNAL, __VA_ARGS__)
#define LOGW(...) LOGW_TAG(NDKSAMPLES_LOG_TAG_INTERNAL, __VA_ARGS__)
#define LOGE(...) LOGE_TAG(NDKSAMPLES_LOG_TAG_INTERNAL, __VA_ARGS__)
#define LOGF(...) LOGF_TAG(NDKSAMPLES_LOG_TAG_INTERNAL, __VA_ARGS__)

#define CHECKF(condition, ...)                                                 \
  do {                                                                         \
    if (!(condition)) {                                                        \
      ::ndksamples::base::CheckFailed(                                        \
          #condition, NDKSAMPLES_LOG_TAG_INTERNAL,                            \
          NDKSAMPLES_SOURCE_FILE_INTERNAL, __LINE__, __VA_ARGS__);            \
    }                                                                          \
  } while (false)
