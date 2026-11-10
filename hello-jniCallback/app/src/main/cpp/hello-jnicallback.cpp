/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#define LOG_TAG "hello-jniCallback"
#include <base/logging.h>
#include <base/macros.h>
#include <inttypes.h>
#include <jni.h>
#include <pthread.h>
#include <string.h>

// processing callback to handler class
typedef struct tick_context {
  JavaVM* javaVM;
  jclass jniHandlerClz;
  jobject jniHandlerObj;
  jclass mainActivityClz;
  jobject mainActivityObj;
  jmethodID updateStatusMethod;
  jmethodID updateTimerMethod;
  // Activity lifecycle calls StartTicks/StopTicks serially on the main thread;
  // this lock protects the state shared with the detached worker thread.
  pthread_mutex_t lock;
  pthread_cond_t condition;
  int syncInitialized;
  int stopRequested;
  int running;
} TickContext;
TickContext g_ctx;

/* This is a trivial JNI example where we use a native method
 * to return a new VM String. See the corresponding Java source
 * file located at:
 *
 *   hello-jniCallback/app/src/main/java/com/example/hellojnicallback/MainActivity.java
 */
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
#elif defined(__mips64) /* mips64el-* toolchain defines __mips__ too */
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

/*
 *  A helper function to show how to call
 *     java static functions JniHandler::getBuildVersion()
 *     java non-static function JniHandler::getRuntimeMemorySize()
 *  The trivial implementation for these functions are inside file
 *     JniHandler.java
 */
static bool QueryRuntimeInfo(JNIEnv* env, jobject instance) {
  // Find out which OS we are running on. It does not matter for this app
  // just to demo how to call static functions.
  // Our java JniHandler class id and instance are initialized when this
  // shared lib got loaded, we just directly use them
  //    static function does not need instance, so we just need to feed
  //    class and method id to JNI
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

  // we are called from JNI_OnLoad, so got to release LocalRef to avoid leaking
  env->DeleteLocalRef(buildVersion);

  // Query available memory size from a non-static public function
  // we need use an instance of JniHandler class to call JNI
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
  (void)result;  // silence the compiler warning
  return true;
}

/*
 * A helper function to wrap java JniHandler::updateStatus(String msg)
 * JNI allow us to call this function via an instance even it is
 * private function.
 */
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
  // Resolve callbacks before creating the pthread so lookup failures can
  // return to the Java caller with their original pending exception.
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

/*
 * Main working thread function. From a pthread,
 *     calling back to MainActivity::updateTimer() to display ticks on UI
 *     calling back to JniHandler::updateStatus(String msg) for msg
 */
void* UpdateTicks(void* context) {
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

/*
 * Interface to Java side to start ticks, caller is from onResume()
 */
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

/*
 * Interface to Java side to stop ticks:
 *    we need to hold and make sure our native thread has finished before return
 *    for a clean shutdown. The caller is from onPause
 */
void StopTicks(JNIEnv* env, jobject) {
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
  // The worker also reaches this state after a callback exception, so the
  // predicate prevents a missed notification from becoming a deadlock.
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
  return env->RegisterNatives(c, methods, arraysize(methods));
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

/*
 * processing one time initialization:
 *     Cache the javaVM into our context
 *     Register native methods
 *     Find class ID for JniHandler
 *     Create an instance of JniHandler
 *     Make global reference since we are using them from a native thread
 * Note:
 *     Global references on the success path live for the process lifetime;
 *     partially initialized references are released before JNI_OnLoad fails.
 *     The pairing function JNI_OnUnload() never gets called at all.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  JNIEnv* env;
  memset(&g_ctx, 0, sizeof(g_ctx));

  g_ctx.javaVM = vm;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;  // JNI version not supported.
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
