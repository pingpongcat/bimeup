find_package(Vulkan REQUIRED)

add_library(bimeup_ui STATIC
    src/HierarchyPanel.cpp
    src/ImGuiContext.cpp
    src/PropertyPanel.cpp
    src/SelectionBridge.cpp
    src/Theme.cpp
    src/Toolbar.cpp
    src/UIManager.cpp
    src/ViewportOverlay.cpp
)

target_include_directories(bimeup_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bimeup_ui
    PUBLIC imgui::imgui Vulkan::Vulkan bimeup_ifc bimeup_renderer bimeup_core
    PRIVATE bimeup_tools bimeup_platform
)
bimeup_enable_sanitizers(bimeup_ui)
