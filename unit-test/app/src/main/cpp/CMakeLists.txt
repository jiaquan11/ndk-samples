# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.22.1)
project("unittest")

include(AppLibrary)

# app_tests 使用四个 Prefab Package：base 与 native-diagnostics 来自
# 本地 Project，googletest 和 junit-gtest 来自外部 AAR；生产目标
# unittest 不依赖这些测试能力。
find_package(base REQUIRED CONFIG)
find_package(native-diagnostics REQUIRED CONFIG)
find_package(googletest REQUIRED CONFIG)
find_package(junit-gtest REQUIRED CONFIG)

# Keep base's private tests next to the implementation while reusing this
# sample's existing on-device googletest runner.
get_filename_component(NDK_SAMPLES_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(BASE_CPP_DIR "${NDK_SAMPLES_ROOT}/base/src/main/cpp")
set(BASE_TEST_DIR "${NDK_SAMPLES_ROOT}/base/src/test/cpp")
set(BASE_BENCHMARK_DIR "${NDK_SAMPLES_ROOT}/base/src/benchmark/cpp")
set(NATIVE_DIAGNOSTICS_TEST_DIR
    "${NDK_SAMPLES_ROOT}/native-diagnostics/src/test/cpp")

add_app_library(adder OBJECT adder.cpp)

add_app_library(
    unittest
    SHARED
    $<TARGET_OBJECTS:adder>
    native-lib.cpp
)

# app_tests 虽然是共享库，但只作为测试执行入口，不发布稳定 Native ABI；
# 它没有独立版本脚本，因此显式跳过公共导出规则。
add_app_library(app_tests NO_VERSION_SCRIPT SHARED
    adder_test.cpp
    native_diagnostics_jni_test_bridge.cpp
    "${BASE_TEST_DIR}/base_logging_test.cpp"
    "${NATIVE_DIAGNOSTICS_TEST_DIR}/native_diagnostics_test.cpp"
)
target_include_directories(app_tests PRIVATE "${BASE_CPP_DIR}")
target_link_libraries(app_tests
    PRIVATE
    base::base
    native-diagnostics::native-diagnostics
    $<TARGET_OBJECTS:adder>
    googletest::gtest
    junit-gtest::junit-gtest
)

# 日志性能基准是独立的设备端可执行程序，不打入 APK，也不与正确性测试
# 混合。它通过 Prefab 导入的 base::base 测量真实消费方的链接形态。
add_executable(base_logging_benchmark
    "${BASE_BENCHMARK_DIR}/base_logging_benchmark.cpp"
)
target_compile_features(base_logging_benchmark PRIVATE cxx_std_17)
target_compile_options(base_logging_benchmark PRIVATE
    -Wall
    -Wextra
    -Werror
)
target_include_directories(base_logging_benchmark PRIVATE "${BASE_CPP_DIR}")
target_link_libraries(base_logging_benchmark PRIVATE base::base)
