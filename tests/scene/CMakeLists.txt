add_executable(test_scene
    test_scene.cpp
    test_scene_mesh.cpp
    test_aabb.cpp
    test_batching.cpp
)

target_link_libraries(test_scene PRIVATE
    GTest::gtest_main
    bimeup_scene
)

bimeup_enable_sanitizers(test_scene)

include(GoogleTest)
gtest_discover_tests(test_scene)

add_executable(test_scene_builder
    test_scene_builder.cpp
)

target_link_libraries(test_scene_builder PRIVATE
    GTest::gtest_main
    bimeup_scene
    bimeup_ifc
)

bimeup_enable_sanitizers(test_scene_builder)

target_compile_definitions(test_scene_builder PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/ifc/data"
)

set(IFC_LSAN_SUPPRESSIONS "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tests/lsan_suppressions.txt")

gtest_discover_tests(test_scene_builder
    PROPERTIES ENVIRONMENT "${IFC_LSAN_SUPPRESSIONS}"
)
