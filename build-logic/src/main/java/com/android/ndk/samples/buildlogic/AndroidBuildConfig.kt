package com.android.ndk.samples.buildlogic

import org.gradle.api.JavaVersion

// 负责本项目统一的 Android 构建标准。
object AndroidBuildConfig {
    const val COMPILE_SDK = 36
    const val TARGET_SDK = 35
    const val MIN_SDK = 21
    const val NDK = "29.0.14206865" // r29
    const val CMAKE = "4.1.2"
    val JAVA = JavaVersion.VERSION_1_8
}
