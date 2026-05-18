cmake_minimum_required(VERSION 3.20)
project(NBCAM VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 将所有非Release配置映射到第三方库的Release配置，避免误拉取Debug DLL。
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release)

# 默认优先使用Release；仍保留Debug配置，便于VS直接调试。
# 对VS等多配置生成器，将Release放在首位，降低误编到Debug的概率。
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;Debug" CACHE STRING "Build configurations" FORCE)
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
endif()

if(MSVC)
    # 第三方依赖当前按Release ABI提供；Debug配置也统一到/MD和非调试迭代器，
    # 避免与Release导入库产生LNK2038。
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Qt路径
set(QT_DIR "D:/Qt/6.10.1/msvc2022_64" CACHE PATH "Qt installation directory")
list(PREPEND CMAKE_PREFIX_PATH "${QT_DIR}")

# VTK路径
set(VTK_DIR "E:/VTK-9.4.2/install" CACHE PATH "VTK installation directory")

# vcpkg工具链
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# 查找依赖
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL)
find_package(VTK REQUIRED)
find_package(CGAL REQUIRED)
find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

# vcpkg管理的依赖
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(nanosvg CONFIG REQUIRED)

if(NOT Clipper2_DIR OR Clipper2_DIR MATCHES "NOTFOUND")
    set(_clipper2_config_hints
        "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/share/clipper2"
        "D:/vcpkg/installed/x64-windows/share/clipper2")
    if(DEFINED ENV{VCPKG_ROOT})
        list(APPEND _clipper2_config_hints "$ENV{VCPKG_ROOT}/installed/x64-windows/share/clipper2")
    endif()
    foreach(_clipper2_hint IN LISTS _clipper2_config_hints)
        if(EXISTS "${_clipper2_hint}/Clipper2Config.cmake")
            set(Clipper2_DIR "${_clipper2_hint}" CACHE PATH "Clipper2 CMake config directory" FORCE)
            break()
        endif()
    endforeach()
endif()
find_package(Clipper2 CONFIG REQUIRED)

# 某些包在多配置生成器下会优先挑选Debug导入库，这里显式覆盖到Release。
if(TARGET spdlog::spdlog)
    set_property(TARGET spdlog::spdlog PROPERTY MAP_IMPORTED_CONFIG_DEBUG Release)
    set_property(TARGET spdlog::spdlog PROPERTY MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
endif()
if(TARGET fmt::fmt)
    set_property(TARGET fmt::fmt PROPERTY MAP_IMPORTED_CONFIG_DEBUG Release)
    set_property(TARGET fmt::fmt PROPERTY MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
endif()

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/apps
)

# 添加子目录
add_subdirectory(core)
add_subdirectory(apps/laser_cam_qt)

# 安装规则（可选，如果需要安装）
# install(TARGETS laser_cam_qt
#     RUNTIME DESTINATION bin
# )
