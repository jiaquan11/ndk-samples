# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.22.1)
project("unittest")

include(AppLibrary)

# 三个 Package 来自两类 Gradle 依赖：base 来自本地 Project，
# googletest 和 junit-gtest 来自外部 AAR；进入 CMake 后统一表现为
# find_package 导入的 CMake Target。
find_package(base REQUIRED CONFIG)
find_package(googletest REQUIRED CONFIG)
find_package(junit-gtest REQUIRED CONFIG)

add_app_library(adder OBJECT adder.cpp)

add_app_library(
    unittest
    SHARED
    $<TARGET_OBJECTS:adder>
    native-lib.cpp
)

target_link_libraries( # Specifies the target library.
    unittest
    PRIVATE
    base::base
)

add_app_library(app_tests NO_VERSION_SCRIPT SHARED adder_test.cpp)
target_link_libraries(app_tests
    PRIVATE
    $<TARGET_OBJECTS:adder>
    googletest::gtest
    junit-gtest::junit-gtest
)
