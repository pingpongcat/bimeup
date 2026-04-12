add_library(bimeup_core STATIC
    src/SceneUploader.cpp
)

target_include_directories(bimeup_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bimeup_core PUBLIC bimeup_scene bimeup_renderer)
bimeup_enable_sanitizers(bimeup_core)
