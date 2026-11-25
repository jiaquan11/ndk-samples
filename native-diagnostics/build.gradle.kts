plugins {
    id("ndksamples.android.library")
}

android {
    namespace = "com.android.ndk.samples.nativediagnostics"

    defaultConfig {
        consumerProguardFiles("consumer-rules.pro")
    }

    externalNativeBuild {
        // native-diagnostics 是静态 Prefab 库，最终消费端仍需要链接它公开依赖的
        // base::base。这里把跨 Prefab Package 的依赖写入发布元数据。
        experimentalProperties["prefab.native-diagnostics.exportLibraries"] =
            listOf("//base:base", "-ldl")

        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    buildFeatures {
        // 当前模块既消费 :base 发布的 Prefab，又向业务 Native Target 发布
        // native-diagnostics 静态库和公开头文件。
        prefab = true
        prefabPublishing = true
    }

    prefab {
        create("native-diagnostics") {
            headers = "src/main/cpp/include"
        }
    }
}

dependencies {
    // native-diagnostics 的最终链接接口包含 base::base，因此 Gradle/AAR 层
    // 也使用 api 传递 :base，保证消费端能够解析对应 Prefab Package。
    api(project(":base"))
    testImplementation(libs.junit)
}
