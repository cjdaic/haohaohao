# CGAL参数化实现说明

## 概述

参数化模块已更新为直接使用CGAL库的`Surface_mesh_parameterization`模块进行网格参数化。

## 实现细节

### 1. ABF参数化器 (`ABFParameterizer`)

- **算法**: 使用CGAL的`LSCM_parameterizer_3`（Least Squares Conformal Maps）
- **原因**: CGAL没有直接的ABF实现，LSCM是最接近的保形映射算法
- **备选方案**: 如果LSCM失败，自动回退到`Mean_value_coordinates_parameterizer_3`

### 2. BFF参数化器 (`BFFParameterizer`)

- **算法**: 使用CGAL的`ARAP_parameterizer_3`（As-Rigid-As-Possible）
- **原因**: ARAP算法与BFF类似，都能产生高质量的保形参数化
- **备选方案**: 如果ARAP失败，自动回退到`Discrete_authalic_parameterizer_3`

## 工作流程

1. **网格转换**: 将`TriangleMesh`转换为CGAL的`Surface_mesh<Point_3>`
2. **边界查找**: 遍历halfedge找到边界halfedge
3. **参数化**: 调用CGAL的参数化函数
4. **UV提取**: 从属性映射中提取UV坐标

## CGAL API使用

### 关键头文件
```cpp
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Surface_mesh_parameterization/LSCM_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/ARAP_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/parameterize.h>
#include <CGAL/boost/graph/properties.h>
```

### 边界查找
```cpp
halfedge_descriptor bhd = boost::graph_traits<Surface_mesh>::null_halfedge();
for (auto h : halfedges(mesh)) {
    if (CGAL::is_border(h, mesh)) {
        bhd = h;
        break;
    }
}
```

### 参数化调用
```cpp
CGAL::Surface_mesh_parameterization::LSCM_parameterizer_3<Surface_mesh> parameterizer;
CGAL::Surface_mesh_parameterization::Error_code err = 
    CGAL::Surface_mesh_parameterization::parameterize(mesh, parameterizer, bhd, uv_map);
```

## 依赖项

- CGAL库（通过vcpkg安装）
- Boost库（CGAL的依赖）

## CMake配置

```cmake
target_link_libraries(nbcam_param PUBLIC
    CGAL::CGAL
    spdlog::spdlog
)
```

## 注意事项

1. **网格要求**: 网格必须是流形（manifold），且有明确的边界
2. **边界处理**: 需要找到至少一个边界halfedge才能进行参数化
3. **UV坐标顺序**: UV坐标的提取顺序与顶点顺序一致
4. **错误处理**: 如果主要算法失败，会自动尝试备选算法

## 性能考虑

- LSCM和ARAP都是迭代算法，对于大型网格可能需要较长时间
- 建议对网格进行预处理（简化、修复）以提高参数化速度
- 可以考虑多线程处理多个UV岛

## 未来改进

1. 支持多UV岛参数化
2. 添加参数化质量评估
3. 支持自定义边界参数化
4. 添加参数化可视化
