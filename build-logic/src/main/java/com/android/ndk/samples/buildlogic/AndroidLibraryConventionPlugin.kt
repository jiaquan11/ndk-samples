package com.android.ndk.samples.buildlogic

import com.android.build.api.dsl.LibraryExtension
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.kotlin.dsl.configure

/**
 * Android Library 模块的公共构建约定。
 *
 * 模块应用 ndksamples.android.library 后，Gradle 会创建本类并调用 [apply]；
 * 这里负责应用官方 Android Library 插件，并集中写入项目通用的 Android、NDK 和 CMake 配置。
 */
class AndroidLibraryConventionPlugin : Plugin<Project> {
    override fun apply(target: Project) {
        with(target) {
            with(pluginManager) {
                // 真正让目标项目成为 Android Library，并创建 android/LibraryExtension。
                apply("com.android.library")
            }

            // 配置官方插件创建的扩展。模块自己的 android {} 会在此基础上补充或覆盖配置。
            extensions.configure<LibraryExtension> {
                compileSdk = AndroidBuildConfig.COMPILE_SDK
                ndkVersion = AndroidBuildConfig.NDK

                externalNativeBuild {
                    cmake {
                        version = AndroidBuildConfig.CMAKE
                    }
                }

                defaultConfig {
                    minSdk = AndroidBuildConfig.MIN_SDK
                    lint {
                        targetSdk = AndroidBuildConfig.TARGET_SDK
                    }
                    testOptions {
                        targetSdk = AndroidBuildConfig.TARGET_SDK
                    }
                    externalNativeBuild {
                        cmake {
                            // 所有 Library 模块共享的 CMake 参数；模块仍需指定自己的 CMakeLists.txt。
                            arguments.add("-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON")
                            arguments.add("-DCMAKE_MODULE_PATH=${rootDir.resolve("cmake").absolutePath.replace('\\', '/')}")
                        }
                    }
                    ndk {
                        // riscv64 isn't a supported Android ABI yet (August 2025), but we're
                        // enabling it here as part of that experiment. Until it's a supported ABI,
                        // don't include this in your app, as Play will block uploads of APKs which
                        // contain riscv64 libraries.
                        abiFilters.addAll(
                            listOf(
                                "arm64-v8a",
                                "armeabi-v7a",
                                "riscv64",
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

                // Studio will not automatically pass logcat through ndk-stack, so we need to avoid
                // stripping debug binaries if we want the crash trace to be readable.
                buildTypes {
                    debug {
                        packaging {
                            jniLibs {
                                keepDebugSymbols += "**/*.so"
                            }
                        }
                    }
                }
            }
        }
    }
}
