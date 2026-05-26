#pragma once

#include <string>
#include <vector>
#include "../pattern/fill_strategy_interface.h"

namespace nbcam {

// SVG路径点（2D坐标）
struct SVGPathPoint {
    double x, y;
    double grayscale = 0.0;  // 由填充/描边颜色换算的灰度强度 [0,1]
    bool is_move_to = false;  // 是否为MoveTo命令
};

enum class SVGFillRule {
    NONZERO,
    EVENODD
};

struct SVGPathLoop {
    std::vector<SVGPathPoint> points;  // 单个闭合轮廓（首点标记 move_to）
};

struct SVGShapePaths {
    SVGFillRule fill_rule = SVGFillRule::NONZERO;
    std::vector<SVGPathLoop> loops;    // 该shape下的所有轮廓
};

// Nanosvg包装器，用于解析SVG文件
class NanosvgWrapper {
public:
    NanosvgWrapper() = default;
    ~NanosvgWrapper() = default;
    
    // 从文件加载SVG
    bool loadFromFile(const std::string& filepath);
    
    // 获取所有路径点（已离散化）
    std::vector<SVGPathPoint> getPathPoints(double tolerance = 0.01) const;

    // 获取按shape分组的填充轮廓（用于精确边界提取）
    std::vector<SVGShapePaths> getFilledShapePaths(double tolerance = 0.5) const;
    
    // 获取SVG边界框
    void getBounds(double& x_min, double& y_min, double& x_max, double& y_max) const;

    // 获取SVG画布边界（通常为 [0,0]-[width,height]）
    void getCanvasBounds(double& x_min, double& y_min, double& x_max, double& y_max) const;
    
    // 将SVG渲染为图像（用于纹理贴图）
    // width, height: 输出图像尺寸
    // 返回RGBA图像数据（每像素4字节：R, G, B, A），调用者负责释放内存
    unsigned char* renderToImage(int width, int height) const;
    
    // 清除数据
    void clear();
    
    // 检查是否已加载
    bool isLoaded() const { return loaded_; }

private:
    bool loaded_ = false;
    void* svg_image_ = nullptr;  // NSVGimage* (forward declaration to avoid including nanosvg.h here)
    
    // SVG边界框
    double bounds_x_min_ = 0.0;
    double bounds_y_min_ = 0.0;
    double bounds_x_max_ = 0.0;
    double bounds_y_max_ = 0.0;
    
    // 解析SVG内容
    bool parseSVG(const std::string& filepath);
    
    // 计算边界框
    void calculateBounds();
};

} // namespace nbcam
