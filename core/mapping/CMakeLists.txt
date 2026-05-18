add_library(nbcam_mapping STATIC
    uv_mapper.h
    uv_mapper.cpp
)

target_link_libraries(nbcam_mapping PUBLIC
    nbcam_mesh
    nbcam_param
    nbcam_pattern
    CGAL::CGAL
    spdlog::spdlog
)

target_include_directories(nbcam_mapping PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
