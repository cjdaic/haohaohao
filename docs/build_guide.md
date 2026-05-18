# NBCAM 构建指南

## 前置要求

1. **Windows 10/11**
2. **Visual Studio 2019/2022** (MSVC编译器)
3. **CMake 3.20+**
4. **Qt 6.10.1** (已安装到 D:\Qt\6.10.1\msvc2022_64)
5. **VTK 9.4.2** (已编译到 E:\VTK-9.4.2\install)
6. **vcpkg** (用于管理CGAL、gRPC等依赖)

## 步骤1: 安装vcpkg依赖

```powershell
# 设置vcpkg环境变量（如果未设置）
$env:VCPKG_ROOT = "D:\vcpkg"  # 根据实际路径修改

# 安装依赖
vcpkg install cgal:x64-windows
vcpkg install grpc:x64-windows
vcpkg install protobuf:x64-windows
vcpkg install nlohmann-json:x64-windows
vcpkg install spdlog:x64-windows
```

## 步骤2: 配置CMake

```powershell
# 推荐：使用预设，默认就是Release
cmake --preset release
```

如果不使用预设，也可以手动配置：

```powershell
mkdir build
cd build

cmake .. `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="D:/Qt/6.10.1/msvc2022_64" `
    -DVTK_DIR="E:/VTK-9.4.2/install"

# 或者使用CMake GUI配置
```

## 步骤3: 生成protobuf代码

```powershell
# 生成protobuf和gRPC代码
protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=grpc_cpp_plugin.exe `
    ..\core\executor\five_axis.proto
```

注意：需要将生成的`.pb.h`和`.pb.cc`文件添加到CMakeLists.txt中。

## 步骤4: 编译

```powershell
# 推荐：使用Release预设编译
cmake --build --preset release

# 或者在build目录下显式编Release
cmake --build . --config Release

# 或者使用Visual Studio打开生成的.sln文件进行编译
```

## 步骤5: 运行

编译完成后，可执行文件位于 `build/bin/Release/laser_cam_qt.exe`

## 常见问题

### 1. Qt未找到
- 检查CMAKE_PREFIX_PATH是否正确设置
- 确认Qt版本为6.10.1

### 2. VTK未找到
- 检查VTK_DIR路径是否正确
- 确认VTK库文件已正确编译

### 3. vcpkg依赖未找到
- 检查CMAKE_TOOLCHAIN_FILE路径
- 确认VCPKG_ROOT环境变量已设置
- 运行 `vcpkg integrate install`

### 4. protobuf代码生成失败
- 确认protoc和grpc_cpp_plugin在PATH中
- 或手动指定完整路径

## 开发模式

对于开发调试，可以使用Debug模式：

```powershell
cmake --preset debug
cmake --build --preset debug
```

## 下一步

完成构建后，参考以下文档：
- `docs/architecture.md` - 系统架构说明
- `prompt.txt` - 项目需求文档
- `README.md` - 项目概述
