add_library(nbcam_param STATIC
    parameterizer_interface.h
    cgal_mesh_utils.h
    lscm_parameterizer.h
    lscm_parameterizer.cpp
    abf_parameterizer.h
    abf_parameterizer.cpp
    arap_parameterizer.h
    arap_parameterizer.cpp
    authalic_parameterizer.h
    authalic_parameterizer.cpp
    patch_parameterizer.h
    patch_parameterizer.cpp
)

target_link_libraries(nbcam_param PUBLIC
    CGAL::CGAL
    Eigen3::Eigen
    spdlog::spdlog
)

# CGAL的Surface_mesh_parameterization和Polygon_mesh_processing通常包含在CGAL::CGAL中
# 如果链接失败，可能需要单独链接：
# target_link_libraries(nbcam_param PUBLIC CGAL::Surface_mesh_parameterization CGAL::Polygon_mesh_processing)

target_include_directories(nbcam_param PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core/mesh
)
