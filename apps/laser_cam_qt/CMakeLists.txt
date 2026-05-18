cmake_minimum_required(VERSION 3.20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找Qt6组件
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL Network)

# 查找VTK组件
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    RenderingCore
    RenderingOpenGL2
    InteractionStyle
    GUISupportQt
)

# 创建可执行文件
add_executable(laser_cam_qt
    main.cpp
    app_resources.qrc
    laser_cam_qt.rc
    mainwindow.h
    mainwindow.cpp
    mainwindow.ui
    application_controller.h
    application_controller.cpp
    model_view.h
    model_view.cpp
    uv_view.h
    uv_view.cpp
    path_preview.h
    path_preview.cpp
    parameter_panel.h
    parameter_panel.cpp
    grpc_client.h
    grpc_client.cpp
    plane_pose_widget.h
    plane_pose_widget.cpp
    texture_editor_dialog.h
    texture_editor_dialog.cpp
    texture_list_widget.h
    texture_list_widget.cpp
    texture_details_dialog.h
    texture_details_dialog.cpp
    svg_raster_cache.h
    svg_raster_cache.cpp
)

# Some Qt installations/licensing setups can fail intermittently when AutoMoc
# spawns multiple moc processes in parallel on Windows. Keep autogen serial
# for this target to improve build stability.
set_property(TARGET laser_cam_qt PROPERTY AUTOGEN_PARALLEL 1)

target_link_libraries(laser_cam_qt PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::Network
    nbcam_job
    nbcam_mesh
    nbcam_param
    nbcam_pattern
    nbcam_mapping
    nbcam_process
    nbcam_planner
    nbcam_executor
    VTK::CommonCore
    VTK::RenderingCore
    VTK::RenderingOpenGL2
    VTK::InteractionStyle
    VTK::GUISupportQt
    spdlog::spdlog
    Clipper2::Clipper2
)

target_include_directories(laser_cam_qt PRIVATE
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/apps
)

# 自动复制运行时依赖到可执行文件目录，避免配置切换时加载到错误版本的DLL。
if(MSVC)
    get_target_property(_qt_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)
    get_filename_component(_qt_prefix_dir "${_qt_bin_dir}" DIRECTORY)
    set(_qt_platform_plugin "${_qt_prefix_dir}/plugins/platforms/qwindows.dll")

    add_custom_command(TARGET laser_cam_qt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:laser_cam_qt>
            $<TARGET_FILE_DIR:laser_cam_qt>
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:laser_cam_qt>/platforms
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_qt_platform_plugin}"
            $<TARGET_FILE_DIR:laser_cam_qt>/platforms
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/qt.conf"
            $<TARGET_FILE_DIR:laser_cam_qt>/qt.conf
        COMMAND_EXPAND_LISTS
    )

    if(EXISTS "${CMAKE_SOURCE_DIR}/RTC6/RTC6DLLx64.dll")
        add_custom_command(TARGET laser_cam_qt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/RTC6/RTC6DLLx64.dll"
                $<TARGET_FILE_DIR:laser_cam_qt>
        )
    endif()
endif()
