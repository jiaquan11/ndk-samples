#include <native_diagnostics/jni_boundary.h>

#include <android/log.h>

#include "internal/runtime.h"

namespace ndksamples::diagnostics {
namespace detail {
namespace {

const char* SafeText(const char* text) noexcept {
  return text == nullptr ? "" : text;
}

NativeDiagnosticEvent MakeEvent(const JniCallContext& context,
                                const char* domain, std::int32_t code,
                                ErrorCategory category, const char* message,
                                SourceLocation source,
                                NativeStackTrace stack_trace) noexcept {
  return NativeDiagnosticEvent{
      NextDiagnosticId(),
      NativeError{SafeText(domain), code, category, SafeText(context.module),
                  SafeText(context.operation), SafeText(message), source},
      GetCurrentNativeThreadId(),
      stack_trace,
  };
}

void FinishJavaException(JNIEnv* env, JavaExceptionKind kind,
                         const NativeDiagnosticEvent& event) noexcept {
  if (EnsureJavaExceptionPending(env, kind, event)) return;

  // 没有任何 Java 异常可传播时不能正常返回，否则上层会误走成功分支。
  // FatalError() 只作为异常桥接彻底失效后的最后兜底，不处理普通业务错误。
  if (!env->ExceptionCheck()) {
    env->FatalError("Native diagnostics could not create a Java exception");
  }
}

}  // namespace

void HandleNativeException(JNIEnv* env, const JniCallContext& context,
                           const NativeException& exception) noexcept {
  const NativeDiagnosticEvent event = MakeEvent(
      context, exception.domain().c_str(), exception.code(),
      exception.category(), exception.what(), exception.source(),
      exception.stack_trace());
  ReportNativeError(event);
  FinishJavaException(env, JavaExceptionKind::kNativeOperation, event);
}

void HandleInvalidArgument(JNIEnv* env, const JniCallContext& context,
                           const std::invalid_argument& exception) noexcept {
  const NativeDiagnosticEvent event =
      MakeEvent(context, "cpp", 1001, ErrorCategory::kInvalidArgument,
                exception.what(), context.source,
                CaptureNativeStackTrace(
                    NativeStackOrigin::kJniCatchBoundary, 1U));
  ReportNativeError(event);
  FinishJavaException(env, JavaExceptionKind::kIllegalArgument, event);
}

void HandleStandardException(JNIEnv* env, const JniCallContext& context,
                             const std::exception& exception) noexcept {
  const NativeDiagnosticEvent event =
      MakeEvent(context, "cpp", 1003, ErrorCategory::kInternal,
                exception.what(), context.source,
                CaptureNativeStackTrace(
                    NativeStackOrigin::kJniCatchBoundary, 1U));
  ReportNativeError(event);
  FinishJavaException(env, JavaExceptionKind::kNativeOperation, event);
}

void HandleOutOfMemory(JNIEnv* env,
                       const JniCallContext& context) noexcept {
  // bad_alloc 表明 Native 内存已紧张。这里不进入可能复制、排队或
  // 分配内存的外部 Reporter，只做最小 Logcat 记录并尝试建立 OOME。
  __android_log_write(ANDROID_LOG_ERROR, "native-diagnostics::jni",
                      "Native allocation failed; external reporter skipped");

  const NativeDiagnosticEvent event =
      MakeEvent(context, "cpp", 1002, ErrorCategory::kResourceExhausted,
                "Native memory allocation failed", context.source,
                NativeStackTrace{});
  FinishJavaException(env, JavaExceptionKind::kOutOfMemory, event);
}

void HandleUnknownException(JNIEnv* env,
                            const JniCallContext& context) noexcept {
  const NativeDiagnosticEvent event =
      MakeEvent(context, "cpp", 1099, ErrorCategory::kUnknown,
                "Unknown C++ exception", context.source,
                CaptureNativeStackTrace(
                    NativeStackOrigin::kJniCatchBoundary, 1U));
  ReportNativeError(event);
  FinishJavaException(env, JavaExceptionKind::kNativeOperation, event);
}

}  // namespace detail
}  // namespace ndksamples::diagnostics
