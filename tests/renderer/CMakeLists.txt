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
    SsaoPipelineTest.cpp
    SsaoBlurPipelineTest.cpp
    OutlineEdgeTest.cpp
    OutlinePipelineTest.cpp
)
target_link_libraries(renderer_tests PRIVATE bimeup_renderer bimeup_platform GTest::gtest_main)
target_compile_definitions(renderer_tests PRIVATE BIMEUP_SHADER_DIR="${BIMEUP_SHADER_OUTPUT_DIR}")
bimeup_enable_sanitizers(renderer_tests)
add_test(NAME renderer COMMAND renderer_tests)
set_tests_properties(renderer PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/asan_suppressions.txt"
)
