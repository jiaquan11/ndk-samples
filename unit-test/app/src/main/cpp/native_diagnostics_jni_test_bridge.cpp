#include <jni.h>
#include <native_diagnostics/jni_boundary.h>
#include <native_diagnostics/native_error.h>
#include <native_diagnostics/reporter.h>

#include <atomic>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>

namespace {

using ndksamples::diagnostics::ErrorCategory;
using ndksamples::diagnostics::JniCallContext;
using ndksamples::diagnostics::NativeDiagnosticEvent;
using ndksamples::diagnostics::NativeException;
using ndksamples::diagnostics::NativeStackOrigin;
using ndksamples::diagnostics::SourceLocation;

std::atomic<std::uint64_t> g_last_reporter_id{0};
std::atomic<std::uint16_t> g_last_stack_frame_count{0};
std::atomic<NativeStackOrigin> g_last_stack_origin{
    NativeStackOrigin::kUnavailable};
std::atomic<int> g_failing_reporter_count{0};

JniCallContext TestContext(const char* operation,
                           SourceLocation source) noexcept {
  return JniCallContext{"native-diagnostics-test", operation, source};
}

void ResetReporterSnapshot() noexcept {
  g_last_reporter_id.store(0, std::memory_order_relaxed);
  g_last_stack_frame_count.store(0, std::memory_order_relaxed);
  g_last_stack_origin.store(NativeStackOrigin::kUnavailable,
                            std::memory_order_relaxed);
}

void CapturingReporter(const NativeDiagnosticEvent& event) {
  g_last_reporter_id.store(event.diagnostic_id, std::memory_order_relaxed);
  g_last_stack_frame_count.store(event.stack_trace.frame_count,
                                 std::memory_order_relaxed);
  g_last_stack_origin.store(event.stack_trace.origin,
                            std::memory_order_relaxed);
}

void ThrowingReporter(const NativeDiagnosticEvent&) {
  g_failing_reporter_count.fetch_add(1, std::memory_order_relaxed);
  throw std::runtime_error("reporter failure");
}

bool ThrowInitialJavaException(JNIEnv* env) {
  jclass exception_class = env->FindClass("java/lang/IllegalStateException");
  if (exception_class == nullptr) return false;

  const jint result = env->ThrowNew(exception_class, "original");
  env->DeleteLocalRef(exception_class);
  return result == JNI_OK && env->ExceptionCheck();
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_throwStandard(JNIEnv* env,
                                                                jclass) {
  ResetReporterSnapshot();
  ndksamples::diagnostics::SetNativeErrorReporter(CapturingReporter);
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("throwStandard", NATIVE_DIAGNOSTICS_HERE),
      [] { throw std::runtime_error("A C++ runtime_error"); });
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_throwInvalidArgument(
    JNIEnv* env, jclass) {
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("throwInvalidArgument", NATIVE_DIAGNOSTICS_HERE),
      [] { throw std::invalid_argument("bad argument"); });
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_throwUnknown(JNIEnv* env,
                                                               jclass) {
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("throwUnknown", NATIVE_DIAGNOSTICS_HERE),
      [] { throw 42; });
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_throwDomain(JNIEnv* env,
                                                              jclass) {
  ResetReporterSnapshot();
  ndksamples::diagnostics::SetNativeErrorReporter(CapturingReporter);
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("throwDomain", NATIVE_DIAGNOSTICS_HERE), [] {
        throw NativeException("decoder", 2001, ErrorCategory::kIo,
                              "decode failed", NATIVE_DIAGNOSTICS_HERE);
      });
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_preservePending(JNIEnv* env,
                                                                  jclass) {
  if (!ThrowInitialJavaException(env)) {
    env->FatalError(
        "Native diagnostics test could not create initial exception");
  }

  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("preservePending", NATIVE_DIAGNOSTICS_HERE),
      [] { throw std::runtime_error("must not replace original"); });
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_throwWithFailingReporter(
    JNIEnv* env, jclass) {
  g_failing_reporter_count.store(0, std::memory_order_relaxed);
  ndksamples::diagnostics::SetNativeErrorReporter(ThrowingReporter);
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("failingReporter", NATIVE_DIAGNOSTICS_HERE),
      [] { throw std::runtime_error("business failure"); });
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_throwBadAlloc(JNIEnv* env,
                                                                jclass) {
  ResetReporterSnapshot();
  ndksamples::diagnostics::SetNativeErrorReporter(CapturingReporter);
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("badAlloc", NATIVE_DIAGNOSTICS_HERE),
      [] { throw std::bad_alloc{}; });
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_invokeSuccess(JNIEnv* env,
                                                                jclass) {
  ResetReporterSnapshot();
  ndksamples::diagnostics::SetNativeErrorReporter(CapturingReporter);
  int result = 0;
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("success", NATIVE_DIAGNOSTICS_HERE),
      [&result] { result = 1; });
  return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_throwUnicode(JNIEnv* env,
                                                               jclass) {
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("unicode", NATIVE_DIAGNOSTICS_HERE),
      [] { throw std::runtime_error(u8"中文😀"); });
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_throwInvalidUtf8(JNIEnv* env,
                                                                   jclass) {
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("invalidUtf8", NATIVE_DIAGNOSTICS_HERE), [] {
        const char invalid_utf8[] = {'b', 'a', 'd', static_cast<char>(0xc3),
                                     '(', '\0'};
        throw std::runtime_error(invalid_utf8);
      });
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_throwLongMessage(JNIEnv* env,
                                                                   jclass) {
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("longMessage", NATIVE_DIAGNOSTICS_HERE),
      [] { throw std::runtime_error(std::string(1200, 'a')); });
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_throwMessageEndingWithOverflowingEmoji(
    JNIEnv* env, jclass) {
  ndksamples::diagnostics::InvokeJniVoid(
      env, TestContext("overflowingEmoji", NATIVE_DIAGNOSTICS_HERE),
      [] { throw std::runtime_error(std::string(1023, 'a') + u8"😀"); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_lastReporterId(JNIEnv*,
                                                                 jclass) {
  return static_cast<jlong>(g_last_reporter_id.load(std::memory_order_relaxed));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_lastReporterStackFrameCount(
    JNIEnv*, jclass) {
  return static_cast<jint>(
      g_last_stack_frame_count.load(std::memory_order_relaxed));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_lastReporterStackWasExceptionConstruction(
    JNIEnv*, jclass) {
  return g_last_stack_origin.load(std::memory_order_relaxed) ==
                 NativeStackOrigin::kExceptionConstruction
             ? JNI_TRUE
             : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_lastReporterStackWasJniCatchBoundary(
    JNIEnv*, jclass) {
  return g_last_stack_origin.load(std::memory_order_relaxed) ==
                 NativeStackOrigin::kJniCatchBoundary
             ? JNI_TRUE
             : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_failingReporterInvocationCount(
    JNIEnv*, jclass) {
  return static_cast<jint>(
      g_failing_reporter_count.load(std::memory_order_relaxed));
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_unittest_NativeDiagnosticsBridge_clearReporter(JNIEnv*,
                                                                jclass) {
  ndksamples::diagnostics::SetNativeErrorReporter(nullptr);
  ResetReporterSnapshot();
  g_failing_reporter_count.store(0, std::memory_order_relaxed);
}
