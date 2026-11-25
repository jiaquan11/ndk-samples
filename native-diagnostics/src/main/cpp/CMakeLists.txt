cmake_minimum_required(VERSION 3.22.1)
project(NativeDiagnostics LANGUAGES CXX)

include(AppLibrary)
find_package(base REQUIRED CONFIG)

add_app_library(native-diagnostics
    STATIC
    exception_bridge.cpp
    jni_boundary.cpp
    native_error.cpp
    native_stack.cpp
    reporter.cpp
)

target_include_directories(native-diagnostics PUBLIC include)
target_link_libraries(native-diagnostics PUBLIC base::base dl)
