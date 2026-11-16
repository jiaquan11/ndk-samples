// Copyright (C) 2015 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#include "base/logging.h"

#include <android/log.h>
#include <android/set_abort_message.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <utility>

#include "logging_splitters.h"

namespace ndksamples::base {
namespace {

// 短日志优先在栈上完成格式化，避免每条日志都进行堆分配；容量包含末尾的 \0。
constexpr std::size_t kStackFormatBufferSize = 512;

// 使用 printf 规则把 va_list 格式化为字符串。短消息只执行一次格式化，
// 超出栈缓冲区时再按精确长度分配字符串并执行第二次格式化。
std::string StringVPrintf(const char* format, va_list args) {
  if (format == nullptr) {
    return "<null log format>";
  }

  // vsnprintf 可能改写 errno；函数返回时需要恢复调用者进入时的错误现场。
  detail::ErrnoRestorer errno_restorer;
  std::array<char, kStackFormatBufferSize> stack_buffer;

  // va_list 在交给 vsnprintf 后可能被消耗，因此每次格式化都使用独立副本。
  // 第一次调用既尝试直接生成短消息，也获取完整结果所需的字符数。
  va_list stack_args;
  va_copy(stack_args, args);
  const int required =
      vsnprintf(stack_buffer.data(), stack_buffer.size(), format, stack_args);
  va_end(stack_args);
  if (required < 0) {
    return "<log formatting failed>";
  }

  const auto required_size = static_cast<std::size_t>(required);
  if (required_size < stack_buffer.size()) {
    return std::string(stack_buffer.data(), required_size);
  }

  // required 不包含结尾的 \0，因此多分配一个字符交给 vsnprintf 写终止符。
  std::string result(required_size + 1, '\0');
  // 第一次 vsnprintf 可能改变 errno；第二次格式化前恢复原值，保证依赖 errno
  // 的格式说明（例如平台支持的 %m）两次得到一致结果。
  errno = errno_restorer.saved_errno();
  va_list dynamic_args;
  va_copy(dynamic_args, args);
  const int written =
      vsnprintf(result.data(), result.size(), format, dynamic_args);
  va_end(dynamic_args);
  if (written != required) {
    return "<log formatting failed>";
  }

  result.resize(required_size);
  return result;
}

// __FILE__ 可能使用 Unix 或 Windows 路径分隔符。这里只返回原字符串中
// 文件名部分的指针，不创建额外字符串。
const char* GetFileBasename(const char* file) {
  if (file == nullptr) {
    return nullptr;
  }
  const char* last_separator = strrchr(file, '/');
  const char* last_backslash = strrchr(file, '\\');
  if (last_separator == nullptr ||
      (last_backslash != nullptr && last_backslash > last_separator)) {
    last_separator = last_backslash;
  }
  return last_separator == nullptr ? file : last_separator + 1;
}

// 将库内等级转换成 Android liblog 使用的优先级。这里只负责等级映射，
// 是否终止进程由具体调用路径决定。
int32_t LogSeverityToPriority(LogSeverity severity) {
  switch (severity) {
    case VERBOSE:
      return ANDROID_LOG_VERBOSE;
    case DEBUG:
      return ANDROID_LOG_DEBUG;
    case INFO:
      return ANDROID_LOG_INFO;
    case WARNING:
      return ANDROID_LOG_WARN;
    case ERROR:
      return ANDROID_LOG_ERROR;
    case FATAL:
    default:
      return ANDROID_LOG_FATAL;
  }
}

std::atomic<LogSeverity>& GetMinimumLogSeverityState() {
  // 函数局部 static 保证首次初始化的线程安全；atomic 保证多线程读写等级时
  // 不产生数据竞争。对象故意存活到进程结束，避免其他翻译单元在静态析构
  // 期间继续记录日志时遭遇状态已经销毁的问题。
  static auto& state = *new std::atomic<LogSeverity>{VERBOSE};
  return state;
}

// 日志调用向后端传入 nullptr 时才进入这里，通常是当前翻译单元未定义
// LOG_TAG。优先使用进程名，进程名不可用时使用固定值；该兜底不属于
// 正式的 Tag 配置方式。
const char* ResolveFallbackTag() noexcept {
  const char* process_name = getprogname();
  return process_name == nullptr || process_name[0] == '\0' ? "ndk-samples"
                                                            : process_name;
}

// SplitByLogdChunks 通过该回调把每个合法片段真正写入 Android liblog。
void LogdLogChunk(LogSeverity severity, const char* tag, const char* message) {
  __android_log_write(LogSeverityToPriority(severity), tag, message);
}

// 统一完成 Tag 兜底、长日志分片和最终写入；日志等级不承担终止进程的职责。
void EmitLog(LogSeverity severity, const char* tag, const char* file,
             unsigned int line, std::string message) {
  const char* basename = GetFileBasename(file);
  // 显式空字符串仍是调用方选择的 Tag；只有 nullptr 才使用防御兜底。
  const char* resolved_tag = tag == nullptr ? ResolveFallbackTag() : tag;
  // 分片器负责按换行、logd 负载上限和 UTF-8 边界拆分，再逐片回调写入。
  SplitByLogdChunks(severity, resolved_tag, basename, line, message.c_str(),
                    LogdLogChunk);
}

// 在 CHECKF 终止进程前保存完整失败原因，便于 tombstone 等崩溃信息展示。
void SetAbortMessage(const char* file, unsigned int line,
                     const std::string& message) {
  const char* basename = GetFileBasename(file);
  if (basename == nullptr) {
    android_set_abort_message(message.c_str());
    return;
  }

  const std::string abort_message = "[" + std::string(basename) + ":" +
                                    std::to_string(line) + "] " + message;
  android_set_abort_message(abort_message.c_str());
}

}  // namespace

void SetMinimumLogSeverity(LogSeverity severity) noexcept {
  // 防御外部强制转换得到的非法枚举值，将阈值限制在支持的等级区间内。
  const int value = static_cast<int>(severity);
  if (value < static_cast<int>(VERBOSE)) {
    severity = VERBOSE;
  } else if (value > static_cast<int>(FATAL)) {
    severity = FATAL;
  }
  // 等级状态不承担其他数据的线程同步，只需要保证自身原子读写。
  GetMinimumLogSeverityState().store(severity, std::memory_order_relaxed);
}

LogSeverity GetMinimumLogSeverity() noexcept {
  // 与设置端一致使用 relaxed；读取到一个完整有效的等级即可完成过滤判断。
  return GetMinimumLogSeverityState().load(std::memory_order_relaxed);
}

bool IsLoggable(LogSeverity severity) noexcept {
  // LogSeverity 按从低到高排列，包括 FATAL 在内的普通日志统一按阈值判断。
  // CHECKF 直接调用 CheckFailed，不经过该过滤入口。
  return severity >= GetMinimumLogSeverity();
}

void LogPrint(LogSeverity severity, const char* tag, const char* file,
              unsigned int line, const char* format, ...) {
  // 宏入口会提前过滤以避免求值 Tag 和格式化参数；这里再次检查，使直接调用
  // LogPrint 的代码也遵守最低等级设置。
  if (!IsLoggable(severity)) {
    return;
  }

  // 即使调用方绕过宏直接调用，也保证日志格式化和输出不会破坏 errno 现场。
  detail::ErrnoRestorer errno_restorer;
  va_list args;
  va_start(args, format);
  std::string message = StringVPrintf(format, args);
  va_end(args);
  EmitLog(severity, tag, file, line, std::move(message));
}

void CheckFailed(const char* condition, const char* tag, const char* file,
                 unsigned int line, const char* format, ...) {
  // CHECKF 不受最低日志等级过滤：先格式化补充说明，再组合断言条件。
  va_list args;
  va_start(args, format);
  const std::string detail = StringVPrintf(format, args);
  va_end(args);

  std::string message = "Check failed: ";
  message.append(condition == nullptr ? "<null condition>" : condition);
  message.append(": ");
  message.append(detail);
  // 终止行为属于 CHECKF 的控制流契约，不由 FATAL 日志等级隐式触发。
  SetAbortMessage(file, line, message);
  EmitLog(FATAL, tag, file, line, std::move(message));
  abort();
}

}  // namespace ndksamples::base
