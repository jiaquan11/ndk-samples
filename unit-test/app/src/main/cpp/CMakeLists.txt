# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.22.1)
project("unittest")

include(AppLibrary)

# app_tests 使用三个 Prefab Package：base 来自本地 Project，googletest
# 和 junit-gtest 来自外部 AAR；生产目标 unittest 不依赖这些测试能力。
find_package(base REQUIRED CONFIG)
find_package(googletest REQUIRED CONFIG)
find_package(junit-gtest REQUIRED CONFIG)

# Keep base's private tests next to the implementation while reusing this
# sample's existing on-device googletest runner.
get_filename_component(NDK_SAMPLES_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(BASE_CPP_DIR "${NDK_SAMPLES_ROOT}/base/src/main/cpp")
set(BASE_TEST_DIR "${NDK_SAMPLES_ROOT}/base/src/test/cpp")

add_app_library(adder OBJECT adder.cpp)

add_app_library(
    unittest
    SHARED
    $<TARGET_OBJECTS:adder>
    native-lib.cpp
)

add_app_library(app_tests NO_VERSION_SCRIPT SHARED
    adder_test.cpp
    "${BASE_TEST_DIR}/base_logging_test.cpp"
)
target_include_directories(app_tests PRIVATE "${BASE_CPP_DIR}")
target_link_libraries(app_tests
    PRIVATE
    base::base
    $<TARGET_OBJECTS:adder>
    googletest::gtest
    junit-gtest::junit-gtest
)
