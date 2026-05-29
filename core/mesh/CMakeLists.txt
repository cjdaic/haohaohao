add_library(nbcam_mesh STATIC
    mesh_io.h
    mesh_io.cpp
    mesh_preprocessor.h
    mesh_preprocessor.cpp
    patch_clusterer.h
    patch_clusterer.cpp
    mesh_accelerator.h
    mesh_accelerator.cpp
)

target_link_libraries(nbcam_mesh PUBLIC
    CGAL::CGAL
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(nbcam_mesh PRIVATE
        ole32
        oleaut32
    )
endif()

target_include_directories(nbcam_mesh PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
