#include <native_diagnostics/native_error.h>

#include <utility>

#include "internal/runtime.h"

namespace ndksamples::diagnostics {

NativeException::NativeException(std::string domain, std::int32_t code,
                                 ErrorCategory category, std::string message,
                                 SourceLocation source)
    // std::runtime_error 只提供接收 const std::string& 的文本构造函数，
    // 对 message 使用 std::move() 也不会发生移动；这里直接表达真实的复制语义。
    : std::runtime_error(message),
      domain_(std::move(domain)),
      code_(code),
      category_(category),
      source_file_(source.file == nullptr ? "" : source.file),
      source_function_(source.function == nullptr ? "" : source.function),
      source_line_(source.line),
      stack_trace_(detail::CaptureNativeStackTrace(
          NativeStackOrigin::kExceptionConstruction, 1U)) {}

}  // namespace ndksamples::diagnostics
