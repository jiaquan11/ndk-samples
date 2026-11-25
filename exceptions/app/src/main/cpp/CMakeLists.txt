cmake_minimum_required(VERSION 3.22.1)
project(exceptions LANGUAGES CXX)

# 引入项目统一的 Android Native 目标、版本脚本和编译选项封装。
include(AppLibrary)
# native-diagnostics 的 Prefab 链接接口公开依赖 base::base；CMake 在加载
# native-diagnostics 导入目标前需要先创建对应的 base 导入目标。
find_package(base CONFIG REQUIRED)
find_package(native-diagnostics CONFIG REQUIRED)

add_app_library(
    exceptions
    SHARED
    native-lib.cpp
)

target_link_libraries(
    exceptions
    PRIVATE
    # 通用诊断库负责 JNI 异常映射、结构化事件和统一 Native 日志。
    native-diagnostics::native-diagnostics
)
