add_executable(test_scene
    test_scene.cpp
    test_scene_mesh.cpp
    test_aabb.cpp
)

target_link_libraries(test_scene PRIVATE
    GTest::gtest_main
    bimeup_scene
)

bimeup_enable_sanitizers(test_scene)

include(GoogleTest)
gtest_discover_tests(test_scene)
