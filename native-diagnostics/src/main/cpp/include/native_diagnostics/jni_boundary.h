#pragma once

#include <jni.h>

#include <exception>
#include <new>
#include <stdexcept>
#include <utility>

#include <native_diagnostics/native_error.h>

namespace ndksamples::diagnostics {
namespace detail {

void HandleNativeException(JNIEnv* env, const JniCallContext& context,
                           const NativeException& exception) noexcept;
void HandleInvalidArgument(JNIEnv* env, const JniCallContext& context,
                           const std::invalid_argument& exception) noexcept;
void HandleStandardException(JNIEnv* env, const JniCallContext& context,
                             const std::exception& exception) noexcept;
void HandleOutOfMemory(JNIEnv* env,
                       const JniCallContext& context) noexcept;
void HandleUnknownException(JNIEnv* env,
                            const JniCallContext& context) noexcept;

}  // namespace detail

// 同步 Java/Kotlin -> Native 的 void JNI 入口守卫。它集中保证任何 C++
// 异常都在 JNI 边界内结束，并映射为当前线程的 Java pending exception。
// Native 自建线程没有等待返回的 Java 调用栈，不应直接套用此接口。
template <typename Callable>
void InvokeJniVoid(JNIEnv* env, const JniCallContext& context,
                   Callable&& callable) noexcept {
  // JVM 进入 Native 方法时必然提供当前线程的 JNIEnv。空指针只能
  // 来自调用方误用公共 API；继续执行会吞掉失败并制造假成功。
  if (env == nullptr) std::terminate();

  try {
    std::forward<Callable>(callable)();
  } catch (const NativeException& exception) {
    detail::HandleNativeException(env, context, exception);
  } catch (const std::invalid_argument& exception) {
    detail::HandleInvalidArgument(env, context, exception);
  } catch (const std::bad_alloc&) {
    detail::HandleOutOfMemory(env, context);
  } catch (const std::exception& exception) {
    detail::HandleStandardException(env, context, exception);
  } catch (...) {
    detail::HandleUnknownException(env, context);
  }
}

}  // namespace ndksamples::diagnostics
