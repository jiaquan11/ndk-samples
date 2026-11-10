plugins {
    // 该 ID 由 build-logic 注册；实现类会应用 com.android.library 并注入公共构建配置。
    id("ndksamples.android.library")
}

// 这里只声明 base 自己的差异化配置，它们会与约定插件提供的公共配置合并。
android {
    namespace = "com.android.ndk.samples.base"

    externalNativeBuild {
        // AGP 8.13.0 的实验性 Prefab 发布属性：把 base 对 Android liblog
        // 的链接依赖写入 module.json，使仅通过 base 使用日志实现的消费端
        // 无需重复声明 log。
        // 该属性不属于稳定 Prefab DSL，升级 AGP 时需要重新核对。
        experimentalProperties["prefab.base.exportLibraries"] = listOf("-llog")

        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    buildFeatures {
        // Prefab 发布端开关：让 AGP 为这个 Android Library 生成可供消费的
        // Prefab Package。C/C++ 仍由 externalNativeBuild/CMake 编译。
        prefabPublishing = true
    }

    prefab {
        // 声明名为 base 的 Prefab Module；本项目中它对应同名 CMake 目标，
        // 消费端最终通过 base::base 引用它。
        create("base") {
            // 只把这棵公开头文件目录暴露给消费端，私有实现头文件不应放在这里。
            headers = "src/main/cpp/include"
        }
    }
}
