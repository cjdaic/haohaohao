# NBCAM 系统架构文档

## 1. 系统总体架构

NBCAM采用分层架构设计，从下到上分为：

1. **硬件执行层**: 自研板卡/RTC-6板卡
2. **通信层**: TCP/gRPC服务
3. **算法层**: 几何处理、参数化、路径规划
4. **应用层**: Qt UI

## 2. 核心模块说明

### 2.1 Job模块
- **职责**: 任务数据模型定义和序列化
- **主要类**: `LaserJob`, `JsonSerializer`
- **数据格式**: JSON (laser_job.json)

### 2.2 Mesh模块
- **职责**: 网格导入、预处理
- **主要类**: `MeshIO`, `MeshPreprocessor`
- **支持格式**: OBJ, STL

### 2.3 Param模块
- **职责**: UV参数化算法
- **接口**: `IParameterizer`
- **实现**: `ABFParameterizer` (可扩展ARAP、BFF等)

### 2.4 Pattern模块
- **职责**: UV平面图案生成
- **接口**: `IFillStrategy`
- **实现**: `ContourStrategy`, `HatchStrategy` (可扩展Ring、ImageSample等)

### 2.5 Mapping模块
- **职责**: UV到XYZ坐标映射
- **主要类**: `UVMapper`
- **算法**: 三角面定位、重心坐标插值

### 2.6 Process模块
- **职责**: 工艺参数生成
- **接口**: `IProcessModel`
- **实现**: `CurvatureModel` (基于曲率的参数分配)

### 2.7 Planner模块
- **职责**: 路径后处理
- **主要类**: `PathPlanner`
- **功能**: 分段、跳线、减速、参数稀疏化

### 2.8 Executor模块
- **职责**: 硬件驱动
- **接口**: `IJobExecutor`
- **实现**: `BoardExecutor`, `Rtc6Executor`

### 2.9 Sim模块
- **职责**: 可视化和仿真
- **主要类**: `Visualizer`
- **功能**: 3D/UV可视化、统计信息

## 3. 数据流

```
模型文件(OBJ/STL)
    ↓
MeshIO加载
    ↓
MeshPreprocessor预处理
    ↓
IParameterizer参数化 → UV坐标
    ↓
IFillStrategy生成图案 → UV路径
    ↓
UVMapper映射 → XYZ路径
    ↓
IProcessModel分配参数 → 带参数的路径
    ↓
PathPlanner后处理 → 优化后的路径
    ↓
LaserJob序列化 → JSON文件
    ↓
IJobExecutor执行 → 硬件板卡
```

## 4. 插件化设计

系统采用接口+实现的方式，支持插件化扩展：

- `IParameterizer`: 参数化算法插件
- `IFillStrategy`: 填充策略插件
- `IProcessModel`: 工艺参数模型插件
- `IJobExecutor`: 执行器插件

## 5. 关键技术点

### 5.1 UV参数化
- 使用CGAL库实现ABF/ARAP等算法
- 处理UV岛和缝（seam）

### 5.2 UV→XYZ映射
- 使用BVH/AABB树加速三角面查找
- 重心坐标插值

### 5.3 工艺参数自适应
- 基于曲率/高度/入射角
- 能量模型

### 5.4 硬件通信
- 双缓冲机制（DataBuffer）
- TCP实时通信
- gRPC服务接口

## 6. 扩展方向

1. 支持更多参数化算法（ARAP、BFF等）
2. 支持更多填充策略（环形、纹理图像等）
3. 支持更多工艺参数模型（能量模型等）
4. 支持更多硬件板卡
5. Protobuf二进制格式支持
