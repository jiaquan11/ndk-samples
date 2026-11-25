#define LOG_TAG "native-diagnostics::jni"
#include <base/logging.h>

#include "internal/runtime.h"

#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>

namespace ndksamples::diagnostics::detail {
namespace {

constexpr std::size_t kMaxJavaStringUnits = 1024;

const char* SafeText(const char* text) noexcept {
  return text == nullptr ? "" : text;
}

bool IsContinuationByte(unsigned char value) noexcept {
  return (value & 0xc0U) == 0x80U;
}

// 将标准 UTF-8 尽力转换为 UTF-16。非法或过长输入不会越界，也不会让
// 异常辅助逻辑再次抛出 C++ 异常；非法序列统一替换为 U+FFFD。
jstring NewJavaString(JNIEnv* env, const char* text) noexcept {
  std::array<jchar, kMaxJavaStringUnits> output{};
  std::size_t output_size = 0;
  const auto* cursor =
      reinterpret_cast<const unsigned char*>(SafeText(text));

  while (*cursor != 0U && output_size < output.size()) {
    std::uint32_t code_point = 0xfffdU;
    std::size_t consumed = 1;
    const unsigned char lead = cursor[0];

    if (lead < 0x80U) {
      code_point = lead;
    } else if (lead >= 0xc2U && lead <= 0xdfU && cursor[1] != 0U &&
               IsContinuationByte(cursor[1])) {
      code_point = (static_cast<std::uint32_t>(lead & 0x1fU) << 6U) |
                   static_cast<std::uint32_t>(cursor[1] & 0x3fU);
      consumed = 2;
    } else if (lead >= 0xe0U && lead <= 0xefU && cursor[1] != 0U &&
               cursor[2] != 0U && IsContinuationByte(cursor[1]) &&
               IsContinuationByte(cursor[2])) {
      const std::uint32_t candidate =
          (static_cast<std::uint32_t>(lead & 0x0fU) << 12U) |
          (static_cast<std::uint32_t>(cursor[1] & 0x3fU) << 6U) |
          static_cast<std::uint32_t>(cursor[2] & 0x3fU);
      const bool overlong = candidate < 0x800U;
      const bool surrogate = candidate >= 0xd800U && candidate <= 0xdfffU;
      if (!overlong && !surrogate) {
        code_point = candidate;
        consumed = 3;
      }
    } else if (lead >= 0xf0U && lead <= 0xf4U && cursor[1] != 0U &&
               cursor[2] != 0U && cursor[3] != 0U &&
               IsContinuationByte(cursor[1]) &&
               IsContinuationByte(cursor[2]) &&
               IsContinuationByte(cursor[3])) {
      const std::uint32_t candidate =
          (static_cast<std::uint32_t>(lead & 0x07U) << 18U) |
          (static_cast<std::uint32_t>(cursor[1] & 0x3fU) << 12U) |
          (static_cast<std::uint32_t>(cursor[2] & 0x3fU) << 6U) |
          static_cast<std::uint32_t>(cursor[3] & 0x3fU);
      if (candidate >= 0x10000U && candidate <= 0x10ffffU) {
        code_point = candidate;
        consumed = 4;
      }
    }

    cursor += consumed;
    if (code_point <= 0xffffU) {
      output[output_size++] = static_cast<jchar>(code_point);
      continue;
    }

    // 一个非 BMP 码点需要一对 UTF-16 代理项；只剩一个位置时直接截断，
    // 不能把半个代理项交给 Java。
    if (output_size + 1U >= output.size()) break;
    code_point -= 0x10000U;
    output[output_size++] =
        static_cast<jchar>(0xd800U + (code_point >> 10U));
    output[output_size++] =
        static_cast<jchar>(0xdc00U + (code_point & 0x3ffU));
  }

  return env->NewString(output.data(), static_cast<jsize>(output_size));
}

void DeleteLocalRef(JNIEnv* env, jobject reference) noexcept {
  if (reference != nullptr) env->DeleteLocalRef(reference);
}

const char* ClassName(JavaExceptionKind kind) noexcept {
  switch (kind) {
    case JavaExceptionKind::kIllegalArgument:
      return "java/lang/IllegalArgumentException";
    case JavaExceptionKind::kOutOfMemory:
      return "java/lang/OutOfMemoryError";
    case JavaExceptionKind::kNativeOperation:
      return "com/android/ndk/samples/nativediagnostics/NativeOperationException";
  }
  return "com/android/ndk/samples/nativediagnostics/NativeOperationException";
}

jthrowable CreateSimpleException(JNIEnv* env, jclass exception_class,
                                 const char* message) noexcept {
  jmethodID constructor = env->GetMethodID(
      exception_class, "<init>", "(Ljava/lang/String;)V");
  if (constructor == nullptr || env->ExceptionCheck()) return nullptr;

  jstring java_message = NewJavaString(env, message);
  if (java_message == nullptr) return nullptr;

  jthrowable exception = static_cast<jthrowable>(
      env->NewObject(exception_class, constructor, java_message));
  DeleteLocalRef(env, java_message);
  if (env->ExceptionCheck()) {
    DeleteLocalRef(env, exception);
    return nullptr;
  }
  return exception;
}

jthrowable CreateNativeOperationException(
    JNIEnv* env, jclass exception_class,
    const NativeDiagnosticEvent& event) noexcept {
  jmethodID constructor = env->GetMethodID(
      exception_class, "<init>",
      "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;J"
      "Ljava/lang/String;)V");
  if (constructor == nullptr || env->ExceptionCheck()) return nullptr;

  jstring domain = NewJavaString(env, event.error.domain);
  if (domain == nullptr) return nullptr;
  jstring module = NewJavaString(env, event.error.module);
  if (module == nullptr) {
    DeleteLocalRef(env, domain);
    return nullptr;
  }
  jstring operation = NewJavaString(env, event.error.operation);
  if (operation == nullptr) {
    DeleteLocalRef(env, module);
    DeleteLocalRef(env, domain);
    return nullptr;
  }
  jstring message = NewJavaString(env, event.error.message);
  if (message == nullptr) {
    DeleteLocalRef(env, operation);
    DeleteLocalRef(env, module);
    DeleteLocalRef(env, domain);
    return nullptr;
  }

  jthrowable exception = static_cast<jthrowable>(env->NewObject(
      exception_class, constructor, domain,
      static_cast<jint>(event.error.code), module, operation,
      static_cast<jlong>(event.diagnostic_id), message));

  DeleteLocalRef(env, message);
  DeleteLocalRef(env, operation);
  DeleteLocalRef(env, module);
  DeleteLocalRef(env, domain);
  if (env->ExceptionCheck()) {
    DeleteLocalRef(env, exception);
    return nullptr;
  }
  return exception;
}

jthrowable CreateException(JNIEnv* env, JavaExceptionKind kind,
                           const NativeDiagnosticEvent& event) noexcept {
  jclass exception_class = env->FindClass(ClassName(kind));
  if (exception_class == nullptr) return nullptr;

  jthrowable exception =
      kind == JavaExceptionKind::kNativeOperation
          ? CreateNativeOperationException(env, exception_class, event)
          : CreateSimpleException(env, exception_class, event.error.message);
  DeleteLocalRef(env, exception_class);
  return exception;
}

}  // namespace

bool EnsureJavaExceptionPending(
    JNIEnv* env, JavaExceptionKind kind,
    const NativeDiagnosticEvent& event) noexcept {
  if (env == nullptr) return false;

  // JNI 调用已经留下异常时，原异常通常最接近根因。公共桥不清除、不描述、
  // 也不替换它，只让 Native 尽快返回 ART。
  if (env->ExceptionCheck()) return true;

  jthrowable exception = CreateException(env, kind, event);
  if (env->ExceptionCheck()) {
    // FindClass、字符串或对象创建失败时，JVM 已留下更直接的异常。
    DeleteLocalRef(env, exception);
    return true;
  }
  if (exception == nullptr) return false;

  const jint throw_result = env->Throw(exception);
  DeleteLocalRef(env, exception);
  const bool exception_pending = env->ExceptionCheck();
  if (throw_result == JNI_OK && exception_pending) return true;

  try {
    LOGE("Failed to set Java exception class=%s diagnostic_id=%016" PRIx64,
         ClassName(kind), event.diagnostic_id);
  } catch (...) {
    // 日志失败不能让 C++ 异常越过本函数的 noexcept 边界。
  }
  return exception_pending;
}

}  // namespace ndksamples::diagnostics::detail
