cmake_minimum_required(VERSION 3.20)
project(bimeup VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BIMEUP_BUILD_TESTS "Build unit tests" OFF)
option(BIMEUP_ENABLE_VR "Enable OpenXR VR support" OFF)
option(BIMEUP_ENABLE_RAYTRACING "Enable Vulkan ray tracing" OFF)

# Debug builds: swap the CMake default `-O0 -g` for `-Og -g`. `-Og` preserves
# nearly all debuggability (no heavy inlining, locals stay in scope) while
# folding trivial accessors / constants — typical dev iteration runs several
# times faster on a Vulkan/ImGui frame with no practical cost to stepping.
# MSVC's Debug config ships `/RTC1` runtime-checks which conflict with
# optimization, so we leave it alone there.
if(NOT MSVC)
    set(CMAKE_C_FLAGS_DEBUG "-Og -g"
        CACHE STRING "Flags used by the C compiler during DEBUG builds." FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "-Og -g"
        CACHE STRING "Flags used by the CXX compiler during DEBUG builds." FORCE)
endif()

# CMake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(Sanitizers)
include(CompileShaders)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# External dependencies
add_subdirectory(external)

# Shaders — paths set BEFORE add_subdirectory so every module that bakes
# BIMEUP_SHADER_DIR into its compile defs (bimeup_core, bimeup app, the
# renderer tests) sees a non-empty path. Previously src/core was added
# before these set() calls, so `-DBIMEUP_SHADER_DIR=""` landed in
# bimeup_core — Application::Application threw on first SPV load in
# headless tests, leaking ctor-state VMA allocations on unwind.
set(BIMEUP_SHADER_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(BIMEUP_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
bimeup_compile_shaders(bimeup_shaders "${BIMEUP_SHADER_DIR}" "${BIMEUP_SHADER_OUTPUT_DIR}")

# Modules
add_subdirectory(src/tools)
add_subdirectory(src/platform)
add_subdirectory(src/renderer)
add_subdirectory(src/ifc)
add_subdirectory(src/scene)
add_subdirectory(src/core)
add_subdirectory(src/ui)

# Application
add_subdirectory(src/app)

# Tests
if(BIMEUP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
