find_package(Vulkan REQUIRED)

add_library(bimeup_renderer STATIC
    src/VulkanContext.cpp
    src/Device.cpp
    src/Swapchain.cpp
    src/RenderLoop.cpp
    src/Buffer.cpp
    src/Shader.cpp
    src/Pipeline.cpp
    src/DescriptorSet.cpp
    src/Camera.cpp
    src/MeshBuffer.cpp
    src/Highlight.cpp
    src/Lighting.cpp
    src/ShadowPass.cpp
    src/ShadowTransmissionPipeline.cpp
    src/ViewportNavigator.cpp
    src/FirstPersonController.cpp
    src/ClipPlane.cpp
    src/ClipPlaneManager.cpp
    src/SectionFillPipeline.cpp
    src/EdgeOverlayPipeline.cpp
    src/DiskMarkerPipeline.cpp
    src/DiskMarker.cpp
    src/Tonemap.cpp
    src/TonemapPipeline.cpp
    src/OctNormal.cpp
    src/LinearDepth.cpp
    src/DepthLinearizePipeline.cpp
    src/DepthMipPipeline.cpp
    src/SsaoBlurPipeline.cpp
    src/XeGtaoMath.cpp
    src/SsaoXeGtaoPipeline.cpp
    src/SmaaMath.cpp
    src/SmaaLut.cpp
    src/SmaaEdgePipeline.cpp
    src/SmaaWeightsPipeline.cpp
    src/SmaaBlendPipeline.cpp
    src/SunPosition.cpp
    src/SkyColor.cpp
    src/AccelerationStructure.cpp
    src/TopLevelAS.cpp
    src/RayTracingPipeline.cpp
    src/RtShadowPass.cpp
    src/RtAoPass.cpp
    src/RtIndoorPass.cpp
    src/RtSunCompositePipeline.cpp
    src/RtIndoorCompositePipeline.cpp
)

target_include_directories(bimeup_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bimeup_renderer PUBLIC Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator glm::glm PRIVATE bimeup_tools smaa::lut)
bimeup_enable_sanitizers(bimeup_renderer)
