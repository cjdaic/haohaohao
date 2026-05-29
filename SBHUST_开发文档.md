# NBCAM 软件开发文档

版本基线：当前代码库，2026-05-21  
适用对象：软件开发、算法开发、设备联调、测试维护人员  
主程序：`apps/laser_cam_qt`，可执行文件目标名：`laser_cam_qt`  
项目版本：`0.0.1`

## 1. 项目概述

NBCAM 是一套面向复杂曲面激光加工/标刻的软件系统。软件以 Qt 桌面应用为主入口，完成模型导入、Patch 聚类、UV 参数化、SVG 贴图、路径规划、路径预览、模拟加工、真实下发和设备联调。

主业务链路：

```text
OBJ/STL 模型
  -> 网格加载与显示
  -> Patch 聚类与选择
  -> Patch 参数化
  -> SVG 纹理贴附与 UV 映射
  -> UV 路径生成
  -> UV 到 XYZ/A/B 路径映射
  -> 工艺参数绑定
  -> 路径规划与预览
  -> 模拟加工或真实下发
```

当前执行后端：

- 默认板卡链路：`BoardExecutor + DataBuffer + TcpSocket + DataGenerator`，通过 TCP 发送 16 字节固定数据帧。
- RTC-6 链路：`Rtc6Executor`，依赖 `RTC6/RTC6DLLx64.dll`、`RTC6/RTC6DLLx64.lib` 和 RTC-6 程序文件。
- gRPC 链路：保留 `five_axis.proto`、`grpc_client`、`grpc_service` 和历史 C# 参考文件，当前主执行链路仍以 TCP/RTC-6 为准。

轴模式：

- 三轴：X/Y/Z 有效，A/B 编码为中位值或默认值。
- 五轴：X/Y/Z/A/B 均进入默认板卡数据帧，A/B 来自 `PathPoint.a/b` 并参与偏置、软限位和编码。
- `swap_yz_axes`：设备设置中的 Y/Z 轴互换开关，影响真实加工、手动操作、校正图案和 frame 日志。

## 2. 目录结构

```text
E:/haohaohao
  apps/laser_cam_qt        Qt 主程序、视图、菜单、对话框、控制入口
  core/job                 LaserJob 数据结构与 JSON 序列化
  core/mesh                OBJ/STL 加载、网格预处理、Patch 聚类、查询加速
  core/param               LSCM/ARAP/Authalic/Patch 参数化
  core/pattern             SVG 解析、填充策略、UV 路径生成
  core/mapping             UV -> 曲面 XYZ 映射
  core/process             工艺参数模型
  core/planner             路径段后处理、跳线、规划封装
  core/executor            BoardExecutor、Rtc6Executor、DataBuffer、TCP、帧日志
  assets                   图标、视图按钮、资源图片
  docs                     历史设计资料、实验材料、开发记录
  reference                用户可查阅资料、外部协议、本文档和用户手册
  RTC6                     RTC-6 DLL、LIB、RBF/OUT/DAT 等运行资料
  scripts                  打包、清理、辅助脚本
```

约定：

- `reference` 放稳定参考资料、协议、说明书、用户手册和交付文档。
- `docs` 放开发过程记录、样例输入、临时分析材料和实验说明。
- `build`、`build_test`、`ChiikawaLaser-cache`、`ChiikawaLaser-SetupFiles` 属于构建或安装产物，不作为源码维护入口。

## 3. 构建环境

### 3.1 推荐环境

- Windows 10/11
- Visual Studio 2022，MSVC x64
- CMake 3.20+
- Qt 6.10.1，当前预设路径：`D:/Qt/6.10.1/msvc2022_64`
- VTK 9.4.2，当前预设路径：`E:/VTK-9.4.2/install/lib/cmake/vtk-9.4`
- vcpkg，建议设置环境变量 `VCPKG_ROOT`

`vcpkg.json` 依赖：

- `cgal`
- `grpc`
- `protobuf`
- `nlohmann-json`
- `spdlog`
- `nanosvg`
- `clipper2`

### 3.2 配置与编译

推荐使用 CMake presets：

```powershell
cmake --preset release
cmake --build --preset release
```

Debug：

```powershell
cmake --preset debug
cmake --build --preset debug
```

直接构建现有目录：

```powershell
cmake --build build --config Release --target laser_cam_qt
cmake --build build --config Debug --target laser_cam_qt
```

常见输出：

```text
build/bin/Release/laser_cam_qt.exe
build/bin/Debug/laser_cam_qt.exe
build/bin/RelWithDebInfo/laser_cam_qt.exe
```

### 3.3 运行时依赖复制

`apps/laser_cam_qt/CMakeLists.txt` 在 MSVC 下会执行：

- 复制 `$<TARGET_RUNTIME_DLLS:laser_cam_qt>` 到可执行文件目录。
- 创建并填充 `platforms/qwindows.dll`。
- 复制 `apps/laser_cam_qt/qt.conf`。
- 如果存在 `RTC6/RTC6DLLx64.dll`，复制到可执行文件目录。

若运行时报 Qt 平台插件错误，优先检查：

- 可执行目录下是否有 `platforms/qwindows.dll`。
- 可执行目录下是否有 `qt.conf`。
- 是否混用了 Debug/Release 版本 DLL。

### 3.4 第三方库配置约束

根 `CMakeLists.txt` 当前将 Debug、RelWithDebInfo、MinSizeRel 映射到第三方 Release 导入库，并在 MSVC 下设置：

```text
CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreadedDLL
Debug: _ITERATOR_DEBUG_LEVEL=0
```

原因是当前第三方依赖按 Release ABI 提供。修改这部分时必须同时验证 Qt、VTK、vcpkg 库的 ABI 和运行时 DLL 来源。

## 4. 软件分层

### 4.1 应用层 `apps/laser_cam_qt`

职责：

- 主窗口、菜单、工具栏、对话框和状态栏。
- 3D 模型视图、UV 视图、路径预览和纹理管理。
- 加工控制按钮：开始、暂停、急停、刷新、模拟。
- 通信、设备、板卡、激光器配置入口。
- 命令行模式入口。

关键文件：

| 文件 | 职责 |
|---|---|
| `main.cpp` | 程序入口、CLI 模式、日志初始化、Qt/VTK OpenGL 配置、异常栈日志 |
| `mainwindow.*` | 主窗口、菜单、配置对话框、加工控制、手动点动、加工平面校正、日志、路径保存、验证示例 |
| `application_controller.*` | 业务流水线编排，连接核心模块 |
| `model_view.*` | VTK 3D 显示、Patch 选择、纹理、路径和已加工高亮 |
| `uv_view.*` | UV 显示、纹理、路径、校验模式和高亮缓存 |
| `path_preview.*` | 路径段表格、筛选、导出 CSV、保存路径 |
| `parameter_panel.*` | 功率、频率、速度、填充策略、单位转换 |
| `plane_pose_widget.*` | 平面位姿参数控件 |
| `texture_*` | SVG 纹理列表、编辑和详情 |
| `svg_raster_cache.*` | SVG 栅格缓存 |
| `grpc_client.*` | gRPC 连接测试和预留客户端 |

### 4.2 控制器层 `ApplicationController`

`ApplicationController` 是 UI 与核心库之间的主要边界。UI 不应直接拼接复杂算法流程。

主要状态：

- 当前网格模型。
- 当前参数化结果。
- 当前 SVG/图案路径。
- 当前 `LaserJob`。
- 当前 Pattern 所属 Patch。

主要动作：

- `loadModel`
- `parameterizeMesh`
- `parameterizePatch`
- `importSVGWithTiling`
- `generatePattern`
- `generatePatternInPatch`
- `mapToXYZ`
- `assignProcessParams`
- `planPath`
- `saveJob`
- `loadJob`
- `executeJob`
- `clearPatternForPatch`

### 4.3 核心算法层 `core/*`

| 模块 | 库目标 | 职责 |
|---|---|---|
| `job` | `nbcam_job` | `LaserJob`、JSON 序列化 |
| `mesh` | `nbcam_mesh` | OBJ/STL、Patch 聚类、网格查询 |
| `param` | `nbcam_param` | LSCM/ARAP/Authalic/Patch 参数化 |
| `pattern` | `nbcam_pattern` | SVG 解析、轴向/回型/轮廓等策略 |
| `mapping` | `nbcam_mapping` | UV 点映射到曲面 |
| `process` | `nbcam_process` | 工艺参数模型 |
| `planner` | `nbcam_planner` | 路径规划与后处理 |
| `executor` | `nbcam_executor` | 执行器、帧生成、TCP/RTC-6 下发 |

### 4.4 执行层 `core/executor`

关键类：

- `IJobExecutor`：统一执行器接口。
- `BoardExecutor`：默认板卡执行器，负责预检、坐标变换、软限位、编码、插补、延时帧和 TCP 写入。
- `DataBuffer`：双缓冲发送队列，维护 TCP 线程状态。
- `TcpSocket`：TCP 连接、重试、发送。
- `DataGenerator`：从历史 C# 逻辑移植的数据帧生成辅助。
- `Rtc6Executor`：RTC-6 板卡执行器。
- `frame_log_utils`：生成帧日志、首帧验证、gRPC+TCP 示例发送日志。
- `five_axis.proto`：五轴历史接口定义，当前也放在 `reference` 便于联调对照。

## 5. 核心数据模型

### 5.1 `LaserJob`

位置：`core/job/laser_job.h`

`LaserJob` 是路径规划链路和执行链路之间的主数据契约。

主要字段：

- `meta`：任务 ID、单位、源模型、创建时间。
- `coordinate`：工作平面、机床坐标系、模型到机床 4x4 变换矩阵。
- `parameterization`：参数化算法、UV island 数量和备注。
- `process_defaults`：默认功率、频率、速度、开关光延时。
- `segments`：路径段列表。

重要方法：

- `clear()`
- `isValid()`
- `getTotalPointCount()`
- `estimateTotalTime()`

### 5.2 `PathPoint`

字段：

- `u, v`：UV 坐标。
- `x, y, z`：三维坐标，单位 mm。
- `a, b`：五轴偏摆/旋转轴，单位按执行器配置解释。
- `laser`：激光状态，`0` 关闭，`1` 开启。
- `params_override`：点级工艺参数覆盖。

注意：`PathPoint` 包含 `std::unique_ptr`，禁用复制，仅支持移动。

### 5.3 `PathSegment`

字段：

- `id`：段 ID。
- `type`：`MARK` 或 `JUMP`。
- `strategy`：`CONTOUR`、`HATCH`、`RING`、`IMAGE_SAMPLE`、`ARC_HATCH`。
- `params_override`：段级工艺参数覆盖。
- `points`：路径点列表。

### 5.4 工艺参数优先级

执行时建议按以下优先级解析：

```text
点级 params_override
  -> 段级 params_override
  -> job.process_defaults
  -> 执行器或 UI 默认值
```

修改功率、频率、速度、延时逻辑时，应同步确认路径预览、帧日志和真实下发三处行为一致。

## 6. UI 功能入口

### 6.1 菜单

`文件`：

- 打开模型
- 导出模型
- 执行任务
- 退出

`视图`：

- 3D 线框
- UV 线框
- 显示包围盒
- 显示坐标系
- 显示 UV 视图
- UV 校验模式
- 着色模式：默认、法向量、曲率

`工具`：

- 导入 SVG 图案
- 面片聚类
- 参数化选中 Patch
- 应用 SVG 纹理到 Patch
- 自适应均匀细分
- 模型偏置与加工区域
- 路径规划 -> 执行路径规划
- 清理面片数据
- 验证示例流程

`连接`：

- 测试 gRPC 通信
- 测试 TCP 通信
- 测试串口通信
- 连接

`设备与板卡`：

- 设备设置
- 板卡设置
- 激光器设置

`手动操作`：

- 打开手动操作面板。当前已接入步长输入、X/Y/Z 方向点动、STOP 回中和 TCP 数据帧下发。

`校正`：

- 加工平面3x3正方形校正。生成中心在 X/Y=-5、0、5 mm，Z=0 的 3x3 正方形阵列，默认边长 2 mm。

`测量`：

- 计算面积
- 计算曲面点坐标
- 计算两点直线距离
- 计算两点曲面测地线距离

`日志`：

- 打开日志
- 清理当前日志
- 清理所有日志

`语言`：

- 系统默认和可发现的 Qt 翻译语言。

`帮助`：

- 命令行模式
- 关于

### 6.2 加工控制按钮

`MainWindow::createMachiningControlWidget()` 创建右上角按钮：

- 开始加工
- 暂停
- 急停
- 刷新至起始数据帧并回归起点
- 模拟加工

模拟加工是 UI 层的安全预演入口，不等价于 `BoardExecutor::dry_run_only` 的内部开关。真实下发前建议优先使用模拟加工。

## 7. 命令行模式

启动：

```powershell
build\bin\Release\laser_cam_qt.exe --cli
```

支持命令：

```text
help
status
pwd
cd <path>
ls [path]
load_model <path>
parameterize [LSCM|ARAP|AUTHALIC]
import_svg <path> [tile_u] [tile_v]
generate_pattern <strategy> <spacing_mm> [angle_deg]
map_to_xyz
assign_params [curvature]
plan_path
save_job <path>
load_job <path>
estimate_time
exit
```

典型脚本化验证：

```text
load_model "docs/model1111.STL"
parameterize LSCM
import_svg "docs/HUST.svg" 0.05 0.05
map_to_xyz
assign_params curvature
plan_path
save_job "out/job.json"
estimate_time
```

CLI 当前主要面向开发验证和离线任务生成，不覆盖所有 GUI Patch 纹理管理能力。

路径解析规则：

- `resolveCliPath()` 将相对路径按当前 CLI 工作目录解析。
- `pwd` 输出 `QDir::currentPath()`。
- `cd <path>` 通过 `QDir::setCurrent()` 修改后续命令工作目录。
- `ls` 和 `dir` 共用 `printCliDirectoryListing()`，目录优先、名称排序，文件路径会显示单条条目。
- 路径包含空格时依赖 `tokenizeCliLine()` 的引号解析。

## 8. 设备与通信配置

### 8.1 连接设置

入口：`连接 -> 连接`

配置项：

- gRPC 端点。
- 连接时测试 gRPC。
- TCP 主机。
- TCP 端口。
- TCP 超时。
- TCP 重试间隔。
- 串口号。
- 串口波特率。
- 执行前联锁时同时测试串口。
- 执行前将任务坐标转换到机床坐标。
- 真实下发前强制做 TCP/串口联锁检查。

### 8.2 设备设置

入口：`设备与板卡 -> 设备设置`

配置：

- 设备：默认板卡、RTC-6 板卡。
- 轴模式：三轴、五轴。
- 交换 y/z 轴。

轴模式影响默认板卡的 A/B 编码。`swap_yz_axes` 会在 `encodePointForMachining()` 中交换逻辑 Y/Z，再做偏置、软限位和 16 位编码；手动操作和校正图案用同样的通道映射手动编码。RTC-6 当前配置结构主要使用 X/Y/Z 轴范围和 RTC 单位系数。

### 8.3 板卡设置

入口：`设备与板卡 -> 板卡设置`

配置项：

- DataGenerator 时序：跳转速度、开光延迟、关光延迟、扫描延迟、跳转延迟、多边形延迟。
- RTC-6：卡号、编码系数、默认扫描速度、默认跳转速度、程序文件。
- 机床零点偏置：X/Y/Z/A/B。
- 软限位：X/Y/Z/A/B 最小/最大。

默认板卡编码：

```text
encoded = round((machine_mm + offset_mm) * 1000 + 32768)
```

RTC-6 编码：

```text
encoded = round((machine_mm + offset_mm) * units_per_mm)
```

软限位检查发生在任务坐标变换和零点偏置之后。

### 8.4 激光器设置

入口：`设备与板卡 -> 激光器设置`

默认资料：

```text
reference/M8 100w 产品说明书(1).pdf
```

默认参数：

- 型号：M8 100W
- 额定功率：100 W
- 功率范围：0 W 到 100 W
- 频率范围：20000 Hz 到 60000 Hz
- 控制方式：串口+TTL

支持导入：

- `.json`
- `.csv`
- `.txt`
- `.ini`
- `.pdf`

文本类文件会尝试识别型号、控制方式、功率范围、频率范围、波特率和串口号。PDF 当前只记录资料来源，具体字段需要人工确认。

## 9. 加工执行逻辑

### 9.1 模拟加工

入口：右上角“模拟加工”按钮。

作用：

- 验证路径段顺序。
- 检查路径预览、3D 和 UV 高亮。
- 提前发现路径为空、段异常、可视化性能问题。
- 替代旧版连接配置中的 Dry Run 入口。

模拟高亮约定：

- 路径表格只选中当前段。
- 3D/UV 视图累计显示已加工段。
- 3D 高亮通过 VTK cell 颜色数组增量更新。
- UV 高亮通过透明叠加缓存增量补画。

### 9.2 真实下发

入口：

- 右上角“开始加工”。
- `文件 -> 执行任务`。

真实下发前检查：

- 当前任务是否有效。
- 模型是否在加工区域内。
- TCP/串口联锁检查是否通过。
- 执行器 `validateJob` 是否通过。
- 右侧参数面板 `ParameterPanel::isLaserOutputEnabled()` 是否符合本次开光预期。

当前 `onStartMachining()` 不再弹出开关光或段级反馈确认框：

- `machining_laser_output_enabled_` 直接读取右侧“激光输出/开光”。
- `machining_segment_feedback_enabled_` 默认关闭。
- `prepareMachiningPlan()` 生成批次后，`sendCurrentMachiningFrame()` 连续 `appendBytes()` 写入本地双缓冲。
- `machining_transport_pending_bytes_` 记录未满块尾部，`flushMachiningTransportTail()` 在结束时用 END 帧和零填充补齐一个 `DataBuffer::DATA_BUF_SIZE` 块，再 `forceFill()` 和 `waitUntilIdle()`。
- `waitUntilIdle()` 只表示发送侧队列排空，不表示振镜物理运动已经完成。

### 9.3 急停、暂停和刷新

- 暂停：停止当前 UI 加工推进，保留当前进度。
- 急停：停止数据帧发送并停止 TCP 线程。
- 刷新：回到起始数据帧；真实链路下会尝试发送回归起点数据帧。

软件急停不能替代硬件急停、门禁、光闸和联锁回路。

### 9.4 手动操作和校正图案

手动操作入口：

- `MainWindow::openManualOperationPanel()`
- `MainWindow::handleManualJog()`
- `MainWindow::resetManualJogToCenter()`
- `MainWindow::sendManualJogTo()`

发送逻辑：

- 检查 `MachiningRunState::Idle`，加工中拒绝点动。
- 按当前步长更新逻辑 X/Y/Z。
- 使用 `validateTcpTargetForMachining()` 和 `ensureMachiningTransportConnected()`。
- 逻辑点按 `swap_yz_axes` 映射到数据帧 Y/Z 通道。
- 生成 BEGIN、目标保持帧、END，并补齐到 `DataBuffer::DATA_BUF_SIZE` 后发送。
- spdlog 记录逻辑坐标、frame_axis_mm、编码值、payload 帧数、TCP 反馈。

校正入口：

- `MainWindow::openCalibrationDialog()`
- `MainWindow::sendCalibrationSquareGrid()`

校正图案：

- 默认边长 2 mm、中心间距 5 mm、3 x 3。
- 逻辑平面为 X/Y，Z=0；Y/Z 轴互换后再写入实际帧通道。
- 每条正方形边按 `computeInterpolationSamplesForMachining()` 和 `interpolateAxisForMachining()` 插补，不只发送四个角点。
- 使用当前 `laser_on_delay_us`、`laser_off_delay_us`、`mark_delay_us`、`jump_delay_us`、`polygon_delay_us`。
- 默认开光，但对话框允许本次不开光检查轨迹。

## 10. 默认板卡数据帧

默认板卡使用固定 16 字节帧，每帧为 8 个小端 `uint16_t`。

字段顺序：

```text
B, A, Z, Y, X, OPCODE, arg7, arg8
```

常用 OPCODE：

| 操作 | 帧内容 |
|---|---|
| 开始 | `(0,0,0,0,0,0xFF00,0,0)` |
| Mark 点 | `(B,A,Z,Y,X,0x00FF,0,0)` |
| Jump 点 | `(B,A,Z,Y,X,0x0000,0,0)` |
| 结束 | `(0,0,0,0,0,0x1100,0,0)` |
| 设置频率开始 | `(0x00AA,0,0,0,0,0xAA00,0,0)` |
| 设置频率结束 | `(0x00AA,0,0,0,0,0x5500,0,0)` |
| 设置功率 | `(power,0,0,0,0,0xBB00,0,11451)` |

三轴模式：

- A/B 不使用路径值，默认编码为 `0x8000`。

五轴模式：

- `PathPoint.a/b` 经偏置、限位、编码后进入 A/B 字段。

帧日志：

- 路径规划和验证示例会在 `log` 下生成 frame 日志。
- 文件名通过 `buildUnifiedLogFileName("frame", model_path, svg_path)` 生成，形如 `frame_<model>_<svg>-<timestamp>.log`。
- 连接流程不再生成连接情况日志；`日志 -> 打开日志` 固定打开最近一次 frame 日志。
- `frame_log_utils` 导出时接收 `swap_yz_axes` 和开光状态，便于与真实发送帧、C# 参考实现、板卡抓包对照。

## 11. RTC-6 链路

文件：

- `core/executor/rtc6_executor.*`
- `RTC6/RTC6DLLx64.dll`
- `RTC6/RTC6DLLx64.lib`
- `reference/RTC6.pdf`

构建：

- 如果 `RTC6/RTC6DLLx64.lib` 存在，`nbcam_executor` 会链接该库。
- 如果 `RTC6/RTC6DLLx64.dll` 存在，`laser_cam_qt` 构建后会复制到输出目录。

配置：

- 卡号。
- `units_per_mm`。
- 默认扫描速度。
- 默认跳转速度。
- 程序文件。
- X/Y/Z 偏置和软限位。

限制：

- 离线环境只能验证构建、配置和预检。
- 真实调用需要 RTC-6 硬件、驱动、DLL 和程序文件均匹配。

## 12. 日志与异常

主日志：

```text
nbcam.log
```

frame 和验证日志：

```text
log/
```

`main.cpp` 做了以下初始化：

- Windows 控制台 UTF-8。
- Qt 插件路径定位。
- Qt 消息转 spdlog。
- Windows 顶层异常过滤器。
- 未处理异常时记录异常码、地址和调用栈。

排查崩溃时优先收集：

- `nbcam.log`
- 最近的 `log/*.log`
- 使用的模型和 SVG。
- 构建配置和可执行文件目录 DLL 列表。

## 13. 关键开发约定

### 13.1 UI 不直接承载算法

UI 负责采集参数、触发动作和显示状态。几何、路径、执行逻辑应优先放入 `core/*` 或 `ApplicationController`。

### 13.2 执行链路修改必须同步预检

新增坐标轴、帧字段、限位规则、变换规则时，应同步检查：

- `BoardExecutor::validateJob`
- `BoardExecutor::encodePointForMachine`
- `Rtc6Executor::validateJob`
- `Rtc6Executor::encodePointForRtc`
- `MainWindow::prepareMachiningPlan`
- `MainWindow::executeJob`
- `MainWindow::sendManualJogTo`
- `MainWindow::sendCalibrationSquareGrid`
- 板卡设置 UI
- 帧日志导出

### 13.3 区分“路径选择”和“加工进度”

当前约定：

- 用户手动选择路径段：表格选择同步到 3D/UV 视图。
- 加工进度推进：表格只显示当前段，3D/UV 视图累计显示已加工段。

相关文件：

- `mainwindow.cpp`
- `path_preview.cpp`
- `model_view.cpp`
- `uv_view.cpp`

### 13.4 Patch 路径保存与纹理状态

`MainWindow` 维护 Patch 纹理信息和保存路径信息。清理纹理、移除保存路径、重新规划路径时需要同时维护：

- `patch_texture_infos_`
- `saved_patch_jobs_`
- `current_svg_path_`
- `texture_list_widget_`
- `uv_view_`
- `model_view_`
- `execute_job_action_` 可用状态

### 13.5 单位转换

`ParameterPanel` 对外返回标准单位：

- 功率：W
- 频率：Hz
- 速度：mm/s
- 间距：mm
- 角度：度

新增 UI 单位时，需要保证内部标准单位不变，避免影响核心算法和执行器。

### 13.6 Y/Z 轴互换约定

`swap_yz_axes` 是硬件通道映射开关，不是模型几何变换 UI：

- 真实加工：`encodePointForMachining()` 在偏置和编码前交换逻辑 Y/Z。
- 手动操作：`sendManualJogTo()` 直接计算 `frame_y_mm` 和 `frame_z_mm`。
- 校正图案：按逻辑 XY 平面生成点，再映射到帧通道。
- 日志：frame 日志导出和 spdlog 均应反映交换后的帧通道值。

修改该逻辑时必须同步硬件实测：X/Y/Z 手动点动、3x3 校正图案、实际加工小路径。

## 14. 常见开发任务

### 14.1 新增填充策略

建议步骤：

1. 在 `core/pattern` 中新增策略类，实现 `IFillStrategy`。
2. 在 `ParameterPanel` 增加策略名称。
3. 在 `ApplicationController` 中补充 GUI 名称到策略枚举/逻辑的映射。
4. 确认 `PathSegment.strategy` 标记正确。
5. 确认 SVG mask、轮廓模式、路径断点和 Jump 段处理。
6. 更新路径预览、帧日志和用户手册。

### 14.2 新增参数化算法

建议步骤：

1. 在 `core/param` 中新增 `IParameterizer` 实现。
2. 在 `PatchParameterizer` 或控制器中接入算法名称。
3. 在 `MainWindow::parameterizeSelectedPatch` 的算法选择对话框中增加入口。
4. 验证 UV 边界、Patch 布局和 SVG 贴图。
5. 增加失败提示和日志。

### 14.3 新增设备执行后端

建议步骤：

1. 新增 `IJobExecutor` 实现。
2. 定义后端 `MachineConfig` 和 `PreflightReport`。
3. 实现初始化、预检、执行、停止和状态查询。
4. 在 `MainWindow::configureDeviceSettings` 增加设备选项。
5. 在 `MainWindow::executeJob` 接入分支。
6. 明确模拟、取消开光轨迹试跑、真实出光和联锁边界。
7. 增加帧/命令日志，便于硬件联调。

### 14.4 扩展五轴 A/B 逻辑

检查点：

1. `PathPoint.a/b` 的来源。
2. `UV -> XYZ/A/B` 映射逻辑。
3. `AxisMode::FiveAxis` 下的偏置和软限位。
4. 帧字段顺序仍为 `B,A,Z,Y,X,OPCODE,arg7,arg8`。
5. 三轴模式下 A/B 是否保持中位值。
6. 参考 `reference/DataGenerator.cs` 和 `reference/five_axis.proto`。

### 14.5 修改通信参数

检查点：

1. UI 默认值和 `BoardExecutor::TransportConfig` 默认值是否一致。
2. 连接测试、真实下发前联锁、执行器初始化是否使用同一份配置。
3. 发送超时、重试次数和重试间隔是否适合硬件。
4. 日志是否包含 host、port、timeout、错误原因。

### 14.6 修改加工平面校正

检查点：

1. 图案逻辑坐标仍以 X/Y 平面、Z=0 为输入。
2. `swap_yz_axes` 后的数据帧通道是否符合硬件台面。
3. 软限位是否覆盖默认 `-6 mm` 到 `6 mm` 范围。
4. 插补速度、开关光延时和补齐缓冲是否与真实加工链路一致。
5. spdlog 是否包含中心点、角点、编码值、payload 帧数和 TCP 反馈。

## 15. 测试与验证

### 15.1 编译验证

```powershell
cmake --build build --config Release --target laser_cam_qt
cmake --build build --config Debug --target laser_cam_qt
```

### 15.2 GUI 功能验证

1. 打开 STL/OBJ 模型。
2. 执行 Patch 聚类。
3. 选择 Patch 并参数化。
4. 应用 SVG 纹理。
5. 修改功率、频率、速度、间距、角度和策略。
6. 执行路径规划。
7. 打开路径预览，检查段 ID、Mark/Jump、首尾点和点数。
8. 保存 Patch 路径，再切换 Patch 验证状态保持。
9. 使用模拟加工检查累计高亮。
10. 使用连接测试检查 TCP/串口/gRPC。
11. 真实下发前确认加工区域、软限位、联锁、激光器和急停状态。

### 15.3 CLI 验证

```powershell
build\bin\Release\laser_cam_qt.exe --cli
```

执行：

```text
pwd
ls docs
cd docs
pwd
cd ..
status
load_model "docs/model1111.STL"
parameterize ARAP
import_svg "docs/HUST.svg" 0.05 0.05
generate_pattern line_hatch 0.1 0
map_to_xyz
assign_params curvature
plan_path
estimate_time
```

### 15.4 硬件联调验证

建议顺序：

1. 只测试 TCP 连接，不发送任务。
2. 用手动操作检查 X/Y/Z 方向，确认是否需要 `swap_yz_axes`。
3. 发送不开光的 3x3 校正图案检查中心、比例和方向。
4. 导出 frame 日志，与板卡协议核对。
5. 取消右侧“开光”执行小路径。
6. 检查软限位、零点偏置、急停和断连行为。
7. 最后勾选“开光”真实出光。

## 16. 已知注意事项

- 当前不是 Git 仓库时无法通过 `git status` 区分用户改动，编辑前应人工确认文件范围。
- gRPC 服务代码和 proto 是预留/参考链路，默认真实执行仍是 TCP/DataGenerator 或 RTC-6。
- RTC-6 需要外部硬件和 DLL 环境，离线只能做构建和预检。
- 激光器 PDF 导入不会自动解析 PDF 内容，只记录资料来源。
- 复杂模型和密集路径会增加 VTK/UV 绘制压力，性能验证优先使用 Release。
- Debug 配置也映射第三方 Release ABI，不能简单按普通 Debug ABI 排查第三方库问题。
