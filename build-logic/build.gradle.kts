import org.jetbrains.kotlin.gradle.tasks.KotlinCompile

plugins {
    // 将 build-logic 配置为 Gradle 插件开发工程，并使用 Kotlin 编写插件实现。
    id("java-gradle-plugin")
    `kotlin-dsl`
    alias(libs.plugins.jetbrains.kotlin.jvm)
}

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}

tasks.withType<KotlinCompile>().configureEach {
    kotlinOptions {
        jvmTarget = JavaVersion.VERSION_17.toString()
    }
}

dependencies {
    // 只在编译约定插件时使用 AGP/Kotlin Gradle Plugin API；
    // 官方插件的版本和实际应用位置由主构建负责。
    compileOnly(libs.android.gradlePlugin)
    compileOnly(libs.kotlin.gradlePlugin)
}

gradlePlugin {
    // 建立“模块中使用的插件 ID -> 实现类”的映射。
    // java-gradle-plugin 会据此自动生成 META-INF/gradle-plugins 下的插件描述文件。
    plugins {
        register("androidApplication") {
            id = "ndksamples.android.application"
            implementationClass = "com.android.ndk.samples.buildlogic.AndroidApplicationConventionPlugin"
        }
        // androidLibrary 是 build-logic 内部的注册名称；模块真正引用的是下面的 id。
        register("androidLibrary") {
            id = "ndksamples.android.library"
            implementationClass = "com.android.ndk.samples.buildlogic.AndroidLibraryConventionPlugin"
        }
        register("kotlinAndroid") {
            id = "ndksamples.android.kotlin"
            implementationClass = "com.android.ndk.samples.buildlogic.KotlinConventionPlugin"
        }
    }
}
