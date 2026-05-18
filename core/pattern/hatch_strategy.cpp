#include "hatch_strategy.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace nbcam {

std::vector<UVPathPoint> HatchStrategy::generatePath(
    const std::vector<UVCoord>& boundary,
    double spacing,
    double angle) {
    
    std::vector<UVPathPoint> path;
    
    // 栅格填充：生成平行线
    // 简化实现，完整实现需要多边形裁剪
    
    if (boundary.empty()) {
        return path;
    }
    
    // 计算边界框
    double u_min = boundary[0].u, u_max = boundary[0].u;
    double v_min = boundary[0].v, v_max = boundary[0].v;
    
    for (const auto& uv : boundary) {
        u_min = std::min(u_min, uv.u);
        u_max = std::max(u_max, uv.u);
        v_min = std::min(v_min, uv.v);
        v_max = std::max(v_max, uv.v);
    }
    
    // 生成平行线
    double cos_a = std::cos(angle);
    double sin_a = std::sin(angle);
    
    int line_count = static_cast<int>((v_max - v_min) / spacing) + 1;
    
    for (int i = 0; i < line_count; ++i) {
        double v = v_min + i * spacing;
        
        // 计算线与边界的交点（简化实现）
        UVPathPoint p1, p2;
        p1.u = u_min;
        p1.v = v;
        p2.u = u_max;
        p2.v = v;
        
        path.push_back(p1);
        path.push_back(p2);
    }
    
    spdlog::info("生成栅格路径，点数: {}", path.size());
    
    return path;
}

} // namespace nbcam
