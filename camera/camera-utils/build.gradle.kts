plugins {
    id("ndksamples.android.library")
}

android {
    namespace = "com.android.ndk.samples.camera.utils"

    defaultConfig {
        minSdk = 24
    }

    externalNativeBuild {
        // camera-utils 的公开日志头依赖 base，因此 Prefab 导出目标也需要
        // 把跨 Package 的 base::base 链接要求传递给消费端。
        experimentalProperties["prefab.camera-utils.exportLibraries"] =
            listOf("//base:base")

        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    buildFeatures {
        // camera-utils 同时是 base Prefab Package 的消费端。
        prefab = true
        // 与 :base 相同，这是另一个 Prefab 发布端；它发布 camera-utils
        // Native 目标及公开头文件，供两个 Camera 应用消费。
        prefabPublishing = true
    }

    prefab {
        // 该 Prefab Module 对应 CMake 中的 camera-utils 静态库目标。
        create("camera-utils") {
            headers = "src/main/cpp/include"
        }
    }
}

dependencies {
    // camera-utils 的公开 native 头会包含 base/logging.h，因此使用 api
    // 让 Android/Gradle 消费者也能得到 base AAR。
    api(project(":base"))
}
