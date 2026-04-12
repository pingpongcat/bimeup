add_executable(platform_tests WindowTest.cpp)
target_link_libraries(platform_tests PRIVATE bimeup_platform GTest::gtest_main)
bimeup_enable_sanitizers(platform_tests)
add_test(NAME platform COMMAND platform_tests)
set_tests_properties(platform PROPERTIES
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/asan_suppressions.txt"
)
