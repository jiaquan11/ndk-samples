include_guard()

# Wrapper for add_library which enables common options we want for all
# libraries.
function(add_app_library name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "NO_VERSION_SCRIPT" "" "")
    add_library(${name} ${arg_UNPARSED_ARGUMENTS})

    # C++17 is the minimum Native API baseline for the sample targets. Public
    # headers such as base/logging.h expose C++17 library types, so keep this a
    # usage requirement rather than relying on the NDK compiler's default.
    target_compile_features(${name} PUBLIC cxx_std_17)

    target_compile_options(${name}
        PRIVATE
        -Wall
        -Wextra
        -Werror
    )

    if(NOT arg_NO_VERSION_SCRIPT)
        target_link_options(${name}
            PRIVATE
            -Wl,--no-undefined-version
            -Wl,--version-script,${CMAKE_SOURCE_DIR}/lib${name}.map.txt
        )

        set_target_properties(${name}
            PROPERTIES
            LINK_DEPENDS ${CMAKE_SOURCE_DIR}/lib${name}.map.txt
        )
    endif()
endfunction()
