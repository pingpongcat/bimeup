add_executable(tools_tests LogTest.cpp ConfigTest.cpp TimerTest.cpp CliArgsTest.cpp)
target_link_libraries(tools_tests PRIVATE bimeup_tools GTest::gtest_main)
bimeup_enable_sanitizers(tools_tests)
add_test(NAME tools COMMAND tools_tests)
