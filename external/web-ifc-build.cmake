# Build web-ifc C++ core as a static library
# Fetches header-only dependencies not already provided by the project

include(FetchContent)

set(WEBIFC_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/web-ifc/src/cpp)

# --- Header-only dependencies needed by web-ifc ---

FetchContent_Declare(fastfloat
    GIT_REPOSITORY https://github.com/fastfloat/fast_float
    GIT_TAG v8.0.2
    SOURCE_SUBDIR "../"
)

FetchContent_Declare(tinynurbs
    GIT_REPOSITORY https://github.com/QuimMoya/tinynurbs
    GIT_TAG 47115cd9b6e922b27bbc4ab01fdeac2e9ea597a4
    SOURCE_SUBDIR "../"
)

FetchContent_Declare(earcut
    GIT_REPOSITORY https://github.com/mapbox/earcut.hpp
    GIT_TAG v2.2.4
    SOURCE_SUBDIR "../"
)

FetchContent_Declare(cdt
    GIT_REPOSITORY https://github.com/artem-ogre/CDT
    GIT_TAG 4d0c9026b8ec846fe544897e7111f8f9080d5f8a
    SOURCE_SUBDIR "../"
)

FetchContent_Declare(stduuid
    GIT_REPOSITORY https://github.com/mariusbancila/stduuid
    GIT_TAG 3afe7193facd5d674de709fccc44d5055e144d7a
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(fastfloat tinynurbs earcut cdt stduuid)

# --- Collect web-ifc source files ---

file(GLOB_RECURSE WEBIFC_SOURCES ${WEBIFC_SRC_DIR}/web-ifc/*.cpp)

# --- Build static library ---

add_library(web-ifc-lib STATIC ${WEBIFC_SOURCES})

target_include_directories(web-ifc-lib PUBLIC
    ${WEBIFC_SRC_DIR}
)

# These are PUBLIC because web-ifc headers transitively include them
target_include_directories(web-ifc-lib PUBLIC
    ${fastfloat_SOURCE_DIR}/include
    ${tinynurbs_SOURCE_DIR}/include
    ${earcut_SOURCE_DIR}/include
    ${cdt_SOURCE_DIR}/CDT/include
    ${stduuid_SOURCE_DIR}/include
)

target_link_libraries(web-ifc-lib PUBLIC
    spdlog::spdlog
    glm::glm
)

target_compile_features(web-ifc-lib PUBLIC cxx_std_20)

# Suppress warnings from third-party code
target_compile_options(web-ifc-lib PRIVATE
    -w
)
