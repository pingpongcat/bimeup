add_executable(tools_tests LogTest.cpp ConfigTest.cpp)
target_link_libraries(tools_tests PRIVATE bimeup_tools GTest::gtest_main)
add_test(NAME tools COMMAND tools_tests)
