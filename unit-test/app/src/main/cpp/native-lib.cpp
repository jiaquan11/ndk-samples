#include <base/macros.h>
#include <jni.h>

#include "adder.h"

jint Add(JNIEnv*, jobject, jint a, jint b) { return add((int)a, (int)b); }

static jint RegisterNativeMethods(JNIEnv* env) {
  jclass c = env->FindClass("com/example/unittest/MainActivity");
  if (c == nullptr) return JNI_ERR;

  static const JNINativeMethod methods[] = {
      {"add", "(II)I", reinterpret_cast<void*>(Add)},
  };
  return env->RegisterNatives(c, methods, arraysize(methods));
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* _Nonnull vm,
                                             void* _Nullable) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  const jint registration_result = RegisterNativeMethods(env);
  if (registration_result != JNI_OK) return registration_result;

  return JNI_VERSION_1_6;
}
