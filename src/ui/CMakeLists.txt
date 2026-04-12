find_package(Vulkan REQUIRED)

add_library(bimeup_ui STATIC
    src/ImGuiContext.cpp
)

target_include_directories(bimeup_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bimeup_ui
    PUBLIC imgui::imgui Vulkan::Vulkan
    PRIVATE bimeup_tools bimeup_platform bimeup_renderer
)
bimeup_enable_sanitizers(bimeup_ui)
