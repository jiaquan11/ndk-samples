cmake_minimum_required(VERSION 3.22.1)
project(Base LANGUAGES CXX)

include(AppLibrary)

# 真正编译 logging.cpp 并生成 libbase.a 的是 CMake/NDK，而不是 Prefab。
# 目标名 base 与 build.gradle.kts 中声明的 Prefab Module 相对应。
add_app_library(base
    STATIC
    logging.cpp
)

# add_app_library() 已通过 AppLibrary.cmake 为目标声明 C++17；
# base 的公开头文件和内部实现保持同一语言标准。
# PUBLIC 是 CMake 目标自身的使用要求；Prefab 对外打包哪些头文件，
# 仍由 build.gradle.kts 中的 prefab.headers 单独指定。
target_include_directories(base PUBLIC include)
target_link_libraries(base PUBLIC log)
