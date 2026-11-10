dependencyResolutionManagement {
    // build-logic 是独立构建，不会自动继承主构建的仓库和版本目录。
    repositories {
        google()
        mavenCentral()
    }
    versionCatalogs {
        create("libs") {
            // 显式复用主工程的版本目录，保证约定插件与主工程使用相同版本。
            from(files("../gradle/libs.versions.toml"))
        }
    }
}

rootProject.name = "build-logic"
