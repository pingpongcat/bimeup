add_executable(test_core
    SceneUploaderTest.cpp
)

target_link_libraries(test_core PRIVATE
    GTest::gtest_main
    bimeup_core
)

bimeup_enable_sanitizers(test_core)

include(GoogleTest)
gtest_discover_tests(test_core
    PROPERTIES ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/asan_suppressions.txt"
)
