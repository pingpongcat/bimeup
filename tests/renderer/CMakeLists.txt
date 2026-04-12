add_executable(renderer_tests
    VulkanContextTest.cpp
    DeviceTest.cpp
)
target_link_libraries(renderer_tests PRIVATE bimeup_renderer GTest::gtest_main)
bimeup_enable_sanitizers(renderer_tests)
add_test(NAME renderer COMMAND renderer_tests)
set_tests_properties(renderer PROPERTIES
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/asan_suppressions.txt"
)
