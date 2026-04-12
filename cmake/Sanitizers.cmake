option(BIMEUP_ENABLE_SANITIZERS "Enable ASan + UBSan in Debug builds" ON)

function(bimeup_enable_sanitizers target)
    if(NOT BIMEUP_ENABLE_SANITIZERS)
        return()
    endif()

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        return()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /fsanitize=address)
    else()
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
    endif()
endfunction()
