find_program(GLSLC glslc REQUIRED)

function(bimeup_compile_shaders TARGET SHADER_DIR OUTPUT_DIR)
    # CONFIGURE_DEPENDS makes CMake re-glob on every build so newly-added shader
    # files are picked up without a manual reconfigure.
    file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
        "${SHADER_DIR}/*.vert"
        "${SHADER_DIR}/*.frag"
        "${SHADER_DIR}/*.comp"
    )

    set(SPIRV_OUTPUTS "")

    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SPIRV_FILE "${OUTPUT_DIR}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT ${SPIRV_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${GLSLC} ${SHADER} -o ${SPIRV_FILE}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader ${SHADER_NAME} -> ${SHADER_NAME}.spv"
            VERBATIM
        )

        list(APPEND SPIRV_OUTPUTS ${SPIRV_FILE})
    endforeach()

    add_custom_target(${TARGET} ALL DEPENDS ${SPIRV_OUTPUTS})
endfunction()
