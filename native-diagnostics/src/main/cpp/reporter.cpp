#define LOG_TAG "native-diagnostics"
#include <base/logging.h>

#include <native_diagnostics/reporter.h>

#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <cinttypes>
#include <exception>

#include "internal/runtime.h"

namespace ndksamples::diagnostics {
namespace {

std::atomic<NativeErrorReporter> g_reporter{nullptr};
std::atomic<std::uint64_t> g_sequence{1};
thread_local bool g_is_reporting = false;

constexpr int kMaxDimensionBytes = 96;
constexpr int kMaxSourceBytes = 192;
constexpr int kMaxMessageBytes = 768;
constexpr std::size_t kMaxLoggedFrames = 16;

const char* SafeText(const char* text) noexcept {
  return text == nullptr ? "" : text;
}

const char* FileName(const char* path) noexcept {
  const char* safe_path = SafeText(path);
  const char* file_name = safe_path;
  for (const char* cursor = safe_path; *cursor != '\0'; ++cursor) {
    if (*cursor == '/' || *cursor == '\\') file_name = cursor + 1;
  }
  return file_name;
}

const char* CategoryName(ErrorCategory category) noexcept {
  switch (category) {
    case ErrorCategory::kInvalidArgument:
      return "invalid_argument";
    case ErrorCategory::kInvalidState:
      return "invalid_state";
    case ErrorCategory::kIo:
      return "io";
    case ErrorCategory::kResourceExhausted:
      return "resource_exhausted";
    case ErrorCategory::kInternal:
      return "internal";
    case ErrorCategory::kUnknown:
      return "unknown";
  }
  return "unknown";
}

const char* StackOriginName(NativeStackOrigin origin) noexcept {
  switch (origin) {
    case NativeStackOrigin::kUnavailable:
      return "unavailable";
    case NativeStackOrigin::kExceptionConstruction:
      return "exception_construction";
    case NativeStackOrigin::kJniCatchBoundary:
      return "jni_catch_boundary";
  }
  return "unavailable";
}

void LogEvent(const NativeDiagnosticEvent& event) {
  LOGE(
      "Native error id=%016" PRIx64
      " category=%s domain=%.*s code=%" PRId32
      " module=%.*s operation=%.*s thread=%" PRId64
      " source=%.*s:%" PRIu32 " function=%.*s message=%.*s",
      event.diagnostic_id, CategoryName(event.error.category),
      kMaxDimensionBytes, SafeText(event.error.domain), event.error.code,
      kMaxDimensionBytes, SafeText(event.error.module), kMaxDimensionBytes,
      SafeText(event.error.operation), event.thread_id, kMaxSourceBytes,
      SafeText(event.error.source.file), event.error.source.line,
      kMaxDimensionBytes, SafeText(event.error.source.function),
      kMaxMessageBytes, SafeText(event.error.message));

  const NativeStackTrace& trace = event.stack_trace;
  if (trace.frame_count == 0U) return;

  LOGE("Native stack origin=%s frames=%" PRIu16 " truncated=%s",
       StackOriginName(trace.origin), trace.frame_count,
       trace.truncated ? "true" : "false");
  const std::size_t logged_frames =
      trace.frame_count < kMaxLoggedFrames ? trace.frame_count
                                           : kMaxLoggedFrames;
  for (std::size_t index = 0; index < logged_frames; ++index) {
    const NativeStackFrameInfo frame =
        ResolveNativeStackFrame(trace.program_counters[index]);
    if (!frame.resolved) {
      LOGE("  #%02zu pc=0x%" PRIxPTR " <unresolved>", index,
           frame.program_counter);
      continue;
    }

    const char* symbol_name = frame.symbol_name;
    if (symbol_name != nullptr && symbol_name[0] != '\0') {
      LOGE("  #%02zu pc=0x%" PRIxPTR " %s symbol=%s+0x%" PRIxPTR,
           index, frame.relative_program_counter,
           FileName(frame.module_path), symbol_name, frame.symbol_offset);
    } else {
      LOGE("  #%02zu pc=0x%" PRIxPTR " %s symbol=<unknown>", index,
           frame.relative_program_counter, FileName(frame.module_path));
    }
  }
  if (trace.frame_count > logged_frames) {
    LOGE("  ... %zu additional frames omitted from Logcat",
         static_cast<std::size_t>(trace.frame_count) - logged_frames);
  }
}

void TryLogEvent(const NativeDiagnosticEvent& event) noexcept {
  try {
    LogEvent(event);
  } catch (...) {
    // 诊断日志内部可能因 Native 内存分配失败而抛出；错误处理路径不能因此
    // 覆盖原始异常或触发 std::terminate()。
  }
}

void TryLogReporterFailure(const char* message) noexcept {
  try {
    LOGE("%s", message);
  } catch (...) {
    // Reporter 和回退日志同时失败时只能放弃附加诊断，原始 Java 异常转换
    // 仍由调用方继续完成。
  }
}

}  // namespace

void SetNativeErrorReporter(NativeErrorReporter reporter) noexcept {
  g_reporter.store(reporter, std::memory_order_release);
}

namespace detail {

std::uint64_t NextDiagnosticId() noexcept {
  // 不把时钟、PID 和序号做 XOR：那种混合无法给出清晰的唯一性边界，
  // 而且 wall clock 还可能回拨。单调原子序号的作用域更窄，但契约明确。
  return g_sequence.fetch_add(1, std::memory_order_relaxed);
}

std::int64_t GetCurrentNativeThreadId() noexcept {
  return static_cast<std::int64_t>(syscall(__NR_gettid));
}

void ReportNativeError(const NativeDiagnosticEvent& event) noexcept {
  TryLogEvent(event);

  NativeErrorReporter reporter =
      g_reporter.load(std::memory_order_acquire);
  if (reporter == nullptr || g_is_reporting) return;

  g_is_reporting = true;
  try {
    reporter(event);
  } catch (const std::exception&) {
    TryLogReporterFailure("Native error reporter threw std::exception");
  } catch (...) {
    TryLogReporterFailure("Native error reporter threw an unknown exception");
  }
  g_is_reporting = false;
}

}  // namespace detail
}  // namespace ndksamples::diagnostics
