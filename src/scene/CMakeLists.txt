add_library(bimeup_scene STATIC
    src/Scene.cpp
    src/SceneMesh.cpp
    src/SceneBuilder.cpp
    src/Raycast.cpp
    src/Measurement.cpp
    src/Snap.cpp
    src/Slicing.cpp
    src/SectionCapGeometry.cpp
    src/AxisSectionController.cpp
)

target_include_directories(bimeup_scene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bimeup_scene PUBLIC glm::glm bimeup_ifc bimeup_renderer)
target_link_libraries(bimeup_scene PRIVATE poly2tri)
bimeup_enable_sanitizers(bimeup_scene)
