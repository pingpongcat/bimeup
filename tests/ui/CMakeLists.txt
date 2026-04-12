add_executable(ui_tests
    ImGuiContextTest.cpp
    PanelTest.cpp
    UIManagerTest.cpp
)
target_link_libraries(ui_tests PRIVATE bimeup_ui GTest::gtest_main)
bimeup_enable_sanitizers(ui_tests)
add_test(NAME ui COMMAND ui_tests)
set_tests_properties(ui PROPERTIES
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/asan_suppressions.txt"
)
