#pragma once

#include <jni.h>

#include <cstddef>
#include <cstdint>

#include <native_diagnostics/reporter.h>

namespace ndksamples::diagnostics::detail {

// Java 异常类型只服务于模块内部的 JNI 映射，不构成业务侧公共 API。
enum class JavaExceptionKind : std::uint8_t {
  kIllegalArgument,
  kOutOfMemory,
  kNativeOperation,
};

// 已有 pending exception 时保持原样；否则创建并挂起目标异常。
// 返回 false 表示最终仍没有可交给 ART 传播的 Java 异常。
bool EnsureJavaExceptionPending(
    JNIEnv* env, JavaExceptionKind kind,
    const NativeDiagnosticEvent& event) noexcept;

NativeStackTrace CaptureNativeStackTrace(
    NativeStackOrigin origin,
    std::size_t frames_to_skip = 0) noexcept;

std::uint64_t NextDiagnosticId() noexcept;
std::int64_t GetCurrentNativeThreadId() noexcept;
void ReportNativeError(const NativeDiagnosticEvent& event) noexcept;

}  // namespace ndksamples::diagnostics::detail
