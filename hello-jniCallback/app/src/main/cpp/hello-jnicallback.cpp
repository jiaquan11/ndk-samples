// Copyright (C) 2016 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#define LOG_TAG "hello-jniCallback"
#include <base/logging.h>
#include <errno.h>
#include <iterator>
#include <inttypes.h>
#include <jni.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

// 保存 Native 工作线程回调 Java 所需的 VM、全局引用、方法 ID 和同步状态。
typedef struct tick_context {
  // JavaVM 是进程级入口，可以跨线程长期保存；JNIEnv 只能由各线程自行取得。
  JavaVM* javaVM;

  // JNI_OnLoad 创建并长期持有的 JniHandler 类和对象全局引用。
  jclass jniHandlerClz;
  jobject jniHandlerObj;

  // 一次 StartTicks/StopTicks 会话内持有的 Activity 对象全局引用。
  jobject mainActivityObj;

  // jmethodID 是可缓存的方法句柄，不是 JNI Reference，不调用 DeleteRef。
  // updateStatusMethod 的声明类由进程级 JniHandler 全局引用保持有效；
  // updateTimerMethod 的声明类由会话级 Activity 对象全局引用保持有效。
  jmethodID updateStatusMethod;
  jmethodID updateTimerMethod;

  // Activity 生命周期在主线程调用 StartTicks/StopTicks；互斥量和条件变量
  // 保护它们与分离式工作线程共同访问的运行状态。stopRequested 是工作线程
  // 的停止谓词，running 是生命周期线程的退出谓词；条件变量既用于等待下一次
  // 单调时钟截止点，也用于通知这两个状态发生变化。
  pthread_mutex_t lock;
  pthread_cond_t condition;
  int syncInitialized;
  int stopRequested;
  int running;
} TickContext;

// 教学示例只维护一个进程级上下文，不支持多个并行 Activity 计时会话。
TickContext g_ctx;

#ifndef HELLO_JNI_ABI
#error "HELLO_JNI_ABI must be defined by CMake"
#endif

// 第二个参数是调用该实例方法的 MainActivity 对象，本函数不需要使用。
// HELLO_JNI_ABI 由 CMake 根据当前 ANDROID_ABI 传入，再通过 NewStringUTF()
// 创建 Java String；这里不再重复判断 CPU 架构宏。
jstring StringFromJni(JNIEnv* env, jobject) {
  return env->NewStringUTF("Hello from JNI !  Compiled with ABI "
                           HELLO_JNI_ABI ".");
}

static bool QueryRuntimeInfo(JNIEnv* env, jobject instance) {
  // 演示调用 JniHandler 的静态方法和实例方法；类与对象全局引用已在
  // JNI_OnLoad 阶段完成初始化。该函数也在加载线程执行；查找或调用过程
  // 若产生 pending exception，则保留给加载失败路径，而不是在这里清除。
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

  // buildVersion 是局部引用，用完需要 DeleteLocalRef；返回的字符指针只是
  // 临时视图，必须与 ReleaseStringUTFChars() 成对使用。
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

  // 该 pthread 不是从上层 Java 方法进入 Native 的，回调异常返回到 Native
  // 后没有 Java 调用者继续接收。记录并清除 pending exception 后，线程
  // 才能继续执行引用清理、Detach 和退出通知。
  LOGE("Java exception while %s", operation);
  env->ExceptionDescribe();
  env->ExceptionClear();
  return true;
}

static bool SendJavaMsg(JNIEnv* env, jobject instance, jmethodID func,
                        const char* msg) {
  // 工作线程会反复回调，必须在每次调用后删除本次创建的局部 jstring，
  // 避免局部引用一直累积到线程最终 Detach。
  jstring javaMsg = env->NewStringUTF(msg);
  if (javaMsg == nullptr) {
    LOGE("Failed to create Java status message");
    ReportAndClearJavaException(env, "creating a status message");
    return false;
  }

  env->CallVoidMethod(instance, func, javaMsg);
  env->DeleteLocalRef(javaMsg);

  // Java 回调异常在当前 Native 线程收口，并通过 false 进入统一退出流程。
  return !ReportAndClearJavaException(env, "calling JniHandler.updateStatus");
}

// 只能在没有工作线程可能继续使用引用时调用：要么初始化失败、线程尚未创建，
// 要么 StopTicks() 已经确认工作线程退出。
static void ReleaseMainActivityReferences(JNIEnv* env) {
  if (g_ctx.mainActivityObj != nullptr) {
    env->DeleteGlobalRef(g_ctx.mainActivityObj);
    g_ctx.mainActivityObj = nullptr;
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

  // instance 目前只在本次 Native 调用内有效；工作线程会跨越 StartTicks()
  // 返回继续运行，因此必须把 Activity 对象提升为全局引用。对象的强全局
  // 引用也会保持其所属类和 ClassLoader 存活，无需再保存一份类全局引用。
  jobject globalMainActivity = env->NewGlobalRef(instance);
  if (globalMainActivity == nullptr) {
    LOGE("Failed to create MainActivity object global reference");
    env->DeleteLocalRef(mainActivityClass);
    return false;
  }

  // 只有对象全局引用和两个方法句柄全部准备成功后才提交到全局上下文。
  // 两个 jmethodID 的声明类分别由进程级 JniHandler 全局引用和会话级
  // Activity 对象全局引用保持有效，无需再额外保存 Activity 的 jclass。
  env->DeleteLocalRef(mainActivityClass);
  g_ctx.mainActivityObj = globalMainActivity;
  g_ctx.updateStatusMethod = updateStatusMethod;
  g_ctx.updateTimerMethod = updateTimerMethod;
  return true;
}

static void FinishTickerThread(TickContext* context,
                               bool attachedByThisThread) {
  // 所有 JNI 调用都已结束；只有本线程主动 Attach 时才对称 Detach。
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

  // 在同一把锁下更新谓词并发送通知，StopTicks() 被唤醒后才能观察到
  // running 已经变为 0，而不是只依赖一次可能提前发生的通知。
  context->running = 0;
  context->stopRequested = 0;
  const int signalResult = pthread_cond_broadcast(&context->condition);
  if (signalResult != 0) {
    LOGE("Failed to signal ticker state, ErrorCode = %d", signalResult);
  }
  pthread_mutex_unlock(&context->lock);
}

static bool IsTickerStopRequested(TickContext* context) {
  // 读写 stopRequested 使用同一把锁；加锁失败时按“请求停止”处理，
  // 优先让工作线程结束，避免在同步状态未知时继续回调 Java。
  const int lockResult = pthread_mutex_lock(&context->lock);
  if (lockResult != 0) {
    LOGE("Failed to lock ticker state, ErrorCode = %d", lockResult);
    return true;
  }
  const bool stopRequested = context->stopRequested != 0;
  pthread_mutex_unlock(&context->lock);
  return stopRequested;
}

enum class TickWaitResult {
  kDeadlineReached,
  kStopRequested,
  kError,
};

// 统一从单调时钟读取时间，避免系统墙上时钟调整改变 tick 的等待长度。
static bool ReadMonotonicTime(struct timespec* time) {
  if (clock_gettime(CLOCK_MONOTONIC, time) == 0) return true;

  const int clockError = errno;
  LOGE("Failed to read monotonic clock, ErrorCode = %d", clockError);
  return false;
}

static bool HasPassedDeadline(const struct timespec& current,
                              const struct timespec& deadline) {
  return current.tv_sec > deadline.tv_sec ||
         (current.tv_sec == deadline.tv_sec &&
          current.tv_nsec > deadline.tv_nsec);
}

static TickWaitResult WaitForNextTickOrStop(
    TickContext* context, const struct timespec& deadline) {
  const int lockResult = pthread_mutex_lock(&context->lock);
  if (lockResult != 0) {
    LOGE("Failed to lock ticker wait state, ErrorCode = %d", lockResult);
    return TickWaitResult::kError;
  }

  TickWaitResult waitResult = TickWaitResult::kStopRequested;
  int waitError = 0;
  while (!context->stopRequested) {
    const int result =
        pthread_cond_timedwait(&context->condition, &context->lock, &deadline);
    if (result == ETIMEDOUT) {
      // 超时返回时仍持有互斥量；再次检查谓词，避免把并发停止误判为新 tick。
      waitResult = context->stopRequested ? TickWaitResult::kStopRequested
                                          : TickWaitResult::kDeadlineReached;
      break;
    }
    if (result != 0) {
      waitError = result;
      waitResult = TickWaitResult::kError;
      break;
    }
    // 条件变量可能虚假唤醒，也可能被 StopTicks() 广播唤醒；继续以
    // stopRequested 为准，通知本身不代表必须停止。
  }

  const int unlockResult = pthread_mutex_unlock(&context->lock);
  if (unlockResult != 0) {
    LOGE("Failed to unlock ticker wait state, ErrorCode = %d", unlockResult);
    return TickWaitResult::kError;
  }
  if (waitError != 0) {
    LOGE("Failed to wait for next tick, ErrorCode = %d", waitError);
  }
  return waitResult;
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
    // C++ 创建的 pthread 默认没有 JNIEnv，需要先接入 JVM；该 JNIEnv 只
    // 属于当前工作线程，不能交给主线程或其它 pthread 使用。
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

  if (!SendJavaMsg(env, pctx->jniHandlerObj, pctx->updateStatusMethod,
                   "TickerThread status: start ticking ...")) {
    FinishTickerThread(pctx, attachedByThisThread);
    return context;
  }

  while (!IsTickerStopRequested(pctx)) {
    // 绝对截止时间使用 CLOCK_MONOTONIC，不受用户改时间或网络校时影响；
    // 条件变量初始化时也绑定同一时钟，避免用错时钟域解释 deadline。
    struct timespec deadline;
    if (!ReadMonotonicTime(&deadline)) break;
    deadline.tv_sec += 1;

    // CallVoidMethod() 在当前 Native pthread 上进入 Java；updateTimer()
    // 内部再通过 runOnUiThread() 把真正的 View 更新切回主线程。
    env->CallVoidMethod(pctx->mainActivityObj, pctx->updateTimerMethod);
    if (ReportAndClearJavaException(env, "calling MainActivity.updateTimer")) {
      break;
    }

    struct timespec callbackEnd;
    if (!ReadMonotonicTime(&callbackEnd)) break;
    const bool processingTooLong = HasPassedDeadline(callbackEnd, deadline);

    // 同一个条件变量既负责等待下一次绝对 deadline，也负责响应停止广播。
    // 即使停止通知早于 timedwait，受锁保护的 stopRequested 也不会丢失。
    const TickWaitResult waitResult = WaitForNextTickOrStop(pctx, deadline);
    if (waitResult != TickWaitResult::kDeadlineReached) break;

    if (processingTooLong) {
      if (!SendJavaMsg(env, pctx->jniHandlerObj, pctx->updateStatusMethod,
                       "TickerThread error: processing too long!")) {
        break;
      }
    }
  }

  // 最后一条状态消息失败也不能跳过统一清理，否则 StopTicks() 可能永久等待。
  SendJavaMsg(env, pctx->jniHandlerObj, pctx->updateStatusMethod,
              "TickerThread status: ticking stopped");
  FinishTickerThread(pctx, attachedByThisThread);
  return context;
}

// Java onResume() 调用该入口创建计时线程和回调所需的全局引用。
void StartTicks(JNIEnv* env, jobject instance) {
  // syncInitialized 表示一轮会话的全局引用、互斥量和条件变量已经建立，
  // 不只是“曾经创建过一个 pthread”。生命周期入口由主线程串行调用。
  if (g_ctx.syncInitialized) {
    LOGE("Ticker thread is already initialized");
    return;
  }

  if (!InitializeMainActivityCallbacks(env, instance)) return;

  pthread_t threadInfo;
  pthread_attr_t threadAttr;
  pthread_condattr_t conditionAttr;

  int result = pthread_attr_init(&threadAttr);
  if (result != 0) {
    LOGE("Failed to initialize thread attributes, ErrorCode = %d", result);
    ReleaseMainActivityReferences(env);
    return;
  }

  // pthread detached 表示无需 pthread_join() 回收线程资源；它与 JNI 的
  // AttachCurrentThread()/DetachCurrentThread() 是两个不同层面的概念。
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

  result = pthread_condattr_init(&conditionAttr);
  if (result != 0) {
    LOGE("Failed to initialize ticker condition attributes, ErrorCode = %d",
         result);
    pthread_mutex_destroy(&g_ctx.lock);
    pthread_attr_destroy(&threadAttr);
    ReleaseMainActivityReferences(env);
    return;
  }

  // pthread_cond_timedwait() 默认按 CLOCK_REALTIME 解释绝对时间；这里显式
  // 改为单调时钟，必须与工作线程计算 deadline 使用的时钟保持一致。
  result = pthread_condattr_setclock(&conditionAttr, CLOCK_MONOTONIC);
  if (result != 0) {
    LOGE("Failed to set monotonic condition clock, ErrorCode = %d", result);
    pthread_condattr_destroy(&conditionAttr);
    pthread_mutex_destroy(&g_ctx.lock);
    pthread_attr_destroy(&threadAttr);
    ReleaseMainActivityReferences(env);
    return;
  }

  result = pthread_cond_init(&g_ctx.condition, &conditionAttr);
  const int destroyConditionAttrResult =
      pthread_condattr_destroy(&conditionAttr);
  if (destroyConditionAttrResult != 0) {
    LOGE("Failed to destroy ticker condition attributes, ErrorCode = %d",
         destroyConditionAttrResult);
  }
  if (result != 0) {
    LOGE("Failed to initialize ticker condition, ErrorCode = %d", result);
    pthread_mutex_destroy(&g_ctx.lock);
    pthread_attr_destroy(&threadAttr);
    ReleaseMainActivityReferences(env);
    return;
  }

  // 先完成 attr、mutex、condition 和回调引用准备，再发布运行状态并创建线程；
  // 任一步失败都只回滚此前已经成功取得的资源。
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
  // 唤醒工作线程的定时等待。即使它此刻仍在执行 Java 回调，稍后进入
  // timedwait 前也会先看到 stopRequested，不依赖通知本身被保存。
  const int wakeResult = pthread_cond_broadcast(&g_ctx.condition);
  if (wakeResult != 0) {
    LOGE("Failed to wake ticker wait, ErrorCode = %d", wakeResult);
  }

  // 工作线程遇到 Java 回调异常时也会进入结束状态；循环检查 running
  // 可以处理虚假唤醒、提前通知以及上面的停止广播。
  while (g_ctx.running) {
    // pthread_cond_wait() 会原子地释放互斥量并休眠，返回前重新取得互斥量；
    // 使用 while 同时处理虚假唤醒和通知早于等待发生的情况。
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
  // FindClass 使用以 '/' 分隔的 JNI 内部类名。注册表中的方法名和描述符
  // 必须与 Java 声明逐字一致，尤其要保留 StopTicks 的大写首字母。
  jclass c = env->FindClass("com/example/hellojnicallback/MainActivity");
  if (c == nullptr) return JNI_ERR;

  static const JNINativeMethod methods[] = {
      // 实例方法的 jobject this 是隐式参数，不写入 JNI 方法描述符。
      {"stringFromJNI", "()Ljava/lang/String;",
       reinterpret_cast<void*>(StringFromJni)},
      {"startTicks", "()V", reinterpret_cast<void*>(StartTicks)},
      {"StopTicks", "()V", reinterpret_cast<void*>(StopTicks)},
  };
  return env->RegisterNatives(c, methods,
                              static_cast<jint>(std::size(methods)));
}

static void ReleaseJniHandlerReferences(JNIEnv* env) {
  // 当前没有 JNI_OnUnload，成功加载后的引用按进程生命周期保留；
  // 该函数主要用于 JNI_OnLoad 中途失败时回滚已创建的全局引用。
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
  // 在 Native 中查找 Java 类、定位无参构造器 <init>()V 并创建对象。
  // 局部引用只服务于初始化，成功后转换成可跨调用和线程使用的全局引用。
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
  // 此时尚未创建工作线程，可以安全重置唯一的进程级上下文。只缓存可
  // 跨线程使用的 JavaVM，不把当前加载线程的 JNIEnv 保存到 g_ctx。
  memset(&g_ctx, 0, sizeof(g_ctx));

  g_ctx.javaVM = vm;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;  // 当前虚拟机不支持请求的 JNI 版本。
  }

  // 先建立 Java Native 方法到 C++ 函数的绑定，再创建长期 JniHandler
  // 引用并查询运行信息；任一步失败都不能把共享库视为正常加载。
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
  // RegisterNatives() 成功返回 JNI_OK（0），但 JNI_OnLoad 成功必须返回
  // 本库支持的 JNI 版本号。
  return JNI_VERSION_1_6;
}
