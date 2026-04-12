add_executable(test_ifc
    test_web_ifc_integration.cpp
)

target_link_libraries(test_ifc PRIVATE
    GTest::gtest_main
    web-ifc-lib
)

target_compile_definitions(test_ifc PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/ifc/data"
)

include(GoogleTest)
gtest_discover_tests(test_ifc)
