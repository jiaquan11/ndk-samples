include_guard()

# 对 add_library() 的公共封装，为示例 Native Target 统一设置语言标准和
# 编译告警，并为适用的动态库目标配置符号版本脚本。调用方仍负责选择
# STATIC、SHARED 等目标类型。
function(add_app_library name)
    # NO_VERSION_SCRIPT 是可选布尔参数；只有明确不采用公共版本脚本规则的
    # SHARED/MODULE 目标才需要显式使用。
    cmake_parse_arguments(PARSE_ARGV 1 arg "NO_VERSION_SCRIPT" "" "")
    add_library(${name} ${arg_UNPARSED_ARGUMENTS})

    # C++17 是仓库 Native Target 的统一最低能力基线。使用 PUBLIC 让仓库内
    # 直接链接该目标的消费者继承要求，不依赖 NDK 编译器当前的默认标准。
    # Prefab 不会完整传递任意 CMake compile feature，仓库外消费者仍应
    # 按发布包约定显式选择受支持的 C++ 标准。
    target_compile_features(${name} PUBLIC cxx_std_17)

    # 对项目源码启用统一且严格的诊断；第三方代码应在自己的 Target 中管理告警。
    target_compile_options(${name}
        PRIVATE
        -Wall
        -Wextra
        -Werror
    )

    # 版本脚本只参与 ELF 共享库链接；STATIC/OBJECT 不生成动态符号表，
    # 因此自动跳过，不要求无意义的 lib<目标名>.map.txt。
    get_target_property(target_type ${name} TYPE)
    if(target_type STREQUAL "SHARED_LIBRARY"
            OR target_type STREQUAL "MODULE_LIBRARY")
        if(NOT arg_NO_VERSION_SCRIPT)
            set(version_script "${CMAKE_SOURCE_DIR}/lib${name}.map.txt")

            # 当前约定要求版本脚本是源码文件。缺失时在 CMake 配置期给出
            # 明确错误，避免等到链接阶段才看到难以定位的失败。
            if(NOT EXISTS "${version_script}")
                message(FATAL_ERROR
                    "共享库目标 '${name}' 缺少符号版本脚本："
                    "'${version_script}'。请添加该文件，或在确认无需控制"
                    "导出符号时传入 NO_VERSION_SCRIPT。")
            endif()

            # 默认只导出版本脚本声明的公共入口；若脚本引用不存在的符号，
            # --no-undefined-version 会直接让链接失败。
            target_link_options(${name}
                PRIVATE
                -Wl,--no-undefined-version
                "-Wl,--version-script,${version_script}"
            )

            # 版本脚本不是编译源文件，显式声明链接依赖后，修改脚本也会触发重新链接。
            set_target_properties(${name}
                PROPERTIES
                LINK_DEPENDS "${version_script}"
            )
        endif()
    endif()
endfunction()
