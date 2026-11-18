// Copyright (C) 2016 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#include <iterator>
#include <jni.h>

#include <string>

// 该函数与 Kotlin 中的实例 native 方法 stringFromJNI() 对应。
// 第二个参数是调用它的 HelloJni 对象，本例不需要访问，因此省略参数名。
jstring StringFromJni(JNIEnv* env, jobject) {
  std::string hello = "Hello from JNI.";

  // NewStringUTF() 会把 Modified UTF-8 文本复制为 Java String；局部
  // std::string 随后销毁不会让返回的 Java 字符串引用失效。
  return env->NewStringUTF(hello.c_str());
}

// 集中完成“Java/Kotlin 方法声明 -> C++ 函数指针”的动态注册。
// 返回 JNI_OK 表示全部注册成功，其它结果会继续传给 JNI_OnLoad。
static jint RegisterNativeMethods(JNIEnv* env) {
  // FindClass() 使用 JNI 内部类名，包路径以 '/' 分隔而不是 Java 的 '.'。
  jclass c = env->FindClass("com/example/hellojni/HelloJni");
  if (c == nullptr) return JNI_ERR;

  // 每项依次给出方法名、JNI 方法描述符和 C++ 函数地址。
  // "()Ljava/lang/String;" 表示没有显式参数，返回 java.lang.String；
  // reinterpret_cast<void*>() 把具体函数地址适配为注册表要求的通用地址。
  static const JNINativeMethod methods[] = {
      {"stringFromJNI", "()Ljava/lang/String;",
       reinterpret_cast<void*>(StringFromJni)},
  };

  // std::size() 直接从静态数组计算注册项数量，避免维护硬编码数字。
  return env->RegisterNatives(c, methods,
                              static_cast<jint>(std::size(methods)));
}

// System.loadLibrary() 装载 libhello-jni.so 后，虚拟机会查找并调用
// JNI_OnLoad。extern "C" 关闭 C++ 名字改编，JNIEXPORT 保证默认可见性，
// 版本脚本再把动态导出范围收敛为该入口。
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* _Nonnull vm, void* _Nullable) {
  JNIEnv* env;

  // JNI_OnLoad 所在线程已经接入虚拟机；GetEnv() 只取得该线程对应的
  // JNIEnv 并确认 JNI 1.6 可用，不会执行 AttachCurrentThread()。
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  const jint registration_result = RegisterNativeMethods(env);
  if (registration_result != JNI_OK) return registration_result;

  // RegisterNatives() 成功返回 JNI_OK（0），但 JNI_OnLoad 成功时必须返回
  // 本库支持的 JNI 版本，不能直接把 registration_result 作为成功结果。
  return JNI_VERSION_1_6;
}
