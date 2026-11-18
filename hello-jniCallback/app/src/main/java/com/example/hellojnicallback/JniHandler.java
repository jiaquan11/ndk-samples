// Copyright (C) 2016 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

package com.example.hellojnicallback;

import android.os.Build;
import androidx.annotation.Keep;
import android.util.Log;

/**
 * JNI_OnLoad() 会创建该类的实例并保存全局引用，供 Native 工作线程回调。
 * 该对象不持有 Activity，只负责提供运行信息和接收状态日志。
 */
public class JniHandler {
    // Native 工作线程通过字符串方法名调用该入口。这里只写线程安全的日志，
    // 不直接操作 View，因此不需要切换到 Android 主线程。
    @Keep
    private void updateStatus(String msg) {
        if (msg.toLowerCase().contains("error")) {
            Log.e("JniHandler", "Native Err: " + msg);
        } else {
            Log.i("JniHandler", "Native Msg: " + msg);
        }
    }

    // Native 在加载阶段通过 GetStaticMethodID() 和 CallStaticObjectMethod()
    // 调用该静态方法；@Keep 防止字符串查找依赖的方法名被改变。
    @Keep
    static public String getBuildVersion() {
        return Build.VERSION.RELEASE;
    }

    // Native 在加载阶段对已创建的 JniHandler 实例调用该方法。
    // jlong 与 Java long 对应，因此 JNI 签名为 ()J。
    @Keep
    public long getRuntimeMemorySize() {
        return Runtime.getRuntime().freeMemory();
    }
}
