package com.android.ndk.samples.buildlogic

import com.android.build.api.dsl.ApplicationExtension

import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.kotlin.dsl.configure

class AndroidApplicationConventionPlugin : Plugin<Project> {
    override fun apply(target: Project) {
        with(target) {
            with(pluginManager) {
                apply("com.android.application")
            }

            extensions.configure<ApplicationExtension> {
                compileSdk = AndroidBuildConfig.COMPILE_SDK
                ndkVersion = AndroidBuildConfig.NDK

                externalNativeBuild {
                    cmake {
                        version = AndroidBuildConfig.CMAKE
                    }
                }

                defaultConfig {
                    minSdk = AndroidBuildConfig.MIN_SDK
                    targetSdk = AndroidBuildConfig.TARGET_SDK

                    externalNativeBuild {
                        cmake {
                            arguments.add("-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON")
                            arguments.add("-DCMAKE_MODULE_PATH=${rootDir.resolve("cmake").absolutePath.replace('\\', '/')}")
                        }
                    }

                    ndk {
                        // 统一构建 Android NDK 正式支持的四种通用 ABI；模块仍可按需
                        // 收窄集合，例如只保留 arm64-v8a。
                        abiFilters.addAll(
                            listOf(
                                "arm64-v8a",
                                "armeabi-v7a",
                                "x86",
                                "x86_64",
                            )
                        )
                    }
                }
                compileOptions {
                    sourceCompatibility = AndroidBuildConfig.JAVA
                    targetCompatibility = AndroidBuildConfig.JAVA
                }

                // XML-based app modules use generated binding classes instead of
                // repeating findViewById calls. Modules without layouts simply have
                // no binding classes to consume and keep their existing UI model.
                buildFeatures {
                    viewBinding = true
                }

                // 不在 APK 中全局保留未剥离 so：本地调试使用 CMake 的
                // intermediates/cxx 产物，发布构建则由具体应用按需生成独立符号包。
            }
        }
    }
}
