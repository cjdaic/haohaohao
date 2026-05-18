#include "svg_to_uv_mapper.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace nbcam {

void SVGToUVMapper::mapToUV(const std::vector<SVGPathPoint>& svg_points,
                            std::vector<UVPathPoint>& uv_path,
                            double center_u,
                            double center_v,
                            double scale,
                            bool preserve_aspect) const {
    uv_path.clear();
    
    if (svg_points.empty()) {
        return;
    }
    
    // 计算SVG边界框
    double svg_x_min = svg_points[0].x;
    double svg_x_max = svg_points[0].x;
    double svg_y_min = svg_points[0].y;
    double svg_y_max = svg_points[0].y;
    
    for (const auto& pt : svg_points) {
        svg_x_min = std::min(svg_x_min, pt.x);
        svg_x_max = std::max(svg_x_max, pt.x);
        svg_y_min = std::min(svg_y_min, pt.y);
        svg_y_max = std::max(svg_y_max, pt.y);
    }
    
    double svg_width = svg_x_max - svg_x_min;
    double svg_height = svg_y_max - svg_y_min;
    
    if (svg_width <= 0 || svg_height <= 0) {
        spdlog::warn("SVG边界框无效");
        return;
    }
    
    // 计算缩放因子
    double scale_x = scale / svg_width;
    double scale_y = scale / svg_height;
    
    if (preserve_aspect) {
        // 保持宽高比，使用较小的缩放因子
        double scale_min = std::min(scale_x, scale_y);
        scale_x = scale_min;
        scale_y = scale_min;
    }
    
    // 计算SVG中心点
    double svg_center_x = svg_x_min + svg_width * 0.5;
    double svg_center_y = svg_y_min + svg_height * 0.5;
    
    // 转换每个点
    for (const auto& svg_pt : svg_points) {
        // 检查输入点是否为NaN
        if (std::isnan(svg_pt.x) || std::isnan(svg_pt.y)) {
            spdlog::warn("跳过NaN SVG点");
            continue;
        }
        
        UVPathPoint uv_pt;
        
        // 计算相对于SVG中心的偏移
        double dx = svg_pt.x - svg_center_x;
        double dy = svg_pt.y - svg_center_y;
        
        // 应用缩放并映射到UV空间（中心对齐）
        uv_pt.u = center_u + dx * scale_x;
        
        // Y轴翻转：SVG的Y向下，UV的V向上
        // 翻转dy的符号，然后加上center_v
        uv_pt.v = center_v - dy * scale_y;
        
        // 检查结果是否为NaN
        if (std::isnan(uv_pt.u) || std::isnan(uv_pt.v)) {
            spdlog::warn("UV映射产生NaN，跳过");
            continue;
        }
        
        uv_path.push_back(uv_pt);
    }
    
    spdlog::info("SVG到UV映射完成: {} 个点", uv_path.size());
}

void SVGToUVMapper::tileToUV(const std::vector<SVGPathPoint>& svg_points,
                             std::vector<UVPathPoint>& uv_path,
                             double tile_u,
                             double tile_v,
                             const std::vector<double>& uv_bounds) const {
    uv_path.clear();
    
    if (svg_points.empty() || tile_u <= 0 || tile_v <= 0) {
        return;
    }
    
    // 计算SVG边界框
    double svg_x_min = svg_points[0].x;
    double svg_x_max = svg_points[0].x;
    double svg_y_min = svg_points[0].y;
    double svg_y_max = svg_points[0].y;
    
    for (const auto& pt : svg_points) {
        svg_x_min = std::min(svg_x_min, pt.x);
        svg_x_max = std::max(svg_x_max, pt.x);
        svg_y_min = std::min(svg_y_min, pt.y);
        svg_y_max = std::max(svg_y_max, pt.y);
    }
    
    double svg_width = svg_x_max - svg_x_min;
    double svg_height = svg_y_max - svg_y_min;
    
    if (svg_width <= 0 || svg_height <= 0) {
        spdlog::warn("SVG边界框无效");
        return;
    }
    
    // 确定UV边界
    double u_min = 0.0, v_min = 0.0, u_max = 1.0, v_max = 1.0;
    if (uv_bounds.size() >= 4) {
        u_min = uv_bounds[0];
        v_min = uv_bounds[1];
        u_max = uv_bounds[2];
        v_max = uv_bounds[3];
    }
    
    double uv_width = u_max - u_min;
    double uv_height = v_max - v_min;
    
    // 计算平铺数量
    int tiles_x = static_cast<int>(std::ceil(uv_width / tile_u));
    int tiles_y = static_cast<int>(std::ceil(uv_height / tile_v));
    
    // 计算每个SVG图案在UV空间中的缩放因子（保持宽高比）
    double scale_x = tile_u / svg_width;
    double scale_y = tile_v / svg_height;
    double scale = std::min(scale_x, scale_y);  // 保持宽高比
    
    // 计算每个平铺单元的实际UV尺寸（考虑宽高比）
    double actual_tile_u = svg_width * scale;
    double actual_tile_v = svg_height * scale;
    
    // 平铺SVG图案
    for (int ty = 0; ty < tiles_y; ++ty) {
        for (int tx = 0; tx < tiles_x; ++tx) {
            // 计算当前平铺单元的UV起始位置
            double base_u = u_min + tx * actual_tile_u;
            double base_v = v_min + ty * actual_tile_v;
            
            // 如果超出边界，跳过
            if (base_u >= u_max || base_v >= v_max) {
                continue;
            }
            
            // 转换SVG点到当前平铺单元
            for (const auto& svg_pt : svg_points) {
                // 检查输入点是否为NaN
                if (std::isnan(svg_pt.x) || std::isnan(svg_pt.y)) {
                    spdlog::warn("跳过NaN SVG点");
                    continue;
                }
                
                UVPathPoint uv_pt;
                
                // 将SVG坐标归一化并缩放到平铺单元
                double normalized_x = (svg_pt.x - svg_x_min) / svg_width;
                double normalized_y = (svg_pt.y - svg_y_min) / svg_height;
                
                // 检查归一化结果是否为NaN
                if (std::isnan(normalized_x) || std::isnan(normalized_y)) {
                    spdlog::warn("归一化产生NaN，跳过");
                    continue;
                }
                
                // 映射到UV空间（考虑Y轴翻转）
                uv_pt.u = base_u + normalized_x * actual_tile_u;
                uv_pt.v = base_v + (1.0 - normalized_y) * actual_tile_v;  // Y轴翻转
                
                // 检查最终UV坐标是否为NaN
                if (std::isnan(uv_pt.u) || std::isnan(uv_pt.v)) {
                    spdlog::warn("UV映射产生NaN，跳过");
                    continue;
                }
                
                uv_path.push_back(uv_pt);
            }
        }
    }
    
    spdlog::info("SVG平铺完成: {}x{} 个平铺单元, {} 个UV路径点", tiles_x, tiles_y, uv_path.size());
}

double SVGToUVMapper::calculateAspectRatio(const std::vector<SVGPathPoint>& svg_points) const {
    if (svg_points.empty()) {
        return 1.0;
    }
    
    double x_min = svg_points[0].x, x_max = svg_points[0].x;
    double y_min = svg_points[0].y, y_max = svg_points[0].y;
    
    for (const auto& pt : svg_points) {
        x_min = std::min(x_min, pt.x);
        x_max = std::max(x_max, pt.x);
        y_min = std::min(y_min, pt.y);
        y_max = std::max(y_max, pt.y);
    }
    
    double width = x_max - x_min;
    double height = y_max - y_min;
    
    if (height <= 0) return 1.0;
    return width / height;
}

} // namespace nbcam
