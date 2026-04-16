add_library(bimeup_ifc STATIC
    src/IfcModel.cpp
    src/IfcGeometryExtractor.cpp
    src/IfcHierarchy.cpp
    src/AsyncLoader.cpp
)

target_include_directories(bimeup_ifc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bimeup_ifc PUBLIC web-ifc-lib)
bimeup_enable_sanitizers(bimeup_ifc)
