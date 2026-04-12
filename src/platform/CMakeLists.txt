add_library(bimeup_platform STATIC
    src/Window.cpp
)

target_include_directories(bimeup_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bimeup_platform PUBLIC glfw glm::glm)
bimeup_enable_sanitizers(bimeup_platform)
