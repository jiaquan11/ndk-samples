// Copyright (C) 2016 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#define LOG_TAG "hello-jniCallback"
#include <base/logging.h>
#include <iterator>
#include <inttypes.h>
#include <jni.h>
#include <pthread.h>
#include <string.h>

// 保存 Native 工作线程回调 Java 所需的 VM、全局引用、方法 ID 和同步状态。
typedef struct tick_context {
  JavaVM* javaVM;
  jclass jniHandlerClz;
  jobject jniHandlerObj;
  jclass mainActivityClz;
  jobject mainActivityObj;
  jmethodID updateStatusMethod;
  jmethodID updateTimerMethod;
  // Activity 生命周期在主线程调用 StartTicks/StopTicks；互斥量和条件变量
  // 保护它们与分离式工作线程共同访问的运行状态。
  pthread_mutex_t lock;
  pthread_cond_t condition;
  int syncInitialized;
  int stopRequested;
  int running;
} TickContext;
TickContext g_ctx;

jstring StringFromJni(JNIEnv* env, jobject) {
#if defined(__arm__)
#if defined(__ARM_ARCH_7A__)
#if defined(__ARM_NEON__)
#if defined(__ARM_PCS_VFP)
#define ABI "armeabi-v7a/NEON (hard-float)"
#else
#define ABI "armeabi-v7a/NEON"
#endif
#else
#if defined(__ARM_PCS_VFP)
#define ABI "armeabi-v7a (hard-float)"
#else
#define ABI "armeabi-v7a"
#endif
#endif
#else
#define ABI "armeabi"
#endif
#elif defined(__i386__)
#define ABI "x86"
#elif defined(__x86_64__)
#define ABI "x86_64"
#elif defined(__mips64) /* mips64el-* 工具链也会定义 __mips__ */
#define ABI "mips64"
#elif defined(__mips__)
#define ABI "mips"
#elif defined(__aarch64__)
#define ABI "arm64-v8a"
#else
#define ABI "unknown"
#endif
  return env->NewStringUTF("Hello from JNI !  Compiled with ABI " ABI ".");
}

static bool QueryRuntimeInfo(JNIEnv* env, jobject instance) {
  // 演示调用 JniHandler 的静态方法和实例方法；类与对象全局引用已在
  // JNI_OnLoad 阶段完成初始化。
  jmethodID versionFunc = env->GetStaticMethodID(
      g_ctx.jniHandlerClz, "getBuildVersion", "()Ljava/lang/String;");
  if (!versionFunc) {
    LOGE("Failed to retrieve getBuildVersion() methodID @ line %d", __LINE__);
    return false;
  }
  jstring buildVersion = static_cast<jstring>(
      env->CallStaticObjectMethod(g_ctx.jniHandlerClz, versionFunc));
  if (env->ExceptionCheck() || buildVersion == nullptr) {
    LOGE("Failed to call getBuildVersion() @ line %d", __LINE__);
    if (buildVersion != nullptr) env->DeleteLocalRef(buildVersion);
    return false;
  }

  const char* version = env->GetStringUTFChars(buildVersion, NULL);
  if (!version) {
    LOGE("Unable to get version string @ line %d", __LINE__);
    env->DeleteLocalRef(buildVersion);
    return false;
  }
  LOGI("Android Version - %s", version);
  env->ReleaseStringUTFChars(buildVersion, version);

  env->DeleteLocalRef(buildVersion);

  jmethodID memFunc =
      env->GetMethodID(g_ctx.jniHandlerClz, "getRuntimeMemorySize", "()J");
  if (!memFunc) {
    LOGE("Failed to retrieve getRuntimeMemorySize() methodID @ line %d",
         __LINE__);
    return false;
  }
  jlong result = env->CallLongMethod(instance, memFunc);
  if (env->ExceptionCheck()) {
    LOGE("Failed to call getRuntimeMemorySize() @ line %d", __LINE__);
    return false;
  }

  LOGI("Runtime free memory size: %" PRId64, result);
  return true;
}

static bool ReportAndClearJavaException(JNIEnv* env, const char* operation) {
  if (!env->ExceptionCheck()) return false;

  LOGE("Java exception while %s", operation);
  env->ExceptionDescribe();
  env->ExceptionClear();
  return true;
}

static bool SendJavaMsg(JNIEnv* env, jobject instance, jmethodID func,
                        const char* msg) {
  jstring javaMsg = env->NewStringUTF(msg);
  if (javaMsg == nullptr) {
    LOGE("Failed to create Java status message");
    ReportAndClearJavaException(env, "creating a status message");
    return false;
  }

  env->CallVoidMethod(instance, func, javaMsg);
  env->DeleteLocalRef(javaMsg);
  return !ReportAndClearJavaException(env, "calling JniHandler.updateStatus");
}

static void ReleaseMainActivityReferences(JNIEnv* env) {
  if (g_ctx.mainActivityObj != nullptr) {
    env->DeleteGlobalRef(g_ctx.mainActivityObj);
    g_ctx.mainActivityObj = nullptr;
  }
  if (g_ctx.mainActivityClz != nullptr) {
    env->DeleteGlobalRef(g_ctx.mainActivityClz);
    g_ctx.mainActivityClz = nullptr;
  }
  g_ctx.updateStatusMethod = nullptr;
  g_ctx.updateTimerMethod = nullptr;
}

static bool InitializeMainActivityCallbacks(JNIEnv* env, jobject instance) {
  // 在创建 pthread 前解析全部回调；查找失败时可以把原始待处理异常直接
  // 返回给 Java 调用方，不让半初始化线程继续运行。
  jclass mainActivityClass = env->GetObjectClass(instance);
  if (mainActivityClass == nullptr) {
    LOGE("Failed to get MainActivity class");
    return false;
  }

  jmethodID updateStatusMethod = env->GetMethodID(
      g_ctx.jniHandlerClz, "updateStatus", "(Ljava/lang/String;)V");
  if (updateStatusMethod == nullptr) {
    LOGE("Failed to find JniHandler.updateStatus");
    env->DeleteLocalRef(mainActivityClass);
    return false;
  }

  jmethodID updateTimerMethod =
      env->GetMethodID(mainActivityClass, "updateTimer", "()V");
  if (updateTimerMethod == nullptr) {
    LOGE("Failed to find MainActivity.updateTimer");
    env->DeleteLocalRef(mainActivityClass);
    return false;
  }

  jclass globalMainActivityClass =
      static_cast<jclass>(env->NewGlobalRef(mainActivityClass));
  if (globalMainActivityClass == nullptr) {
    LOGE("Failed to create MainActivity class global reference");
    env->DeleteLocalRef(mainActivityClass);
    return false;
  }

  jobject globalMainActivity = env->NewGlobalRef(instance);
  if (globalMainActivity == nullptr) {
    LOGE("Failed to create MainActivity object global reference");
    env->DeleteGlobalRef(globalMainActivityClass);
    env->DeleteLocalRef(mainActivityClass);
    return false;
  }

  env->DeleteLocalRef(mainActivityClass);
  g_ctx.mainActivityClz = globalMainActivityClass;
  g_ctx.mainActivityObj = globalMainActivity;
  g_ctx.updateStatusMethod = updateStatusMethod;
  g_ctx.updateTimerMethod = updateTimerMethod;
  return true;
}

static void FinishTickerThread(TickContext* context,
                               bool attachedByThisThread) {
  if (attachedByThisThread) {
    const jint detachResult = context->javaVM->DetachCurrentThread();
    if (detachResult != JNI_OK) {
      LOGE("Failed to DetachCurrentThread, ErrorCode = %d", detachResult);
    }
  }

  const int lockResult = pthread_mutex_lock(&context->lock);
  if (lockResult != 0) {
    LOGE("Failed to lock ticker state, ErrorCode = %d", lockResult);
    return;
  }

  context->running = 0;
  context->stopRequested = 0;
  const int signalResult = pthread_cond_broadcast(&context->condition);
  if (signalResult != 0) {
    LOGE("Failed to signal ticker state, ErrorCode = %d", signalResult);
  }
  pthread_mutex_unlock(&context->lock);
}

static bool IsTickerStopRequested(TickContext* context) {
  const int lockResult = pthread_mutex_lock(&context->lock);
  if (lockResult != 0) {
    LOGE("Failed to lock ticker state, ErrorCode = %d", lockResult);
    return true;
  }
  const bool stopRequested = context->stopRequested != 0;
  pthread_mutex_unlock(&context->lock);
  return stopRequested;
}

void* UpdateTicks(void* context) {
  // Native 工作线程先获取本线程专属 JNIEnv，再安全调用 Java 回调；退出时
  // 只分离由本线程主动附加的 JNI 环境。
  TickContext* pctx = static_cast<TickContext*>(context);
  JavaVM* javaVM = pctx->javaVM;
  JNIEnv* env;
  bool attachedByThisThread = false;
  jint res = javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
  if (res == JNI_EDETACHED) {
    res = javaVM->AttachCurrentThread(&env, NULL);
    if (res != JNI_OK) {
      LOGE("Failed to AttachCurrentThread, ErrorCode = %d", res);
      FinishTickerThread(pctx, false);
      return context;
    }
    attachedByThisThread = true;
  } else if (res != JNI_OK) {
    LOGE("Failed to get JNIEnv, ErrorCode = %d", res);
    FinishTickerThread(pctx, false);
    return context;
  }

  if (!SendJavaMsg(env, pctx->jniHandlerObj, pctx->updateStatusMethod,
                   "TickerThread status: initializing...")) {
    FinishTickerThread(pctx, attachedByThisThread);
    return context;
  }

  struct timeval beginTime, curTime, usedTime, leftTime;
  const struct timeval kOneSecond = {(__kernel_time_t)1,
                                     (__kernel_suseconds_t)0};

  if (!SendJavaMsg(env, pctx->jniHandlerObj, pctx->updateStatusMethod,
                   "TickerThread status: start ticking ...")) {
    FinishTickerThread(pctx, attachedByThisThread);
    return context;
  }

  while (!IsTickerStopRequested(pctx)) {
    gettimeofday(&beginTime, NULL);
    env->CallVoidMethod(pctx->mainActivityObj, pctx->updateTimerMethod);
    if (ReportAndClearJavaException(env, "calling MainActivity.updateTimer")) {
      break;
    }

    gettimeofday(&curTime, NULL);
    timersub(&curTime, &beginTime, &usedTime);
    timersub(&kOneSecond, &usedTime, &leftTime);
    struct timespec sleepTime;
    sleepTime.tv_sec = leftTime.tv_sec;
    sleepTime.tv_nsec = leftTime.tv_usec * 1000;

    if (sleepTime.tv_sec <= 1) {
      nanosleep(&sleepTime, NULL);
    } else {
      if (!SendJavaMsg(env, pctx->jniHandlerObj, pctx->updateStatusMethod,
                       "TickerThread error: processing too long!")) {
        break;
      }
    }
  }

  SendJavaMsg(env, pctx->jniHandlerObj, pctx->updateStatusMethod,
              "TickerThread status: ticking stopped");
  FinishTickerThread(pctx, attachedByThisThread);
  return context;
}

// Java onResume() 调用该入口创建计时线程和回调所需的全局引用。
void StartTicks(JNIEnv* env, jobject instance) {
  if (g_ctx.syncInitialized) {
    LOGE("Ticker thread is already initialized");
    return;
  }

  if (!InitializeMainActivityCallbacks(env, instance)) return;

  pthread_t threadInfo;
  pthread_attr_t threadAttr;

  int result = pthread_attr_init(&threadAttr);
  if (result != 0) {
    LOGE("Failed to initialize thread attributes, ErrorCode = %d", result);
    ReleaseMainActivityReferences(env);
    return;
  }

  result = pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);
  if (result != 0) {
    LOGE("Failed to configure detached thread, ErrorCode = %d", result);
    pthread_attr_destroy(&threadAttr);
    ReleaseMainActivityReferences(env);
    return;
  }

  result = pthread_mutex_init(&g_ctx.lock, NULL);
  if (result != 0) {
    LOGE("Failed to initialize ticker mutex, ErrorCode = %d", result);
    pthread_attr_destroy(&threadAttr);
    ReleaseMainActivityReferences(env);
    return;
  }

  result = pthread_cond_init(&g_ctx.condition, NULL);
  if (result != 0) {
    LOGE("Failed to initialize ticker condition, ErrorCode = %d", result);
    pthread_mutex_destroy(&g_ctx.lock);
    pthread_attr_destroy(&threadAttr);
    ReleaseMainActivityReferences(env);
    return;
  }

  g_ctx.syncInitialized = 1;
  g_ctx.stopRequested = 0;
  g_ctx.running = 1;

  result = pthread_create(&threadInfo, &threadAttr, UpdateTicks, &g_ctx);
  const int destroyAttrResult = pthread_attr_destroy(&threadAttr);
  if (destroyAttrResult != 0) {
    LOGE("Failed to destroy thread attributes, ErrorCode = %d",
         destroyAttrResult);
  }

  if (result != 0) {
    LOGE("Failed to create ticker thread, ErrorCode = %d", result);
    g_ctx.running = 0;
    g_ctx.syncInitialized = 0;
    pthread_cond_destroy(&g_ctx.condition);
    pthread_mutex_destroy(&g_ctx.lock);
    ReleaseMainActivityReferences(env);
  }
}

void StopTicks(JNIEnv* env, jobject) {
  // Java onPause() 请求停止，并等待分离式工作线程通过条件变量报告退出，
  // 随后才释放回调引用和同步对象。
  if (!g_ctx.syncInitialized) {
    ReleaseMainActivityReferences(env);
    return;
  }

  int result = pthread_mutex_lock(&g_ctx.lock);
  if (result != 0) {
    LOGE("Failed to lock ticker state, ErrorCode = %d", result);
    return;
  }

  g_ctx.stopRequested = 1;
  // 工作线程遇到 Java 回调异常时也会进入结束状态；循环检查谓词可以避免
  // 因通知早于等待发生而造成死锁。
  while (g_ctx.running) {
    result = pthread_cond_wait(&g_ctx.condition, &g_ctx.lock);
    if (result != 0) {
      LOGE("Failed to wait for ticker thread, ErrorCode = %d", result);
      pthread_mutex_unlock(&g_ctx.lock);
      return;
    }
  }
  pthread_mutex_unlock(&g_ctx.lock);

  ReleaseMainActivityReferences(env);

  result = pthread_cond_destroy(&g_ctx.condition);
  if (result != 0) {
    LOGE("Failed to destroy ticker condition, ErrorCode = %d", result);
  }
  result = pthread_mutex_destroy(&g_ctx.lock);
  if (result != 0) {
    LOGE("Failed to destroy ticker mutex, ErrorCode = %d", result);
  }

  g_ctx.syncInitialized = 0;
  g_ctx.stopRequested = 0;
}

static jint RegisterNativeMethods(JNIEnv* env) {
  jclass c = env->FindClass("com/example/hellojnicallback/MainActivity");
  if (c == nullptr) return JNI_ERR;

  static const JNINativeMethod methods[] = {
      {"stringFromJNI", "()Ljava/lang/String;",
       reinterpret_cast<void*>(StringFromJni)},
      {"startTicks", "()V", reinterpret_cast<void*>(StartTicks)},
      {"StopTicks", "()V", reinterpret_cast<void*>(StopTicks)},
  };
  return env->RegisterNatives(c, methods,
                              static_cast<jint>(std::size(methods)));
}

static void ReleaseJniHandlerReferences(JNIEnv* env) {
  if (g_ctx.jniHandlerObj != nullptr) {
    env->DeleteGlobalRef(g_ctx.jniHandlerObj);
    g_ctx.jniHandlerObj = nullptr;
  }
  if (g_ctx.jniHandlerClz != nullptr) {
    env->DeleteGlobalRef(g_ctx.jniHandlerClz);
    g_ctx.jniHandlerClz = nullptr;
  }
}

static bool InitializeJniHandler(JNIEnv* env) {
  jclass handlerClass =
      env->FindClass("com/example/hellojnicallback/JniHandler");
  if (handlerClass == nullptr) {
    LOGE("Failed to find JniHandler");
    return false;
  }

  jmethodID constructor = env->GetMethodID(handlerClass, "<init>", "()V");
  if (constructor == nullptr) {
    LOGE("Failed to find JniHandler constructor");
    env->DeleteLocalRef(handlerClass);
    return false;
  }

  jobject handler = env->NewObject(handlerClass, constructor);
  if (handler == nullptr || env->ExceptionCheck()) {
    LOGE("Failed to create JniHandler");
    if (handler != nullptr) env->DeleteLocalRef(handler);
    env->DeleteLocalRef(handlerClass);
    return false;
  }

  jclass globalHandlerClass =
      static_cast<jclass>(env->NewGlobalRef(handlerClass));
  if (globalHandlerClass == nullptr) {
    LOGE("Failed to create JniHandler class global reference");
    env->DeleteLocalRef(handler);
    env->DeleteLocalRef(handlerClass);
    return false;
  }

  jobject globalHandler = env->NewGlobalRef(handler);
  if (globalHandler == nullptr) {
    LOGE("Failed to create JniHandler object global reference");
    env->DeleteGlobalRef(globalHandlerClass);
    env->DeleteLocalRef(handler);
    env->DeleteLocalRef(handlerClass);
    return false;
  }

  env->DeleteLocalRef(handler);
  env->DeleteLocalRef(handlerClass);
  g_ctx.jniHandlerClz = globalHandlerClass;
  g_ctx.jniHandlerObj = globalHandler;
  return true;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  // 加载阶段完成动态注册与长期回调引用初始化。成功路径中的全局引用按进程
  // 生命周期保留，本示例没有 JNI_OnUnload；失败路径会回滚已创建的全局引用
  // 并返回 JNI_ERR。
  JNIEnv* env;
  memset(&g_ctx, 0, sizeof(g_ctx));

  g_ctx.javaVM = vm;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;  // 当前虚拟机不支持请求的 JNI 版本。
  }

  const jint registration_result = RegisterNativeMethods(env);
  if (registration_result != JNI_OK) return registration_result;

  if (!InitializeJniHandler(env)) return JNI_ERR;
  if (!QueryRuntimeInfo(env, g_ctx.jniHandlerObj)) {
    ReleaseJniHandlerReferences(env);
    return JNI_ERR;
  }

  g_ctx.syncInitialized = 0;
  g_ctx.stopRequested = 0;
  g_ctx.running = 0;
  return JNI_VERSION_1_6;
}
