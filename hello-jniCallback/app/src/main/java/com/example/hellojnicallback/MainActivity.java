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
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
    }

    @Override
    public void onResume() {
        super.onResume();
        hour = minute = second = 0;
        binding.hellojniMsg.setText(stringFromJNI());
        startTicks();
    }

    @Override
    public void onPause () {
        super.onPause();
        StopTicks();
    }

    @Keep
    private void updateTimer() {
        // Native 工作线程触发计时更新；真正的 View 操作切回主线程执行。
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
        System.loadLibrary("hello-jnicallback");
    }
    public native  String stringFromJNI();
    public native void startTicks();
    public native void StopTicks();
}
