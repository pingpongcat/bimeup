add_library(bimeup_tools STATIC
    src/Log.cpp
    src/Config.cpp
    src/Timer.cpp
)

target_include_directories(bimeup_tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bimeup_tools PUBLIC spdlog::spdlog)
bimeup_enable_sanitizers(bimeup_tools)
