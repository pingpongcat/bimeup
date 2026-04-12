add_executable(bimeup main.cpp)
target_link_libraries(bimeup PRIVATE bimeup_tools bimeup_platform bimeup_renderer)
add_dependencies(bimeup bimeup_shaders)
bimeup_enable_sanitizers(bimeup)

# Make project version and shader path available
target_compile_definitions(bimeup PRIVATE
    BIMEUP_VERSION="${PROJECT_VERSION}"
    BIMEUP_SHADER_DIR="${BIMEUP_SHADER_OUTPUT_DIR}"
)
