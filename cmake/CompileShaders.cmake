find_program(GLSLC glslc REQUIRED)

function(bimeup_compile_shaders TARGET SHADER_DIR OUTPUT_DIR)
    # CONFIGURE_DEPENDS makes CMake re-glob on every build so newly-added shader
    # files are picked up without a manual reconfigure.
    file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
        "${SHADER_DIR}/*.vert"
        "${SHADER_DIR}/*.frag"
        "${SHADER_DIR}/*.comp"
    )
    # Stage 9.3 — ray-tracing stages need SPIR-V 1.4 + Vulkan 1.2 target; kept
    # in a separate bucket so raster/compute shaders stay on glslc defaults.
    file(GLOB RT_SHADER_SOURCES CONFIGURE_DEPENDS
        "${SHADER_DIR}/*.rgen"
        "${SHADER_DIR}/*.rchit"
        "${SHADER_DIR}/*.rahit"
        "${SHADER_DIR}/*.rmiss"
        "${SHADER_DIR}/*.rcall"
        "${SHADER_DIR}/*.rint"
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

    foreach(SHADER ${RT_SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SPIRV_FILE "${OUTPUT_DIR}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT ${SPIRV_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${GLSLC} --target-env=vulkan1.2 --target-spv=spv1.4
                    ${SHADER} -o ${SPIRV_FILE}
            DEPENDS ${SHADER}
            COMMENT "Compiling RT shader ${SHADER_NAME} -> ${SHADER_NAME}.spv"
            VERBATIM
        )

        list(APPEND SPIRV_OUTPUTS ${SPIRV_FILE})
    endforeach()

    add_custom_target(${TARGET} ALL DEPENDS ${SPIRV_OUTPUTS})
endfunction()
