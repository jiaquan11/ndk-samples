// Copyright (C) 2016 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

package com.example.hellojnicallback;

import android.os.Build;
import androidx.annotation.Keep;
import android.util.Log;

public class JniHandler {
    // 这些方法由 JNI 按名称查找；@Keep 防止压缩和混淆破坏回调契约。
    @Keep
    private void updateStatus(String msg) {
        if (msg.toLowerCase().contains("error")) {
            Log.e("JniHandler", "Native Err: " + msg);
        } else {
            Log.i("JniHandler", "Native Msg: " + msg);
        }
    }

    @Keep
    static public String getBuildVersion() {
        return Build.VERSION.RELEASE;
    }

    @Keep
    public long getRuntimeMemorySize() {
        return Runtime.getRuntime().freeMemory();
    }
}
