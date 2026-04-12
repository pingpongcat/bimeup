find_package(Vulkan REQUIRED)

add_library(bimeup_renderer STATIC
    src/VulkanContext.cpp
    src/Device.cpp
    src/Swapchain.cpp
    src/RenderLoop.cpp
    src/Buffer.cpp
    src/Shader.cpp
)

target_include_directories(bimeup_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bimeup_renderer PUBLIC Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator PRIVATE bimeup_tools)
bimeup_enable_sanitizers(bimeup_renderer)
