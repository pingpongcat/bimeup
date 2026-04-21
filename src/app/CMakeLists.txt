add_executable(bimeup main.cpp)
target_link_libraries(bimeup PRIVATE bimeup_tools bimeup_platform bimeup_renderer bimeup_core bimeup_ui)
add_dependencies(bimeup bimeup_shaders)
bimeup_enable_sanitizers(bimeup)

# Make project version, shader path, and default sample IFC available
target_compile_definitions(bimeup PRIVATE
    BIMEUP_VERSION="${PROJECT_VERSION}"
    BIMEUP_SHADER_DIR="${BIMEUP_SHADER_OUTPUT_DIR}"
    BIMEUP_SAMPLE_IFC="${CMAKE_SOURCE_DIR}/assets/sample.ifc"
)
