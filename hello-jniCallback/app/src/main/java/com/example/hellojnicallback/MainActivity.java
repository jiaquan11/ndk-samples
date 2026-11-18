// Copyright (C) 2016 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

package com.example.hellojnicallback;

import androidx.annotation.Keep;
import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;

import com.example.hellojnicallback.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {
    int hour = 0;
    int minute = 0;
    int second = 0;
    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // ActivityMainBinding 由 activity_main.xml 在构建期生成，根 View
        // 作为 Activity 内容视图，字段则直接对应布局中带 id 的控件。
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
    }

    @Override
    public void onResume() {
        super.onResume();

        // 每次回到前台都开启一轮新的计时会话。startTicks() 会在 Native
        // 层缓存回调方法、持有当前 Activity 的全局引用并创建工作线程。
        hour = minute = second = 0;
        binding.hellojniMsg.setText(stringFromJNI());
        startTicks();
    }

    @Override
    public void onPause () {
        super.onPause();

        // StopTicks() 会请求停止并等待 Native 工作线程真正结束；只有确认
        // 不会继续回调后，Native 层才释放 Activity 全局引用和同步对象。
        StopTicks();
    }

    // Native 通过字符串 "updateTimer" 查找该方法，@Keep 防止压缩或混淆
    // 改名。回调最初运行在 Native pthread 上，不能直接操作 View。
    @Keep
    private void updateTimer() {
        ++second;
        if(second >= 60) {
            ++minute;
            second -= 60;
            if(minute >= 60) {
                ++hour;
                minute -= 60;
            }
        }
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                String ticks = "" + MainActivity.this.hour + ":" +
                        MainActivity.this.minute + ":" +
                        MainActivity.this.second;
                MainActivity.this.binding.tickView.setText(ticks);
            }
        });
    }

    static {
        // 参数是逻辑库名，不包含 lib 前缀和 .so 后缀。类初始化时装载
        // libhello-jnicallback.so，并触发 JNI_OnLoad() 完成动态注册。
        System.loadLibrary("hello-jnicallback");
    }

    // 三个方法都由 RegisterNatives() 动态绑定。名称和 JNI 签名必须与
    // Native 注册表完全一致；StopTicks 的首字母大写也是当前契约的一部分。
    public native  String stringFromJNI();
    public native void startTicks();
    public native void StopTicks();
}
