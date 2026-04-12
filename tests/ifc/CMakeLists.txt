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

add_executable(test_ifc_model
    test_ifc_model.cpp
)

target_link_libraries(test_ifc_model PRIVATE
    GTest::gtest_main
    bimeup_ifc
)

bimeup_enable_sanitizers(test_ifc_model)

target_compile_definitions(test_ifc_model PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/ifc/data"
)

gtest_discover_tests(test_ifc_model)

add_executable(test_ifc_element
    test_ifc_element.cpp
)

target_link_libraries(test_ifc_element PRIVATE
    GTest::gtest_main
    bimeup_ifc
)

bimeup_enable_sanitizers(test_ifc_element)

target_compile_definitions(test_ifc_element PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/ifc/data"
)

gtest_discover_tests(test_ifc_element)
