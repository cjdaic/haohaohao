add_library(nbcam_planner STATIC
    path_planner.h
    path_planner.cpp
)

target_link_libraries(nbcam_planner PUBLIC
    nbcam_job
    spdlog::spdlog
)

target_include_directories(nbcam_planner PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
