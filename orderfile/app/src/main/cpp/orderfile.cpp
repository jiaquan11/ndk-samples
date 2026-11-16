#define LOG_TAG "orderfiledemo"
#include <base/logging.h>
#include <jni.h>
#include <linux/limits.h>
#include <stdio.h>

#include <iterator>

#ifdef GENERATE_PROFILES
#include <profile/instr_prof_interface.h>
#endif

void DumpProfileDataIfNeeded(const char* temp_dir) {
#ifdef GENERATE_PROFILES
  char profile_location[PATH_MAX] = {};
  snprintf(profile_location, sizeof(profile_location), "%s/demo.profraw",
           temp_dir);
  // 文件名设置没有返回值；dump 提供可检查的写入结果。
  __llvm_profile_set_filename(profile_location);

  const int dump_result = __llvm_profile_dump();
  if (dump_result != 0) {
    LOGE("__llvm_profile_dump() failed, result=%d", dump_result);
    return;
  }
  LOGD("Wrote profile data to %s", profile_location);
#else
  (void)temp_dir;  // 避免未使用参数警告。
  LOGD("Did not write profile data because the app was not built for profile "
       "generation");
#endif
}

void RunWorkload(JNIEnv* env, jobject /* this */, jstring temp_dir) {
  DumpProfileDataIfNeeded(env->GetStringUTFChars(temp_dir, 0));
}

static jint RegisterNativeMethods(JNIEnv* env) {
  jclass c = env->FindClass("com/example/orderfiledemo/MainActivity");
  if (c == nullptr) return JNI_ERR;

  static const JNINativeMethod methods[] = {
      {"runWorkload", "(Ljava/lang/String;)V",
       reinterpret_cast<void*>(RunWorkload)},
  };
  return env->RegisterNatives(c, methods,
                              static_cast<jint>(std::size(methods)));
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
