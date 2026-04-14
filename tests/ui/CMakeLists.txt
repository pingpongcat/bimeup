add_executable(ui_tests
    ClipPlanesPanelTest.cpp
    HierarchyPanelTest.cpp
    ImGuiContextTest.cpp
    ImGuizmoIntegrationTest.cpp
    PanelTest.cpp
    PlanViewPanelTest.cpp
    PropertyPanelTest.cpp
    RenderQualityPanelTest.cpp
    SelectionBridgeTest.cpp
    ThemeTest.cpp
    ToolbarTest.cpp
    UIManagerTest.cpp
    ViewportOverlayTest.cpp
)
target_link_libraries(ui_tests PRIVATE bimeup_ui bimeup_ifc bimeup_renderer bimeup_core GTest::gtest_main)
bimeup_enable_sanitizers(ui_tests)
add_test(NAME ui COMMAND ui_tests)
set_tests_properties(ui PROPERTIES
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/asan_suppressions.txt"
)
