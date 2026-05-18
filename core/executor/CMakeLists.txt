add_library(nbcam_executor STATIC
    data_buffer.h
    data_buffer.cpp
    data_generator.h
    data_generator.cpp
    frame_log_utils.h
    frame_log_utils.cpp
    executor_interface.h
    board_executor.h
    board_executor.cpp
    rtc6_executor.h
    rtc6_executor.cpp
    tcp_socket.h
    tcp_socket.cpp
)

target_link_libraries(nbcam_executor PUBLIC
    nbcam_job
    Threads::Threads
    spdlog::spdlog
)

target_include_directories(nbcam_executor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# RTC6库链接（如果存在）
if(EXISTS "${CMAKE_SOURCE_DIR}/RTC6/RTC6DLLx64.lib")
    target_link_libraries(nbcam_executor PRIVATE
        "${CMAKE_SOURCE_DIR}/RTC6/RTC6DLLx64.lib"
    )
    target_include_directories(nbcam_executor PRIVATE
        "${CMAKE_SOURCE_DIR}/RTC6"
    )
endif()
