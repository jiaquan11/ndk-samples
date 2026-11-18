cmake_minimum_required(VERSION 3.22.1)
project(HelloJniCallback LANGUAGES CXX)

# Application 约定插件已把仓库公共 cmake/ 目录加入 CMAKE_MODULE_PATH。
include(AppLibrary)

# Gradle 的 implementation project(":base") 提供 AAR，Prefab 再根据
# AAR 内的元数据创建可供 CMake 链接的 Imported Target base::base。
find_package(base CONFIG REQUIRED)

# 生成供 System.loadLibrary("hello-jnicallback") 装载的
# libhello-jnicallback.so，并统一应用 C++17、告警和版本脚本规则。
add_app_library(hello-jnicallback SHARED hello-jnicallback.cpp)

# Android 工具链已经提供当前构建变体的规范 ABI 名称；将它作为字符串字面量
# 传给 C++，避免在源码中重复维护 CPU 宏与 Android ABI 的映射关系。
target_compile_definitions(hello-jnicallback
    PRIVATE
    HELLO_JNI_ABI=\"${ANDROID_ABI}\"
)

target_link_libraries(hello-jnicallback
    PRIVATE
    # 提供统一 LOG* 接口，并通过 Prefab 元数据传播 base 的 Native 链接要求。
    base::base
)
