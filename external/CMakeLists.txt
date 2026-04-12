# External dependencies — added as git submodules

# spdlog (header-only mode)
add_subdirectory(spdlog)

# glm
add_subdirectory(glm)

# googletest (only when tests are enabled)
if(BIMEUP_BUILD_TESTS)
    add_subdirectory(googletest)
endif()
