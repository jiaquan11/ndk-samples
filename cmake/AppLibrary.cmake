include_guard()

# 对 add_library() 的公共封装，为示例 Native Target 统一设置语言标准、
# 编译告警和符号版本脚本。调用方仍负责选择 STATIC、SHARED 等目标类型。
function(add_app_library name)
    # NO_VERSION_SCRIPT 是可选布尔参数，需要自行管理导出符号的目标可显式跳过。
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

    if(NOT arg_NO_VERSION_SCRIPT)
        # 默认读取本次 CMake 配置入口目录下的 lib<目标名>.map.txt，
        # 只导出版本脚本声明的公共入口。
        # --no-undefined-version 可在脚本引用不存在的符号时直接让链接失败。
        target_link_options(${name}
            PRIVATE
            -Wl,--no-undefined-version
            -Wl,--version-script,${CMAKE_SOURCE_DIR}/lib${name}.map.txt
        )

        # 版本脚本不是编译源文件，显式声明链接依赖后，修改脚本也会触发重新链接。
        set_target_properties(${name}
            PROPERTIES
            LINK_DEPENDS ${CMAKE_SOURCE_DIR}/lib${name}.map.txt
        )
    endif()
endfunction()
