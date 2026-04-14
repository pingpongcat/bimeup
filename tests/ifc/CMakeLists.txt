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

set(IFC_LSAN_SUPPRESSIONS "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/asan_suppressions.txt")

gtest_discover_tests(test_ifc
    PROPERTIES ENVIRONMENT "${IFC_LSAN_SUPPRESSIONS}"
)

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

gtest_discover_tests(test_ifc_model
    PROPERTIES ENVIRONMENT "${IFC_LSAN_SUPPRESSIONS}"
)

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

gtest_discover_tests(test_ifc_element
    PROPERTIES ENVIRONMENT "${IFC_LSAN_SUPPRESSIONS}"
)

add_executable(test_ifc_geometry_extractor
    test_ifc_geometry_extractor.cpp
)

target_link_libraries(test_ifc_geometry_extractor PRIVATE
    GTest::gtest_main
    bimeup_ifc
)

bimeup_enable_sanitizers(test_ifc_geometry_extractor)

target_compile_definitions(test_ifc_geometry_extractor PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/ifc/data"
)

gtest_discover_tests(test_ifc_geometry_extractor
    PROPERTIES ENVIRONMENT "${IFC_LSAN_SUPPRESSIONS}"
)

add_executable(test_ifc_hierarchy
    test_ifc_hierarchy.cpp
)

target_link_libraries(test_ifc_hierarchy PRIVATE
    GTest::gtest_main
    bimeup_ifc
)

bimeup_enable_sanitizers(test_ifc_hierarchy)

target_compile_definitions(test_ifc_hierarchy PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/ifc/data"
)

gtest_discover_tests(test_ifc_hierarchy
    PROPERTIES ENVIRONMENT "${IFC_LSAN_SUPPRESSIONS}"
)
