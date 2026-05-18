# NBCAM 软件架构说明书（深度版）

## 1. 文档范围与目标

### 1.1 目标

本文档用于给研发、联调和维护人员提供统一的软件架构认知，回答以下问题：

- 系统由哪些软件层组成，每层负责什么。
- 每个模块采用了什么设计思路、用了什么工具/库。
- 模块之间怎么协作，数据如何流动。
- 当前实现完成度如何，哪些位置是扩展点或风险点。

### 1.2 纳入范围

- `apps/laser_cam_qt`
- `core/job`
- `core/mesh`
- `core/param`
- `core/pattern`
- `core/mapping`
- `core/process`
- `core/planner`
- `core/executor`
- 顶层构建与依赖配置（`CMakeLists.txt`、`vcpkg.json`）

### 1.3 排除范围

- `build_test`
- `fpga`

### 1.4 读者对象

- UI/业务流程开发
- 几何与路径算法开发
- 设备通信与执行链路开发
- 测试与集成工程师

---

## 2. 总体架构

## 2.1 分层结构

系统采用“UI 驱动 + 核心库流水线 + 执行器下发”的分层模式。

1. 应用与交互层（`apps/laser_cam_qt`）
   - UI 交互、参数输入、可视化展示、操作触发。
2. 流程编排层（`ApplicationController`）
   - 把 UI 操作编排成固定处理链路。
3. 领域能力层（`core/*`）
   - 模型处理、参数化、图案生成、映射、工艺、规划。
4. 执行与通信层（`core/executor`）
   - 任务到控制帧转换、缓冲队列、TCP 下发、联调日志。

## 2.2 架构核心思想

- 模块解耦：按业务能力拆库，不把算法写在 UI 里。
- 接口优先：关键能力通过接口抽象，便于替换实现。
- 统一任务模型：`LaserJob` 贯穿规划、保存、执行。
- 逐步演进：先打通闭环，再补齐高阶算法和设备后端。

## 2.3 关键约束

- 主运行平台为 Windows + C++17。
- UI 基于 Qt6，三维渲染基于 VTK。
- 几何核心能力依赖 CGAL。
- 执行链路当前主路径为 TCP。

---

## 3. 构建系统与依赖

## 3.1 构建组织

- 顶层 `CMakeLists.txt` 负责：
  - 统一编译选项与输出目录
  - 三方依赖发现
  - `core` 与 `apps/laser_cam_qt` 子项目接入
- `core/CMakeLists.txt` 将核心域拆分为 8 个静态库。

## 3.2 依赖清单（软件层面）

- Qt6：`Core/Widgets/OpenGL`
- VTK：`CommonCore/RenderingCore/RenderingOpenGL2/InteractionStyle/GUISupportQt`
- CGAL：网格与参数化能力
- gRPC + Protobuf：已接入构建依赖，运行链路未完全实装
- nlohmann_json：任务序列化
- spdlog：日志
- nanosvg：SVG 解析与渲染
- Threads：并发与通信线程

## 3.3 依赖管理策略

- 使用 vcpkg 管理大部分第三方库。
- 通过 CMake 的 target_link 进行模块化链接。
- 对多配置生成器做 Release 映射，避免 Debug/Release 运行时冲突。

---

## 4. 应用层架构（apps/laser_cam_qt）

## 4.1 启动入口（`main.cpp`）

### 设计思路

- 先完成运行环境防护，再启动业务窗口。

### 关键动作

- Qt 插件路径修正（Windows 下平台插件问题规避）。
- OpenGL 默认格式设置（保证 VTK 组件初始化稳定）。
- 日志初始化（控制台/文件）。
- 顶层异常捕获与堆栈日志（Windows）。
- 创建并显示 `MainWindow`。

### 使用工具

- Qt6
- VTK Qt 组件
- spdlog
- Win32 + DbgHelp

## 4.2 主窗口（`mainwindow.*`）

### 设计思路

- 主窗口作为“操作壳”，负责组织视图、菜单和流程触发。
- 业务状态由 Controller 管，窗口负责输入输出与状态反馈。

### 核心职责

- 文件/任务操作：导入模型、保存任务、加载任务。
- 流程触发：参数化、路径生成、路径规划、执行。
- patch 工作流：聚类、选中、纹理应用/编辑、清理。
- 通信探测：TCP/gRPC 联通测试入口。

### 子组件分工

- `ModelView`：3D 模型、patch、路径渲染，支持选中和高亮。
- `UVView`：UV 坐标与 UV 路径可视化，支持校验模式。
- `PathPreview`：按段查看路径，支持筛选和聚焦。
- `ParameterPanel`：参数输入与单位换算。
- 纹理相关对话框：贴图参数与详情管理。

### 使用工具

- Qt Widgets + 信号槽
- VTK 3D 渲染
- Controller 调用核心模块

## 4.3 流程编排器（`application_controller.*`）

### 设计思路

- 将分散的算法模块组装为稳定的处理链。
- 对外暴露“业务动作”而非底层算法细节。

### 对外动作（核心 API）

- `loadModel` / `clearModel`
- `parameterizeMesh` / `parameterizePatch`
- `generatePattern` / `generatePatternInPatch`
- `mapToXYZ`
- `assignProcessParams`
- `planPath`
- `saveJob` / `loadJob`
- `executeJob`

### 关键内部机制

- 策略名称归一（中文策略名与内部策略名映射）。
- patch UV 边界构建与回退策略（边界环 -> 凸包回退）。
- UV 路径裁剪（patch 域裁剪 + SVG Alpha 裁剪）。
- 圆弧往返模式特殊处理（可直接在曲面生成圆弧路径）。

### 使用工具

- 直接组合 `core/*` 能力
- Qt 信号反馈阶段状态

## 4.4 `GrpcClient` 当前定位

### 目标定位

- 预留为 gRPC 客户端抽象层。

### 当前实现状态

- 仍以本地 `DataBuffer/DataGenerator` 调用为主。
- 未形成完整 protobuf-stub 的远程调用路径。

---

## 5. 核心领域模块架构（core）

## 5.1 `job` 模块

### 设计思路

- 统一任务模型，作为算法链与执行链之间的标准契约。

### 核心数据结构

- `ProcessParams`：功率/频率/速度/延时参数。
- `PathPoint`：UV + XYZ + 激光状态 + 可选点级参数。
- `PathSegment`：段类型（MARK/JUMP）+ 点序列 + 可选段级参数。
- `LaserJob`：元数据 + 坐标信息 + 参数化信息 + 默认参数 + 段集合。

### 核心能力

- 任务合法性检查（`isValid`）。
- 点数统计、时间估算（`estimateTotalTime`）。
- JSON 序列化/反序列化（`JsonSerializer`）。

### 使用工具

- `nlohmann_json`
- `spdlog`

### 上下游接口

- 上游：`planner`/`application_controller` 产出任务。
- 下游：`executor` 读取任务并下发硬件。

## 5.2 `mesh` 模块

### 设计思路

- 将模型输入统一为内部三角网格表示，再提供 patch 与查询能力。

### 子模块与能力

1. `MeshIO`
   - 读取 OBJ/STL（包含 ASCII/Binary STL 兼容）。
   - 统一到 `TriangleMesh`。
2. `PatchClusterer`
   - 计算三角法向和二面角。
   - 采用 BFS 连通扩展实现 patch 聚类。
   - 输出 patch 属性（面积、平均法向、边界边）。
3. `MeshAccelerator`
   - 基于 CGAL AABB tree 的查询加速。
4. `MeshPreprocessor`
   - 提供修复/去噪/坐标对齐接口，当前部分为占位实现。

### 算法思路（Patch）

- 邻接判定：共享边的三角形视为邻接。
- 聚类准则：相邻三角二面角小于阈值则同 patch。
- 结果属性：面积加权法向 + 边界边提取。

### 使用工具

- CGAL
- 几何计算
- `spdlog`

### 当前风险

- 预处理高级能力（修复/去噪）尚未完整落地。

## 5.3 `param` 模块

### 设计思路

- 参数化算法策略化，支持按 patch 局部参数化。

### 关键对象

- `IParameterizer`：统一接口。
- `LSCMParameterizer`：共形优先。
- `ARAPParameterizer`：刚性保持优先。
- `PatchParameterizer`：提子网格并回写 UV。

### 算法流程（Patch 参数化）

1. 从原网格按 patch 三角集提取子网格。
2. 子网格执行 LSCM/ARAP 参数化。
3. 将子网格 UV 通过映射写回原网格顶点索引。

### 异常处理策略

- 对 UV 的 NaN/Inf 检测并修复。
- 主算法失败后使用后备参数化器。

### 使用工具

- CGAL 参数化相关组件
- `spdlog`

## 5.4 `pattern` 模块

### 设计思路

- 在 UV 域生成可控轨迹，策略和几何约束分离。

### 接口与实现

- `IFillStrategy`：统一路径生成接口。
- `LineHatchStrategy`：往返填充。
- `ContourStrategy`：轮廓偏置填充。
- `RingFillStrategy`：回型层级填充。

### SVG 处理链

- `NanosvgWrapper`：解析 SVG 路径、提取点列、渲染 alpha 图。
- `SVGToUVMapper`：将 SVG 几何映射到 UV 空间。
- 在 Controller 中结合 alpha mask 对路径做区域裁剪。

### 使用工具

- nanosvg
- 多边形和曲线几何
- `spdlog`

### 当前关注点

- 不同策略对复杂边界和空洞区域的处理一致性。

## 5.5 `mapping` 模块

### 设计思路

- UV->XYZ 映射要保证“可用性优先 + 性能可控”。

### 核心流程

1. 对每个 UV 路径点查找所在三角面。
2. 通过重心坐标插值求 XYZ。
3. 若点不在任何三角内，使用边界容差与近邻策略兜底。

### 性能思路

- 大三角集启用 UV 网格索引，减少 O(N*M) 全遍历。
- 支持 patch 局部三角搜索，降低搜索空间。

### 使用工具

- 数值几何计算
- `spdlog`

## 5.6 `process` 模块

### 设计思路

- 提供“几何 -> 工艺参数”映射的插件接口。

### 结构

- `IProcessModel`：工艺参数分配接口。
- `CurvatureModel`：当前实现版本。

### 当前实现现状

- 具备参数赋值链路。
- 曲率计算仍为简化实现，后续可替换为更准确模型。

### 使用工具

- 几何距离与邻域估计
- `spdlog`

## 5.7 `planner` 模块

### 设计思路

- 将几何路径加工化，使路径满足设备执行特性。

### 关键步骤

- `addJumpSegments`：段间跳转补全。
- `applyCornerDeceleration`：小角度拐点降速。
- `sparsifyParams`：点级参数压缩到段级。
- `optimizePathOrder`：预留路径排序优化点。
- `normalizeSegmentSequence`：修正段 id 与首尾连续性。

### 使用工具

- 向量几何和路径结构处理
- `LaserJob` 数据结构
- `spdlog`

## 5.8 `executor` 模块

### 设计思路

- 通过统一执行器接口隔离设备差异。

### 模块结构

- `IJobExecutor`：初始化、执行、停止、状态查询。
- `BoardExecutor`：当前可用主执行器。
- `Rtc6Executor`：结构预留，尚未完整实现。
- `DataBuffer`：双缓冲队列 + 帧写入。
- `TcpSocket`：长连接通信线程。
- `DataGenerator`：图元到底层帧的生成辅助。
- `frame_log_utils`：联通测试与帧日志。
- `grpc_service`：服务框架预留。

### 下发链路（Board）

1. `BoardExecutor` 遍历 `LaserJob`。
2. 坐标与段类型转换成硬件帧字段。
3. 写入 `DataBuffer` 双缓冲。
4. `TcpSocket` 线程取读缓冲并发送。

### 帧格式思路（当前实现）

- 16 字节固定帧（8 个 `uint16_t`）。
- 字段顺序采用 `B, A, Z, Y, X, OPCODE, arg7, arg8`。
- `OPCODE` 区分 mark/jump/begin/end 等控制语义。

### 使用工具

- WinSock2
- C++ 并发原语
- `spdlog`

### 当前风险点

- `DataGenerator` 某些图元分支仍为待补。
- gRPC 服务与 RTC6 执行器仍未完整实装。

---

## 6. 模块协作与数据流

## 6.1 主流程（GUI）

1. 模型导入
   - `MainWindow -> ApplicationController::loadModel -> MeshIO`
2. patch 处理
   - `ModelView/Controller -> PatchClusterer`
3. 参数化
   - `ApplicationController -> PatchParameterizer -> LSCM/ARAP`
4. 图案生成
   - `ApplicationController -> FillStrategy (+ SVG 约束)`
5. UV->XYZ 映射
   - `ApplicationController -> UVMapper`
6. 工艺参数分配
   - `ApplicationController -> IProcessModel`
7. 路径后处理
   - `ApplicationController -> PathPlanner`
8. 任务固化
   - `LaserJob` 在内存中形成，支持 JSON 保存
9. 执行
   - `ApplicationController -> IJobExecutor (BoardExecutor)`

## 6.2 关键中间数据

- `TriangleMesh`：模型几何基座
- `Patch`：待加工区域结构
- `UVCoord`/`UVPathPoint`：参数域结果
- `PathPoint`/`PathSegment`：三维加工路径
- `LaserJob`：执行契约对象

## 6.3 典型控制流特征

- UI 触发是同步入口，但耗时计算在核心模块内完成。
- 大多数状态在 Controller 内聚，视图通过信号更新。
- 执行下发由独立线程（TCP 线程）与双缓冲解耦。

---

## 7. 模块级工具矩阵

| 模块 | 主要思路 | 主要工具/库 |
|---|---|---|
| apps/laser_cam_qt | UI 触发与可视化壳 | Qt6, VTK, spdlog |
| core/job | 统一任务模型与序列化 | nlohmann_json, spdlog |
| core/mesh | 网格统一表示 + patch 聚类 | CGAL, 几何算法, spdlog |
| core/param | 参数化策略化 | CGAL Parameterization, spdlog |
| core/pattern | UV 路径策略生成 + SVG 约束 | nanosvg, 几何算法, spdlog |
| core/mapping | UV->XYZ 反映射与加速 | 数值几何, spdlog |
| core/process | 几何驱动工艺参数分配 | 几何计算, spdlog |
| core/planner | 设备友好路径后处理 | 路径几何, spdlog |
| core/executor | 任务转帧与通信下发 | WinSock2, C++并发, spdlog |

---

## 8. 现状评估与演进建议

## 8.1 已稳定或可用部分

- GUI 主流程与核心流水线已贯通。
- patch 聚类、LSCM/ARAP 参数化、UV 路径生成、UV->XYZ 映射可形成闭环。
- `LaserJob` 结构化表达和 JSON 存取可用。
- TCP 执行链路可运行。

## 8.2 需持续完善部分

- `MeshPreprocessor` 深度能力（修复/去噪/坐标系对齐）。
- `CurvatureModel` 高质量曲率计算与参数映射。
- `DataGenerator` 图元生成完整性。
- `grpc_service` 生产级服务实现。
- `GrpcClient` 真实远程调用链路。
- `Rtc6Executor` SDK 级完整接入。

## 8.3 建议的工程化下一步

1. 对每个核心模块补齐单元测试和回归样例。
2. 将“占位实现”统一打标签并列入里程碑计划。
3. 为 `LaserJob` 增加版本化字段与兼容策略。
4. 在通信层补充可观测指标（帧吞吐、失败率、重连次数）。

---

## 9. 目录到职责映射

- `apps/laser_cam_qt`：交互入口、视图与流程触发
- `core/job`：任务数据契约与持久化
- `core/mesh`：网格导入与 patch 识别
- `core/param`：UV 参数化
- `core/pattern`：UV 图案路径生成
- `core/mapping`：UV->XYZ 映射
- `core/process`：工艺参数分配
- `core/planner`：路径后处理
- `core/executor`：任务执行与通信

本说明书作为当前 GUI 主链路的软件架构基线，可用于开发对齐、联调和后续演进评审。
