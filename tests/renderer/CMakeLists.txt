add_executable(renderer_tests
    VulkanContextTest.cpp
    DeviceTest.cpp
    SwapchainTest.cpp
    RenderLoopTest.cpp
    BufferTest.cpp
    ShaderTest.cpp
    PipelineTest.cpp
    DescriptorSetTest.cpp
    CameraTest.cpp
    BasicShaderTest.cpp
    CubeRenderTest.cpp
    MeshBufferTest.cpp
    RenderModeTest.cpp
    HighlightTest.cpp
    LightingTest.cpp
    MsaaTest.cpp
    ShadowPassTest.cpp
    ViewportNavigatorTest.cpp
    FirstPersonControllerTest.cpp
    ClipPlaneTest.cpp
    ClipPlaneManagerTest.cpp
    SectionFillPipelineTest.cpp
    DiskMarkerPipelineTest.cpp
    DiskMarkerTest.cpp
    TonemapTest.cpp
    TonemapPipelineTest.cpp
    OctNormalTest.cpp
    LinearDepthTest.cpp
    DepthLinearizePipelineTest.cpp
    DepthMipPipelineTest.cpp
    SsaoKernelTest.cpp
    SsaoBlurPipelineTest.cpp
    XeGtaoMathTest.cpp
    SsaoXeGtaoPipelineTest.cpp
    OutlineEdgeTest.cpp
    OutlinePipelineTest.cpp
    SsilMathTest.cpp
    SsilPipelineTest.cpp
    SsilBlurPipelineTest.cpp
    SmaaMathTest.cpp
    SmaaLutTest.cpp
    SmaaEdgePipelineTest.cpp
    SmaaWeightsPipelineTest.cpp
    SmaaBlendPipelineTest.cpp
    FogMathTest.cpp
)
target_link_libraries(renderer_tests PRIVATE bimeup_renderer bimeup_platform GTest::gtest_main)
target_compile_definitions(renderer_tests PRIVATE BIMEUP_SHADER_DIR="${BIMEUP_SHADER_OUTPUT_DIR}")
bimeup_enable_sanitizers(renderer_tests)

# Gtest suites that don't touch Vulkan/GLFW — pure CPU/math, safe to fan out
# wide with `ctest -j`. Discovery splits the binary into per-test ctest entries
# so `ctest -L cpu`, `ctest -L gpu`, or `ctest -R Smaa` all work.
set(_renderer_cpu_filter
    "CameraTest.*:YawPitchFromForwardTest.*:ClipPlaneTest.*:ClipPlaneManagerTest.*:ClipPlanesUboTest.*:DepthLinearizePipelinePushConstants.*:DiskMarkerTest.*:FirstPersonControllerTest.*:FogMathTest.*:HemisphereAmbientTest.*:HighlightTest.*:LightingTest.*:LinearDepthTest.*:MsaaClampTest.*:OctNormalTest.*:OutlineEdgeTest.*:OutlinePipelinePushConstants.*:PcfShadowTest.*:RenderModeTest.*:ShadowPassTest.*:SmaaBlendPipelinePushConstants.*:SmaaEdgePipelinePushConstants.*:SmaaLutTest.*:SmaaMathTest.*:SmaaWeightsPipelinePushConstants.*:SsaoBlurPipelinePushConstants.*:SsaoKernelTest.*:SsaoXeGtaoPipelinePushConstants.*:SsilBlurPipelinePushConstants.*:SsilMathTest.*:SsilPipelinePushConstants.*:TonemapPushConstantsTest.*:TonemapTest.*:ViewportNavigatorTest.*:XeGtaoMathTest.*")

set(_renderer_test_env
    "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/asan_suppressions.txt")

include(GoogleTest)

gtest_discover_tests(renderer_tests
    TEST_PREFIX "cpu."
    TEST_FILTER "${_renderer_cpu_filter}"
    PROPERTIES
        LABELS "renderer\;cpu"
        ENVIRONMENT "${_renderer_test_env}"
)

gtest_discover_tests(renderer_tests
    TEST_PREFIX "gpu."
    TEST_FILTER "-${_renderer_cpu_filter}"
    PROPERTIES
        LABELS "renderer\;gpu"
        ENVIRONMENT "${_renderer_test_env}"
)
