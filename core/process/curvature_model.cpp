#include "curvature_model.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace nbcam {

double CurvatureModel::computeCurvature(const TriangleMesh& mesh, size_t vertex_index) const {
    // 曲率计算（简化实现）
    // 完整实现需要计算高斯曲率或平均曲率
    if (vertex_index >= mesh.vertices.size()) {
        return 0.0;
    }
    
    const auto& v = mesh.vertices[vertex_index];
    (void)v;
    
    // 简化：使用法向量变化来估算曲率
    // 完整实现需要遍历邻接面并计算曲率张量
    
    return 0.0;  // 占位符
}

void CurvatureModel::assignProcessParams(
    const TriangleMesh& mesh,
    std::vector<PathPoint>& points) {
    
    // 基于曲率分配工艺参数
    for (auto& point : points) {
        // 查找最近的顶点
        double min_dist = 1e9;
        size_t nearest_vertex = 0;
        
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const auto& v = mesh.vertices[i];
            double dx = point.x - v.x;
            double dy = point.y - v.y;
            double dz = point.z - v.z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            
            if (dist < min_dist) {
                min_dist = dist;
                nearest_vertex = i;
            }
        }
        
        // 计算曲率
        double curvature = computeCurvature(mesh, nearest_vertex);
        
        // 根据曲率映射到功率和速度
        // 曲率越大，功率越高，速度越低
        double curvature_norm = std::max(0.0, std::min(1.0, curvature));  // 归一化到[0,1]
        
        if (!point.params_override) {
            point.params_override = std::make_unique<ProcessParams>();
        }
        
        point.params_override->power_w = min_power_ + curvature_norm * (max_power_ - min_power_);
        point.params_override->speed_mm_s = max_speed_ - curvature_norm * (max_speed_ - min_speed_);
        point.params_override->freq_hz = 20000;  // 默认频率
    }
    
    spdlog::info("基于曲率分配工艺参数完成，点数: {}", points.size());
}

} // namespace nbcam
