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
    HighlightTest.cpp
    LightingTest.cpp
    ShadowPassTest.cpp
    ShadowTransmissionPipelineTest.cpp
    ViewportNavigatorTest.cpp
    FirstPersonControllerTest.cpp
    ClipPlaneTest.cpp
    ClipPlaneManagerTest.cpp
    SectionFillPipelineTest.cpp
    EdgeOverlayPipelineTest.cpp
    DiskMarkerPipelineTest.cpp
    DiskMarkerTest.cpp
    TonemapTest.cpp
    TonemapPipelineTest.cpp
    OctNormalTest.cpp
    LinearDepthTest.cpp
    DepthLinearizePipelineTest.cpp
    DepthMipPipelineTest.cpp
    SsaoBlurPipelineTest.cpp
    XeGtaoMathTest.cpp
    SsaoXeGtaoPipelineTest.cpp
    SmaaMathTest.cpp
    SmaaLutTest.cpp
    SmaaEdgePipelineTest.cpp
    SmaaWeightsPipelineTest.cpp
    SmaaBlendPipelineTest.cpp
    SunPositionTest.cpp
    SkyColorTest.cpp
    SunLightingTest.cpp
    AccelerationStructureTest.cpp
    TopLevelASTest.cpp
)
target_link_libraries(renderer_tests PRIVATE bimeup_renderer bimeup_platform GTest::gtest_main)
target_compile_definitions(renderer_tests PRIVATE BIMEUP_SHADER_DIR="${BIMEUP_SHADER_OUTPUT_DIR}")
bimeup_enable_sanitizers(renderer_tests)

set(_renderer_test_env
    "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/asan_suppressions.txt")

# CPU/math gtest suites — no Vulkan/GLFW. Discovery splits the binary into
# per-test ctest entries so they fan out wide under `ctest -j` and individual
# tests show up in `ctest -R`. Suite list mirrors `BIMEUP_RENDERER_CPU_FIXTURES`
# below in negated form so any new GPU suite is automatically picked up there.
set(_renderer_cpu_filter
    "CameraTest.*:YawPitchFromForwardTest.*:ClipPlaneTest.*:ClipPlaneManagerTest.*:ClipPlanesUboTest.*:DepthLinearizePipelinePushConstants.*:DiskMarkerTest.*:FirstPersonControllerTest.*:HemisphereAmbientTest.*:HighlightTest.*:LightingTest.*:LinearDepthTest.*:OctNormalTest.*:PcfShadowTest.*:ShadowPassTest.*:SmaaBlendPipelinePushConstants.*:SmaaEdgePipelinePushConstants.*:SmaaLutTest.*:SmaaMathTest.*:SmaaWeightsPipelinePushConstants.*:SsaoBlurPipelinePushConstants.*:SkyColorTest.*:SsaoXeGtaoPipelinePushConstants.*:SunLightingTest.*:SunPositionTest.*:TonemapPushConstantsTest.*:TonemapTest.*:ViewportNavigatorTest.*:XeGtaoMathTest.*")

include(GoogleTest)

gtest_discover_tests(renderer_tests
    TEST_PREFIX "cpu."
    TEST_FILTER "${_renderer_cpu_filter}"
    PROPERTIES
        LABELS "renderer\;cpu"
        ENVIRONMENT "${_renderer_test_env}"
)

# GPU suites are registered ONE ctest entry per fixture (not per test) so the
# binary runs each fixture's tests in a single process. That lets fixtures
# share their VulkanContext+Device+Swapchain via SetUpTestSuite — the dominant
# cost — while still parallelising across fixtures under `ctest -j`. Per-test
# discovery would force a fresh process (and a fresh Vulkan setup) per test.
set(_renderer_gpu_fixtures
    VulkanContextTest
    DeviceTest
    SwapchainTest
    RenderLoopTest
    BufferTest
    ShaderTest
    PipelineTest
    DescriptorSetTest
    BasicShaderTest
    CubeRenderTest
    MeshBufferTest
    ShadowMapTest
    ShadowTransmissionPipelineTest
    SectionFillPipelineTest
    EdgeOverlayPipelineTest
    DiskMarkerPipelineTest
    TonemapPipelineTest
    DepthLinearizePipelineTest
    DepthMipPipelineTest
    SsaoBlurPipelineTest
    SsaoXeGtaoPipelineTest
    SmaaEdgePipelineTest
    SmaaWeightsPipelineTest
    SmaaBlendPipelineTest
    AccelerationStructureTest
    TopLevelASTest
)

foreach(_fixture IN LISTS _renderer_gpu_fixtures)
    add_test(
        NAME "gpu.${_fixture}"
        COMMAND renderer_tests "--gtest_filter=${_fixture}.*"
    )
    set_tests_properties("gpu.${_fixture}" PROPERTIES
        LABELS "renderer;gpu"
        ENVIRONMENT "${_renderer_test_env}"
    )
endforeach()
