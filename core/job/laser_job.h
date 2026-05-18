#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace nbcam {

// 工艺参数
struct ProcessParams {
    double power_w = 20.0;           // 功率 (W)
    double freq_hz = 20000;          // 频率 (Hz)
    double speed_mm_s = 300.0;       // 速度 (mm/s)
    int64_t laser_on_delay_us = 0;   // 激光开启延时 (微秒)
    int64_t laser_off_delay_us = 0;  // 激光关闭延时 (微秒)
};

// 路径点
struct PathPoint {
    // UV坐标
    double u = 0.0;
    double v = 0.0;
    
    // 三维坐标
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    // 五轴振镜偏摆/旋转轴
    double a = 0.0;
    double b = 0.0;
    
    // 激光状态
    int laser = 0;  // 0=关闭, 1=开启
    
    // 工艺参数（可选，点级覆盖）
    std::unique_ptr<ProcessParams> params_override;
    
    // 默认构造函数
    PathPoint() = default;
    
    // 移动构造函数
    PathPoint(PathPoint&&) = default;
    PathPoint& operator=(PathPoint&&) = default;
    
    // 禁用复制（因为包含unique_ptr）
    PathPoint(const PathPoint&) = delete;
    PathPoint& operator=(const PathPoint&) = delete;
};

// 路径段类型
enum class SegmentType {
    MARK,   // 标刻段
    JUMP    // 跳线段
};

// 填充策略
enum class FillStrategy {
    CONTOUR,    // 轮廓
    HATCH,      // 栅格
    RING,       // 环形
    IMAGE_SAMPLE, // 纹理图像采样
    ARC_HATCH   // 轴向往返填充(圆弧)
};

// 路径段
struct PathSegment {
    int id = 0;
    SegmentType type = SegmentType::MARK;
    FillStrategy strategy = FillStrategy::CONTOUR;
    
    // 段级参数覆盖
    std::unique_ptr<ProcessParams> params_override;
    
    // 路径点列表
    std::vector<PathPoint> points;
    
    // 默认构造函数
    PathSegment() = default;
    
    // 移动构造函数
    PathSegment(PathSegment&&) = default;
    PathSegment& operator=(PathSegment&&) = default;
    
    // 禁用复制（因为包含unique_ptr和不可复制的PathPoint）
    PathSegment(const PathSegment&) = delete;
    PathSegment& operator=(const PathSegment&) = delete;
};

// 元数据
struct JobMeta {
    std::string job_id;
    std::string units = "mm";
    std::string source_model;
    std::string created_at;
};

// 坐标系信息
struct CoordinateInfo {
    std::string workplane = "z=0";
    std::string machine_frame = "laser_head_top";
    std::vector<double> transform_model_to_machine;  // 16个数字的变换矩阵
};

// 参数化信息
struct ParameterizationInfo {
    std::string algorithm = "ABF";
    int uv_island_count = 1;
    std::string notes;
};

// 激光加工任务
class LaserJob {
public:
    LaserJob() = default;
    ~LaserJob() = default;
    
    // 元数据
    JobMeta meta;
    CoordinateInfo coordinate;
    ParameterizationInfo parameterization;
    
    // 默认工艺参数
    ProcessParams process_defaults;
    
    // 路径段列表
    std::vector<PathSegment> segments;
    
    // 工具方法
    void clear();
    bool isValid() const;
    size_t getTotalPointCount() const;
    double estimateTotalTime() const;  // 估算总加工时间（秒）
};

} // namespace nbcam
