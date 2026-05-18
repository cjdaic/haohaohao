add_library(nbcam_job STATIC
    laser_job.h
    laser_job.cpp
    serializer.h
    serializer.cpp
    json_serializer.h
    json_serializer.cpp
)

target_link_libraries(nbcam_job PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

target_include_directories(nbcam_job PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
