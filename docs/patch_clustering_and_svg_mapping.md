# Patch聚类与SVG映射功能实现文档

## 概述

本文档描述了Patch聚类、参数化和SVG映射功能的完整实现。这些功能允许用户将3D模型分割成平面面片（patches），对选中的patch进行UV参数化，并将SVG图案映射到patch上。

## 功能特性

### 1. CGAL AABB/BVH 空间加速结构

实现了基于CGAL的AABB树用于快速三角形拾取，支持高效的鼠标交互。

**实现文件：**
- `core/mesh/mesh_accelerator.h`
- `core/mesh/mesh_accelerator.cpp`

**主要功能：**
- `build()`: 构建AABB树
- `rayQuery()`: 射线查询，用于鼠标拾取
- `pointQuery()`: 点查询，查找最近的三角形

**使用示例：**
```cpp
nbcam::MeshAccelerator accelerator;
accelerator.build(*mesh);
int tri_idx = accelerator.rayQuery(origin, direction);
```

### 2. Patch聚类

使用二面角（dihedral angle）对相邻三角形进行聚类，将几乎共面的三角形归为同一个patch。

**实现文件：**
- `core/mesh/patch_clusterer.h`
- `core/mesh/patch_clusterer.cpp`

**算法原理：**
1. 计算每个三角形的面积和单位法向量
2. 查找相邻三角形
3. 计算相邻三角形的二面角
4. 使用并查集（Union-Find）数据结构进行聚类
5. 计算每个patch的总面积和面积加权平均法向量
6. 提取patch边界用于高亮显示

**主要方法：**
- `clusterPatches()`: 对网格进行patch聚类
- `getTrianglePatchId()`: 获取指定三角形的patch ID

**参数：**
- `dihedral_threshold`: 二面角阈值（默认0.1弧度），小于此值的相邻三角形归为同一patch

### 3. 鼠标交互

实现了鼠标悬停高亮和点击选择patch的功能。

**实现位置：**
- `apps/laser_cam_qt/model_view.h`
- `apps/laser_cam_qt/model_view.cpp`

**功能：**
- **悬停高亮**：鼠标移动到patch上时，高亮显示patch的边界（红色线条）
- **点击选择**：左键点击patch进行选择，发出`patchSelected`信号

**关键方法：**
- `mouseMoveEvent()`: 处理鼠标移动，实现悬停高亮
- `mousePressEvent()`: 处理鼠标点击，实现patch选择
- `pickTriangle()`: 使用VTK CellPicker拾取鼠标位置的三角形
- `highlightPatch()`: 高亮显示指定patch的边界
- `selectPatch()`: 选择指定patch

### 4. Patch参数化

对选中的patch进行UV参数化，将3D patch映射到2D UV空间。

**实现文件：**
- `core/param/patch_parameterizer.h`
- `core/param/patch_parameterizer.cpp`

**工作流程：**
1. **提取子网格**：从原始网格中提取patch包含的所有三角形和顶点
2. **参数化子网格**：使用LSCM算法对子网格进行UV参数化
3. **映射回原始网格**：将子网格的UV坐标映射回原始网格的顶点

**主要方法：**
- `extractPatchSubmesh()`: 提取patch子网格
- `parameterizePatch()`: 对patch进行参数化
- `mapUVToOriginalMesh()`: 将UV坐标映射回原始网格

**使用示例：**
```cpp
nbcam::PatchParameterizer parameterizer;
ParameterizationResult result = parameterizer.parameterizePatch(*mesh, patch);
```

### 5. SVG映射到Patch

将SVG图案映射到patch的UV空间，并还原到3D模型视图。

**实现位置：**
- `apps/laser_cam_qt/application_controller.h`
- `apps/laser_cam_qt/application_controller.cpp`

**工作流程：**
1. 对选中的patch进行参数化
2. 计算patch的UV边界
3. 使用Nanosvg解析SVG文件
4. 将SVG路径映射到patch的UV空间（居中，90%填充）
5. 通过`mapToXYZ()`将UV路径还原到3D模型

**主要方法：**
- `parameterizePatch()`: 对指定patch进行参数化
- `mapSVGToPatch()`: 将SVG映射到patch

## 用户界面集成

### 菜单项

在`MainWindow`中添加了以下菜单项：

1. **工具 -> Patch聚类(&C)...**
   - 触发patch聚类
   - 使用默认二面角阈值0.1弧度

2. **工具 -> 参数化选中Patch(&P)**
   - 对当前选中的patch进行参数化
   - 需要先选择一个patch

3. **工具 -> 导入SVG到Patch(&S)...**
   - 打开文件对话框选择SVG文件
   - 将SVG映射到当前选中的patch
   - 需要先选择一个patch并进行参数化

### 信号连接

- `ModelView::patchSelected(int patch_id)`: patch选择信号
- `ApplicationController::parameterizationCompleted(bool)`: 参数化完成信号
- `ApplicationController::patternGenerated(bool)`: 图案生成信号

## 使用流程

### 完整工作流程

1. **加载模型**
   ```
   文件 -> 打开模型 -> 选择STL/OBJ文件
   ```

2. **进行Patch聚类**
   ```
   工具 -> Patch聚类
   ```
   - 系统会自动对模型进行patch聚类
   - 状态栏显示聚类结果（patch数量）

3. **选择Patch**
   - 在3D视图中移动鼠标，可以看到patch边界高亮（红色）
   - 左键点击选择patch
   - 状态栏显示选中的patch信息

4. **参数化Patch**
   ```
   工具 -> 参数化选中Patch
   ```
   - 对选中的patch进行UV参数化
   - 参数化完成后，UV视图会显示patch的UV坐标

5. **导入SVG到Patch**
   ```
   工具 -> 导入SVG到Patch -> 选择SVG文件
   ```
   - 系统会自动将SVG映射到patch的UV空间
   - 映射完成后，3D视图会显示SVG路径

## 技术细节

### 数据结构

#### Patch结构
```cpp
struct Patch {
    int id;                                    // Patch ID
    std::vector<size_t> triangle_indices;      // 属于此patch的三角形索引
    double total_area;                         // 总面积
    double normal_x, normal_y, normal_z;       // 面积加权平均法向（归一化）
    std::vector<size_t> boundary_edges;        // 边界边（边的索引对）
};
```

#### MeshAccelerator
```cpp
class MeshAccelerator {
    void build(const TriangleMesh& mesh);
    int rayQuery(const Point_3& origin, const Point_3& direction) const;
    int pointQuery(const Point_3& point) const;
    bool isBuilt() const;
};
```

#### PatchParameterizer
```cpp
class PatchParameterizer {
    ParameterizationResult parameterizePatch(
        const TriangleMesh& mesh, 
        const Patch& patch);
    
    SubMeshResult extractPatchSubmesh(
        const TriangleMesh& mesh, 
        const Patch& patch);
};
```

### 依赖库

- **CGAL**: 用于AABB树和参数化
- **VTK**: 用于3D渲染和鼠标拾取
- **Nanosvg**: 用于SVG解析
- **Qt**: 用于用户界面

### CMake配置

#### 新增文件

**core/mesh/CMakeLists.txt:**
```cmake
add_library(nbcam_mesh STATIC
    ...
    mesh_accelerator.h
    mesh_accelerator.cpp
    ...
)
```

**core/param/CMakeLists.txt:**
```cmake
add_library(nbcam_param STATIC
    ...
    patch_parameterizer.h
    patch_parameterizer.cpp
    ...
)
```

**core/pattern/CMakeLists.txt:**
```cmake
target_link_libraries(nbcam_pattern PUBLIC
    spdlog::spdlog
)

# nanosvg是header-only库，需要添加包含目录
# 尝试多种方式找到nanosvg的头文件路径
if(TARGET nanosvg::nanosvg)
    target_link_libraries(nbcam_pattern PUBLIC nanosvg::nanosvg)
elseif(TARGET nanosvg)
    target_link_libraries(nbcam_pattern PUBLIC nanosvg)
elseif(DEFINED nanosvg_INCLUDE_DIRS)
    target_include_directories(nbcam_pattern PUBLIC ${nanosvg_INCLUDE_DIRS})
else()
    # 使用find_path查找nanosvg.h
    find_path(NANOSVG_INCLUDE_DIR nanosvg.h
        PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/*/include
        ENV VCPKG_ROOT
        PATH_SUFFIXES include
    )
    if(NANOSVG_INCLUDE_DIR)
        target_include_directories(nbcam_pattern PUBLIC ${NANOSVG_INCLUDE_DIR})
    endif()
endif()
```

## 常见问题

### Q: 为什么nanosvg.h找不到？CMake报错找不到nanosvg::nanosvg目标？

**A:** nanosvg是header-only库，vcpkg可能不提供CMake目标。解决方案：
1. 确保在`vcpkg.json`中添加了`nanosvg`依赖
2. 运行`vcpkg install nanosvg`安装库
3. 在`CMakeLists.txt`中使用`find_package(nanosvg CONFIG REQUIRED)`
4. `core/pattern/CMakeLists.txt`中已包含多种fallback方式自动查找nanosvg头文件
5. 如果仍然失败，检查vcpkg安装路径，确保`nanosvg.h`存在于`vcpkg_installed/<triplet>/include/`目录下

### Q: Patch聚类后没有显示？

**A:** 检查：
1. 模型是否已加载
2. 聚类是否成功（查看日志）
3. 鼠标是否在模型上移动

### Q: SVG映射失败？

**A:** 确保：
1. 已选择一个patch
2. patch已进行参数化
3. SVG文件格式正确
4. patch有有效的UV坐标

### Q: Patch边界高亮不显示？

**A:** 检查：
1. `createPatchHighlightActor()`是否正确创建了VTK actor
2. actor是否添加到renderer
3. 边界边数据是否正确提取

## 性能优化建议

1. **大规模模型**：
   - 考虑使用更高效的邻接表数据结构
   - 对patch聚类结果进行缓存

2. **实时交互**：
   - 限制patch边界边的数量用于高亮显示
   - 使用LOD（Level of Detail）技术

3. **参数化**：
   - 对于简单patch，可以考虑使用平面投影
   - 缓存参数化结果

## 未来改进

1. **Patch编辑**：
   - 允许手动合并/分割patch
   - 调整二面角阈值并实时预览

2. **高级参数化**：
   - 支持多种参数化算法选择
   - 支持多UV岛处理

3. **SVG预览**：
   - 在UV视图中预览SVG映射
   - 支持SVG缩放和旋转

4. **批量处理**：
   - 支持批量对多个patch进行参数化
   - 支持批量导入SVG

## 相关文档

- [CGAL AABB树文档](https://doc.cgal.org/latest/AABB_tree/index.html)
- [CGAL参数化文档](https://doc.cgal.org/latest/Surface_mesh_parameterization/index.html)
- [Nanosvg文档](https://github.com/memononen/nanosvg)

## 更新日志

### 2026-01-23
- 初始实现Patch聚类功能
- 实现CGAL AABB树空间加速结构
- 实现鼠标交互（悬停高亮、点击选择）
- 实现Patch参数化
- 实现SVG映射到Patch UV并还原到模型
- 修复nanosvg链接问题
