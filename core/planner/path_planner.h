#pragma once

#include "../job/laser_job.h"
#include <vector>

namespace nbcam {

// 路径规划器
class PathPlanner {
public:
    PathPlanner() = default;
    ~PathPlanner() = default;
    
    // 路径后处理：分段、跳线、减速等
    void postProcess(LaserJob& job);
    
    // 添加跳线段
    void addJumpSegments(LaserJob& job);
    
    // 拐角减速
    void applyCornerDeceleration(LaserJob& job, double threshold_angle = 30.0);
    
    // 参数更新稀疏化（将点级参数压缩为段级）
    void sparsifyParams(LaserJob& job);
    
    // 路径排序优化
    void optimizePathOrder(LaserJob& job);
};

} // namespace nbcam
