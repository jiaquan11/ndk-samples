# Copyright (C) 2010 The Android Open Source Project
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 4.1.0)
project(NativeActivity LANGUAGES C CXX)

include(AppLibrary)
find_package(base REQUIRED CONFIG)

# This includes the AndroidNdkModules.cmake file, which is shipped with the
# NDK's CMake distribution. Including this file defines
# android_ndk_import_module_native_app_glue(), which defines the
# native_app_glue target when called.
include(AndroidNdkModules)
android_ndk_import_module_native_app_glue()

add_app_library(native-activity SHARED main.cpp)

# Linking the native_app_glue target with our native-activity target (the
# library which contains the main app code) includes native_app_glue in the app.
target_link_libraries(native-activity
    android
    base::base
    # We have to use $<LINK_LIBRARY:WHOLE_ARCHIVE,native_app_glue> rather than
    # the simpler native_app_glue spelling to instruct the linker that the
    # entire native_app_glue static library should be included in
    # libnative-activity.so, even if the linker does not find any calls in our
    # code to native_app_glue. This is because native_app_glue is a static
    # library rather than a shared library, and normally the linker will only
    # include code from static libraries when it has found a call to that code.
    # This is usually a good thing because it reduces the size of the app, but
    # in this case the calls to native_app_glue, specifically the
    # ANativeActivity_onCreate function, don't come from us, but instead come
    # from ANativeActivity, which the linker cannot detect.
    $<LINK_LIBRARY:WHOLE_ARCHIVE,native_app_glue>
)
