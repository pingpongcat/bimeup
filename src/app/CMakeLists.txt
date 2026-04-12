add_executable(bimeup main.cpp)
target_link_libraries(bimeup PRIVATE bimeup_tools)

# Make project version available
target_compile_definitions(bimeup PRIVATE
    BIMEUP_VERSION="${PROJECT_VERSION}"
)
