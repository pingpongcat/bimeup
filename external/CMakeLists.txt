# External dependencies — added as git submodules

# spdlog (header-only mode)
add_subdirectory(spdlog)

# glm
add_subdirectory(glm)

# GLFW (no docs/tests/examples)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

# VulkanMemoryAllocator (header-only)
add_subdirectory(VulkanMemoryAllocator)

# web-ifc (IFC parsing library)
include(web-ifc-build.cmake)

# Dear ImGui (docking branch) — built with GLFW + Vulkan backends
find_package(Vulkan REQUIRED)
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_demo.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)
add_library(imgui::imgui ALIAS imgui)

# googletest (only when tests are enabled)
if(BIMEUP_BUILD_TESTS)
    add_subdirectory(googletest)
endif()
