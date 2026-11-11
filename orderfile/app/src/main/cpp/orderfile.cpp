#define LOG_TAG "orderfiledemo"
#include <base/logging.h>
#include <iterator>
#include <errno.h>
#include <jni.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef GENERATE_PROFILES
extern "C" int __llvm_profile_set_filename(const char*);
extern "C" int __llvm_profile_initialize_file(void);
extern "C" int __llvm_profile_dump(void);
#endif

void DumpProfileDataIfNeeded(const char* temp_dir) {
#ifdef GENERATE_PROFILES
  char profile_location[PATH_MAX] = {};
  snprintf(profile_location, sizeof(profile_location), "%s/demo.profraw",
           temp_dir);
  if (__llvm_profile_set_filename(profile_location) == -1) {
    PLOGE("__llvm_profile_set_filename(\"%s\") failed", profile_location);
    return;
  }

  if (__llvm_profile_initialize_file() == -1) {
    PLOGE("__llvm_profile_initialize_file failed");
    return;
  }

  if (__llvm_profile_dump() == -1) {
    PLOGE("__llvm_profile_dump() failed");
    return;
  }
  LOGD("Wrote profile data to %s", profile_location);
#else
  (void)temp_dir; // To avoid unused-parameter warning
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
