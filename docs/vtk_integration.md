# VTK可视化集成说明

## 概述

NBCAM使用VTK进行3D模型和路径的可视化。ModelView类集成了VTK的渲染管线。

## 实现细节

### ModelView类

`ModelView`继承自`QVTKOpenGLNativeWidget`，这是Qt和VTK集成的标准方式。

#### 主要功能

1. **3D模型渲染**
   - 使用`vtkPolyData`存储三角网格
   - 使用`vtkPolyDataMapper`进行映射
   - 使用`vtkActor`进行渲染

2. **路径渲染**
   - 标刻段：红色实线
   - 跳线段：虚线（待实现）

3. **交互功能**
   - 鼠标旋转、缩放、平移
   - 重置相机

### UVView类

`UVView`使用Qt的`QPainter`进行2D绘制，显示UV坐标和路径。

#### 主要功能

1. **UV坐标显示**
   - 蓝色点表示网格顶点
   - 自动计算边界和缩放

2. **UV路径显示**
   - 红色线条表示加工路径
   - 红色点表示路径点

3. **网格显示**
   - 灰色网格线辅助查看

## 使用方法

### 设置模型

```cpp
model_view_->setMesh(mesh);
model_view_->resetCamera();
```

### 设置路径

```cpp
model_view_->setJob(job);
model_view_->updateView();
```

### UV视图

```cpp
uv_view_->setUVCoords(uv_coords);
uv_view_->setUVPath(uv_path);
```

## 依赖

- VTK 9.4.2
- Qt 6.10.1
- OpenGL支持

## 注意事项

1. **VTK初始化**: 确保VTK在使用前正确初始化
2. **OpenGL上下文**: 需要有效的OpenGL上下文
3. **内存管理**: VTK对象使用引用计数，注意正确释放
4. **线程安全**: VTK渲染应在主线程进行

## 未来改进

1. 支持路径颜色根据功率/速度变化
2. 支持路径动画播放
3. 支持多视角切换
4. 支持路径选择和高亮
