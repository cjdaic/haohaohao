add_library(nbcam_process STATIC
    process_model_interface.h
    curvature_model.h
    curvature_model.cpp
)

target_link_libraries(nbcam_process PUBLIC
    nbcam_mesh
    spdlog::spdlog
)

target_include_directories(nbcam_process PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
