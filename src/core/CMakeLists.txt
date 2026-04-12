add_library(bimeup_core STATIC
    src/Application.cpp
    src/Picking.cpp
    src/SceneUploader.cpp
    src/Selection.cpp
)

target_include_directories(bimeup_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bimeup_core PUBLIC
    bimeup_platform
    bimeup_scene
    bimeup_renderer
    bimeup_tools
)
bimeup_enable_sanitizers(bimeup_core)
