# LaserJob 数据格式说明

## 概述

LaserJob使用JSON格式存储激光加工任务数据，包含模型信息、参数化信息、工艺参数和路径数据。

## JSON结构

```json
{
  "meta": {
    "job_id": "任务ID",
    "units": "单位（mm）",
    "source_model": "源模型文件名",
    "created_at": "创建时间（ISO 8601格式）"
  },
  "coordinate": {
    "workplane": "工作平面（如z=0）",
    "machine_frame": "机器坐标系名称",
    "transform_model_to_machine": [16个数字的变换矩阵]
  },
  "parameterization": {
    "algorithm": "参数化算法（ABF/ARAP/BFF）",
    "uv_island_count": "UV岛数量",
    "notes": "备注"
  },
  "process_defaults": {
    "power_w": 20.0,
    "freq_hz": 20000,
    "speed_mm_s": 300.0,
    "laser_on_delay_us": 0,
    "laser_off_delay_us": 0
  },
  "segments": [
    {
      "id": 0,
      "type": "mark",
      "strategy": "contour",
      "params_override": {
        "power_w": 18.0,
        "speed_mm_s": 250.0
      },
      "points": [
        {
          "u": 0.12,
          "v": 0.45,
          "x": 10.2,
          "y": -3.8,
          "z": 0.0,
          "laser": 1,
          "power_w": 18.0,
          "freq_hz": 22000,
          "speed_mm_s": 250.0
        }
      ]
    }
  ]
}
```

## 字段说明

### meta（元数据）
- `job_id`: 唯一任务标识符
- `units`: 长度单位，通常为"mm"
- `source_model`: 源模型文件路径
- `created_at`: ISO 8601格式的时间戳

### coordinate（坐标系）
- `workplane`: 工作平面描述
- `machine_frame`: 机器坐标系名称
- `transform_model_to_machine`: 4x4变换矩阵（行优先，16个数字）

### parameterization（参数化）
- `algorithm`: 使用的UV参数化算法
- `uv_island_count`: UV参数化产生的岛数量
- `notes`: 备注信息

### process_defaults（默认工艺参数）
- `power_w`: 激光功率（瓦特）
- `freq_hz`: 激光频率（赫兹）
- `speed_mm_s`: 加工速度（毫米/秒）
- `laser_on_delay_us`: 激光开启延时（微秒）
- `laser_off_delay_us`: 激光关闭延时（微秒）

### segments（路径段列表）

每个段包含：
- `id`: 段ID
- `type`: 段类型（"mark"或"jump"）
- `strategy`: 填充策略（"contour"/"hatch"/"ring"/"image_sample"）
- `params_override`: 段级参数覆盖（可选）
- `points`: 路径点列表

#### 路径点（PathPoint）
- `u`, `v`: UV坐标
- `x`, `y`, `z`: 三维坐标
- `laser`: 激光状态（0=关闭，1=开启）
- `power_w`, `freq_hz`, `speed_mm_s`: 点级参数覆盖（可选）

## 参数优先级

1. **点级参数**（最高优先级）
2. **段级参数**（params_override）
3. **默认参数**（process_defaults）

## 示例

完整示例请参考 `prompt.txt` 中的说明。

## 未来扩展

计划支持Protobuf二进制格式以提高性能和文件大小。
