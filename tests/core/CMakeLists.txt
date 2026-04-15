add_executable(test_core
    ApplicationTest.cpp
    EventBusTest.cpp
    EventsTest.cpp
    PickingTest.cpp
    SceneUploaderTest.cpp
    SelectionTest.cpp
)

target_link_libraries(test_core PRIVATE
    GTest::gtest_main
    bimeup_core
)

bimeup_enable_sanitizers(test_core)

include(GoogleTest)
# test_core contains both pure-logic tests (EventBus/Selection/Picking) and
# Vulkan-touching integration tests (Application/SceneUploaderVulkan). The
# Vulkan ones accumulate unavoidable driver leaks at process exit, so LSan is
# disabled for this binary as a whole. Runtime ASan checks (UAF/OOB) remain
# active.
gtest_discover_tests(test_core
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/asan_suppressions.txt"
)
