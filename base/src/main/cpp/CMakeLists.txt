cmake_minimum_required(VERSION 3.22.1)
project(Base LANGUAGES CXX)

include(AppLibrary)

# 真正编译 logging.cpp 并生成 libbase.a 的是 CMake/NDK，而不是 Prefab。
# 目标名 base 与 build.gradle.kts 中声明的 Prefab Module 相对应。
add_app_library(base
    STATIC
    logging.cpp
)

# base 的公开头文件只要求 C++17；内部 logging_splitters.h 使用 std::format，
# 因此仅将实现侧的编译标准提高到 C++23，不把该要求传播给消费端。
target_compile_features(base PRIVATE cxx_std_23)
target_compile_options(base PRIVATE -Wno-vla-cxx-extension)
# PUBLIC 是 CMake 目标自身的使用要求；Prefab 对外打包哪些头文件，
# 仍由 build.gradle.kts 中的 prefab.headers 单独指定。
target_include_directories(base PUBLIC include)
target_link_libraries(base PUBLIC log)
