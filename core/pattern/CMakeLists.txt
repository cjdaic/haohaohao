add_library(nbcam_pattern STATIC
    fill_strategy_interface.h
    contour_strategy.h
    contour_strategy.cpp
    hatch_strategy.h
    hatch_strategy.cpp
    line_hatch_strategy.h
    line_hatch_strategy.cpp
    ring_fill_strategy.h
    ring_fill_strategy.cpp
    nanosvg_wrapper.h
    nanosvg_wrapper.cpp
    svg_to_uv_mapper.h
    svg_to_uv_mapper.cpp
)

target_link_libraries(nbcam_pattern PUBLIC
    spdlog::spdlog
)

# nanosvg是header-only库，需要添加包含目录
# 尝试多种方式找到nanosvg的头文件路径
if(TARGET nanosvg::nanosvg)
    # 方式1: 如果提供了命名空间目标（CMake 3.x Config模式）
    target_link_libraries(nbcam_pattern PUBLIC nanosvg::nanosvg)
elseif(TARGET nanosvg)
    # 方式2: 如果没有命名空间的目标
    target_link_libraries(nbcam_pattern PUBLIC nanosvg)
elseif(DEFINED nanosvg_INCLUDE_DIRS)
    # 方式3: 如果find_package提供了包含目录变量
    target_include_directories(nbcam_pattern PUBLIC ${nanosvg_INCLUDE_DIRS})
elseif(DEFINED nanosvg_INCLUDE_DIR)
    # 方式4: 单数形式的变量
    target_include_directories(nbcam_pattern PUBLIC ${nanosvg_INCLUDE_DIR})
else()
    # 方式5: 使用find_path查找nanosvg.h
    find_path(NANOSVG_INCLUDE_DIR nanosvg.h
        PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/*/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/*/include/nanosvg
        ${CMAKE_SOURCE_DIR}/../vcpkg_installed/*/include
        ${CMAKE_SOURCE_DIR}/../vcpkg_installed/*/include/nanosvg
        D:/vcpkg/installed/x64-windows/include
        D:/vcpkg/installed/x64-windows/include/nanosvg
        ENV VCPKG_ROOT
        PATH_SUFFIXES include include/nanosvg nanosvg
    )
    if(NANOSVG_INCLUDE_DIR)
        target_include_directories(nbcam_pattern PUBLIC ${NANOSVG_INCLUDE_DIR})
    else()
        message(WARNING "Could not find nanosvg headers. Please ensure nanosvg is installed via vcpkg.")
    endif()
endif()

target_include_directories(nbcam_pattern PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
