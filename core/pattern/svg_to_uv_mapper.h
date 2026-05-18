#pragma once

#include "nanosvg_wrapper.h"
#include "../pattern/fill_strategy_interface.h"
#include <vector>

namespace nbcam {

// SVG到UV坐标映射器
class SVGToUVMapper {
public:
    SVGToUVMapper() = default;
    ~SVGToUVMapper() = default;
    
    // 将SVG路径点映射到UV空间
    // svg_points: SVG路径点
    // uv_path: 输出的UV路径点
    // center_u, center_v: UV空间中的中心位置（默认0.5, 0.5）
    // scale: 缩放因子（默认1.0，表示填充整个UV空间）
    // preserve_aspect: 是否保持宽高比（默认true）
    void mapToUV(const std::vector<SVGPathPoint>& svg_points,
                 std::vector<UVPathPoint>& uv_path,
                 double center_u = 0.5,
                 double center_v = 0.5,
                 double scale = 1.0,
                 bool preserve_aspect = true) const;
    
    // 将SVG图案平铺到UV空间
    // svg_points: SVG路径点
    // uv_path: 输出的UV路径点
    // tile_u, tile_v: 每个平铺单元的UV尺寸（默认0.1, 0.1）
    // uv_bounds: UV空间的边界（u_min, v_min, u_max, v_max），如果为空则使用整个UV空间[0,1]
    void tileToUV(const std::vector<SVGPathPoint>& svg_points,
                  std::vector<UVPathPoint>& uv_path,
                  double tile_u = 0.1,
                  double tile_v = 0.1,
                  const std::vector<double>& uv_bounds = {}) const;

private:
    // 计算SVG的宽高比
    double calculateAspectRatio(const std::vector<SVGPathPoint>& svg_points) const;
};

} // namespace nbcam
