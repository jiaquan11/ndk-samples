cmake_minimum_required(VERSION 3.22.1)
project("hello-jni" LANGUAGES CXX)

include(AppLibrary)
# 从 AGP/Prefab 生成的 CMake Package 配置中导入 base::base。
# find_package 只完成包发现和 Imported Target 创建，不执行最终链接。
find_package(base CONFIG REQUIRED)

add_app_library(hello-jni SHARED
    hello-jni.cpp
)

target_link_libraries(hello-jni
    PRIVATE
    # target_link_libraries 才真正建立链接关系；因为 base 是静态库，
    # 只有存在对象代码引用时，链接器才会按需抽取并放入 libhello-jni.so。
    # base::base 的 Prefab 链接接口会传递其实现所需的 -llog。
    base::base
    android
)
