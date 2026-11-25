#include <iterator>
#include <jni.h>

#include <stdexcept>

#include <native_diagnostics/jni_boundary.h>

namespace {

// 教学用故障注入点：主动抛出一个 C++ 异常，便于观察跨 JNI 边界前的
// 捕获与转换过程。生产代码中的异常通常来自真实业务或底层库调用。
void MightThrow() { throw std::runtime_error("A C++ runtime_error"); }

void ThrowsException(JNIEnv* env, jobject /* this */) {
  // 统一边界守卫集中捕获标准、非标准和领域 C++ 异常，生成结构化诊断
  // 事件，再映射成当前线程的 Java pending exception。这里属于同步
  // Java -> Native 调用，守卫返回后 ART 会沿原 Kotlin 调用栈派发异常。
  ndksamples::diagnostics::InvokeJniVoid(
      env,
      ndksamples::diagnostics::JniCallContext{
          "exceptions", "throwsException", NATIVE_DIAGNOSTICS_HERE},
      [] { MightThrow(); });
}

static jint RegisterNativeMethods(JNIEnv* env) {
  // FindClass() 使用 JNI 内部类名，包路径以 '/' 分隔。
  jclass mainActivityClass =
      env->FindClass("com/example/exceptions/MainActivity");
  if (mainActivityClass == nullptr) return JNI_ERR;

  // 方法表把 Kotlin external 声明连接到对应的 C++ 函数。
  static const JNINativeMethod methods[] = {
      {"throwsException", "()V", reinterpret_cast<void*>(ThrowsException)},
  };

  const jint result =
      env->RegisterNatives(mainActivityClass, methods,
                           static_cast<jint>(std::size(methods)));
  env->DeleteLocalRef(mainActivityClass);
  return result;
}

}  // namespace

// System.loadLibrary("exceptions") 装载 libexceptions.so 后会触发该入口。
// JNI_OnLoad 只负责取得当前线程的 JNIEnv 并调度动态注册。
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* _Nonnull vm, void* _Nullable) {
  JNIEnv* env;
  // 当前线程已经接入虚拟机；GetEnv() 只取得线程私有的 JNIEnv，并确认
  // 本库使用的 JNI 1.6 接口可用。
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  const jint registration_result = RegisterNativeMethods(env);
  if (registration_result != JNI_OK) return registration_result;

  // 注册成功后返回本库支持的 JNI 版本，不能返回 JNI_OK（值为 0）。
  return JNI_VERSION_1_6;
}
