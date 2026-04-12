# External dependencies — added as git submodules

# spdlog (header-only mode)
add_subdirectory(spdlog)

# glm
add_subdirectory(glm)

# GLFW (no docs/tests/examples)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

# googletest (only when tests are enabled)
if(BIMEUP_BUILD_TESTS)
    add_subdirectory(googletest)
endif()
