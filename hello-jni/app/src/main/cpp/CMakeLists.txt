cmake_minimum_required(VERSION 3.22.1)
project("hello-jni" LANGUAGES CXX)

# Application 约定插件已把仓库根目录的 cmake/ 加入 CMAKE_MODULE_PATH，
# 因此这里可以直接加载公共的 add_app_library() 助手。
include(AppLibrary)

# SHARED 目标最终生成 libhello-jni.so；目标名也正是 Kotlin 层
# System.loadLibrary("hello-jni") 使用的逻辑库名。公共助手还会统一应用
# C++17、编译告警和同目录下的 libhello-jni.map.txt 版本脚本。
add_app_library(hello-jni SHARED
    hello-jni.cpp
)

# 本示例只通过 JNIEnv 函数表与虚拟机交互，没有直接调用其它 Android
# Native 系统库，因此不需要额外声明 target_link_libraries()。
