// Copyright (C) 2016 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0
package com.example.hellojni

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.example.hellojni.databinding.ActivityHelloJniBinding

class HelloJni : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // ActivityHelloJniBinding 由 activity_hello_jni.xml 在构建期生成，
        // root 是这棵布局树的根 View，直接作为 Activity 的内容视图。
        val binding = ActivityHelloJniBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // 类初始化阶段已经装载 native 库并完成动态注册，这里会直接进入
        // hello-jni.cpp 中登记的 StringFromJni()。
        binding.helloTextview.text = stringFromJNI()
    }

    /**
     * 这里只声明 Native 方法，不包含实现。libhello-jni.so 的 JNI_OnLoad()
     * 会通过 RegisterNatives() 把它绑定到 C++ 的 StringFromJni()。
     *
     * 返回类型保留为可空，因为 JNI 创建 Java String 失败时可能返回 null，
     * 并同时在虚拟机中挂起异常。
     */
    external fun stringFromJNI(): String?

    /**
     * 这是刻意保留的负向示例，没有加入 C++ 的 JNINativeMethod 表。
     * external 声明不会自动生成实现；若实际调用，虚拟机找不到已注册方法
     * 或同名静态 JNI 符号，将抛出 UnsatisfiedLinkError。
     */
    external fun unimplementedStringFromJNI(): String?

    companion object {
        init {
            // 参数只写逻辑库名，不包含 lib 前缀和 .so 后缀。类初始化时装载
            // 与当前 ABI 匹配的 libhello-jni.so，随后触发 JNI_OnLoad()。
            System.loadLibrary("hello-jni")
        }
    }
}

