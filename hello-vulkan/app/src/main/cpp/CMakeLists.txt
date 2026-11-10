#[[
 Copyright (C) 2022 The Android Open Source Project

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
#]]

cmake_minimum_required(VERSION 4.1.0)
project(HelloVulkan LANGUAGES CXX)

include(AppLibrary)
# 从本地 :base Android Library 的 Prefab Package 导入统一日志目标。
find_package(base REQUIRED CONFIG)
# 从 Gradle 解析到的 games-activity AAR 中导入 Prefab Package，
# 并创建 game-activity::game-activity_static 等 CMake Target。
find_package(game-activity REQUIRED CONFIG)

add_app_library(hellovkjni SHARED
    vk_main.cpp
)

target_compile_definitions(hellovkjni PRIVATE
    VK_USE_PLATFORM_ANDROID_KHR=1
)

target_link_libraries(hellovkjni PUBLIC
    base::base
    vulkan
    # Prefab 负责提供 Target；WHOLE_ARCHIVE 则控制静态链接行为，
    # 要求链接器纳入该归档中的全部目标文件，二者职责不同。
    $<LINK_LIBRARY:WHOLE_ARCHIVE,game-activity::game-activity_static>
    android
)
