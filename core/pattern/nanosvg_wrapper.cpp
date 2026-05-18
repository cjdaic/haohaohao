#include "nanosvg_wrapper.h"
#include <nanosvg.h>
#include <nanosvgrast.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <map>
#include <spdlog/spdlog.h>

namespace {

struct Vec2d {
    double x = 0.0;
    double y = 0.0;
};

Vec2d midpoint(const Vec2d& a, const Vec2d& b)
{
    return Vec2d{(a.x + b.x) * 0.5, (a.y + b.y) * 0.5};
}

double distanceSqPointToLine(const Vec2d& p, const Vec2d& a, const Vec2d& b)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len_sq = dx * dx + dy * dy;
    if (len_sq <= 1e-24) {
        const double px = p.x - a.x;
        const double py = p.y - a.y;
        return px * px + py * py;
    }
    const double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len_sq;
    const double proj_x = a.x + t * dx;
    const double proj_y = a.y + t * dy;
    const double ex = p.x - proj_x;
    const double ey = p.y - proj_y;
    return ex * ex + ey * ey;
}

void flattenCubicRecursive(const Vec2d& p0,
                           const Vec2d& p1,
                           const Vec2d& p2,
                           const Vec2d& p3,
                           double tol_sq,
                           int depth,
                           std::vector<Vec2d>& out)
{
    constexpr int kMaxDepth = 16;
    const double d1 = distanceSqPointToLine(p1, p0, p3);
    const double d2 = distanceSqPointToLine(p2, p0, p3);
    if ((d1 <= tol_sq && d2 <= tol_sq) || depth >= kMaxDepth) {
        out.push_back(p3);
        return;
    }

    const Vec2d p01 = midpoint(p0, p1);
    const Vec2d p12 = midpoint(p1, p2);
    const Vec2d p23 = midpoint(p2, p3);
    const Vec2d p012 = midpoint(p01, p12);
    const Vec2d p123 = midpoint(p12, p23);
    const Vec2d p0123 = midpoint(p012, p123);

    flattenCubicRecursive(p0, p01, p012, p0123, tol_sq, depth + 1, out);
    flattenCubicRecursive(p0123, p123, p23, p3, tol_sq, depth + 1, out);
}

}  // namespace

namespace nbcam {

// 辅助函数：从字符串中提取属性值
static std::string extractAttribute(const std::string& tag, const std::string& attr_name)
{
    std::string search_str = attr_name + "=\"";
    size_t pos = tag.find(search_str);
    if (pos == std::string::npos) {
        // 尝试查找不带引号的属性
        search_str = attr_name + "=";
        pos = tag.find(search_str);
        if (pos == std::string::npos) return "";
        pos += search_str.length();
    } else {
        pos += search_str.length();
    }
    
    size_t end = tag.find("\"", pos);
    if (end == std::string::npos) {
        // 尝试查找空格或>作为结束
        end = tag.find_first_of(" >", pos);
        if (end == std::string::npos) return "";
    }
    
    return tag.substr(pos, end - pos);
}

// 预处理SVG：展开<use>元素引用
static std::string expandUseElements(const std::string& svg_content)
{
    std::string result = svg_content;
    
    // 查找所有<defs>中的<g id="...">定义
    std::map<std::string, std::string> defs_map;
    
    size_t defs_start = result.find("<defs>");
    if (defs_start != std::string::npos) {
        size_t defs_end = result.find("</defs>", defs_start);
        if (defs_end != std::string::npos) {
            std::string defs_content = result.substr(defs_start + 6, defs_end - defs_start - 6);
            
            // 查找所有<g id="...">定义
            size_t g_start = 0;
            while ((g_start = defs_content.find("<g id=\"", g_start)) != std::string::npos) {
                size_t id_start = g_start + 7;
                size_t id_end = defs_content.find("\"", id_start);
                if (id_end == std::string::npos) break;
                
                std::string id = defs_content.substr(id_start, id_end - id_start);
                
                // 查找对应的</g>结束标签（需要处理嵌套）
                size_t g_content_start = defs_content.find(">", id_end) + 1;
                int depth = 1;
                size_t g_end = g_content_start;
                
                while (depth > 0 && g_end < defs_content.length()) {
                    size_t next_open = defs_content.find("<g", g_end);
                    size_t next_close = defs_content.find("</g>", g_end);
                    
                    if (next_close == std::string::npos) break;
                    
                    if (next_open != std::string::npos && next_open < next_close) {
                        depth++;
                        g_end = next_open + 2;
                    } else {
                        depth--;
                        if (depth == 0) {
                            std::string g_content = defs_content.substr(g_content_start, g_end - g_content_start);
                            defs_map[id] = g_content;
                            g_start = g_end + 4;
                            break;
                        }
                        g_end = next_close + 4;
                    }
                }
                
                if (depth > 0) break;  // 未找到匹配的结束标签
            }
        }
    }
    
    if (defs_map.empty()) {
        spdlog::debug("未找到任何defs定义");
        return result;  // 没有找到定义，直接返回
    }
    
    spdlog::info("找到 {} 个defs定义", defs_map.size());
    
    // 展开所有<use>元素（从后往前替换，避免位置偏移）
    std::vector<std::pair<size_t, size_t>> use_positions;
    size_t search_pos = 0;
    while ((search_pos = result.find("<use", search_pos)) != std::string::npos) {
        size_t use_end = result.find(">", search_pos);
        if (use_end == std::string::npos) break;
        
        // 检查是否是自闭合标签
        bool is_self_closing = (result[use_end - 1] == '/');
        if (!is_self_closing) {
            // 查找</use>结束标签
            size_t close_tag = result.find("</use>", use_end);
            if (close_tag != std::string::npos) {
                use_end = close_tag + 6;
            }
        }
        
        use_positions.push_back({search_pos, use_end - search_pos});
        search_pos = use_end;
    }
    
    // 从后往前替换
    for (auto it = use_positions.rbegin(); it != use_positions.rend(); ++it) {
        size_t use_pos = it->first;
        size_t use_len = it->second;
        std::string use_tag = result.substr(use_pos, use_len);
        
        // 查找href或xlink:href属性
        std::string href_value = extractAttribute(use_tag, "href");
        if (href_value.empty()) {
            href_value = extractAttribute(use_tag, "xlink:href");
        }
        
        if (!href_value.empty() && href_value[0] == '#') {
            std::string ref_id = href_value.substr(1);
            
            // 查找x和y属性
            double x = 0.0, y = 0.0;
            std::string x_str = extractAttribute(use_tag, "x");
            std::string y_str = extractAttribute(use_tag, "y");
            if (!x_str.empty()) {
                try { x = std::stod(x_str); } catch (...) {}
            }
            if (!y_str.empty()) {
                try { y = std::stod(y_str); } catch (...) {}
            }
            
            // 查找对应的定义
            auto def_it = defs_map.find(ref_id);
            if (def_it != defs_map.end()) {
                // 创建transform属性来平移内容
                std::ostringstream replacement;
                replacement << "<g transform=\"translate(" << x << "," << y << ")\">";
                replacement << def_it->second;
                replacement << "</g>";
                
                // 替换<use>标签
                result.replace(use_pos, use_len, replacement.str());
                spdlog::debug("展开use元素: id={}, x={}, y={}", ref_id, x, y);
            } else {
                spdlog::warn("未找到defs定义: id={}", ref_id);
            }
        }
    }
    
    return result;
}

bool NanosvgWrapper::loadFromFile(const std::string& filepath)
{
    clear();
    
    // 读取文件内容
    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::error("无法打开SVG文件: {}", filepath);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string svg_content = buffer.str();
    file.close();
    
    // 预处理：展开<use>元素
    std::string expanded_svg = expandUseElements(svg_content);
    
    // 使用Nanosvg解析
    svg_image_ = nsvgParse(const_cast<char*>(expanded_svg.c_str()), "px", 96.0f);
    if (!svg_image_) {
        spdlog::error("SVG解析失败: {}", filepath);
        return false;
    }
    
    loaded_ = true;
    calculateBounds();
    spdlog::info("SVG文件加载成功: {}", filepath);
    return true;
}

void NanosvgWrapper::calculateBounds()
{
    if (!loaded_ || !svg_image_) {
        bounds_x_min_ = bounds_y_min_ = bounds_x_max_ = bounds_y_max_ = 0.0;
        return;
    }
    
    NSVGimage* image = static_cast<NSVGimage*>(svg_image_);
    
    if (!image->shapes) {
        // 如果没有形状，使用默认边界
        bounds_x_min_ = 0.0;
        bounds_y_min_ = 0.0;
        bounds_x_max_ = image->width;
        bounds_y_max_ = image->height;
        return;
    }
    
    // 计算所有路径的边界框
    bool first = true;
    for (NSVGshape* shape = image->shapes; shape != nullptr; shape = shape->next) {
        for (NSVGpath* path = shape->paths; path != nullptr; path = path->next) {
            if (!path->pts || path->npts < 1) continue;
            
            for (int i = 0; i < path->npts; ++i) {
                float x = path->pts[i * 2];
                float y = path->pts[i * 2 + 1];
                
                if (first) {
                    bounds_x_min_ = bounds_x_max_ = x;
                    bounds_y_min_ = bounds_y_max_ = y;
                    first = false;
                } else {
                    bounds_x_min_ = std::min(bounds_x_min_, static_cast<double>(x));
                    bounds_x_max_ = std::max(bounds_x_max_, static_cast<double>(x));
                    bounds_y_min_ = std::min(bounds_y_min_, static_cast<double>(y));
                    bounds_y_max_ = std::max(bounds_y_max_, static_cast<double>(y));
                }
            }
        }
    }
    
    // 如果没有找到点，使用图像尺寸
    if (first) {
        bounds_x_min_ = 0.0;
        bounds_y_min_ = 0.0;
        bounds_x_max_ = image->width;
        bounds_y_max_ = image->height;
    }
}

std::vector<SVGPathPoint> NanosvgWrapper::getPathPoints(double tolerance) const
{
    std::vector<SVGPathPoint> points;
    
    if (!loaded_ || !svg_image_) {
        spdlog::warn("SVG未加载，无法获取路径点");
        return points;
    }
    
    NSVGimage* image = static_cast<NSVGimage*>(svg_image_);
    
    if (!image) {
        spdlog::warn("SVG图像指针为空");
        return points;
    }
    
    // 统计信息
    int shape_count = 0;
    int path_count = 0;
    int total_points = 0;
    
    // 遍历所有形状和路径
    for (NSVGshape* shape = image->shapes; shape != nullptr; shape = shape->next) {
        shape_count++;
        
        // 检查形状是否可见（有填充或描边）
        bool has_fill = shape->fill.type != NSVG_PAINT_NONE;
        bool has_stroke = shape->stroke.type != NSVG_PAINT_NONE;
        
        if (!has_fill && !has_stroke) {
            spdlog::debug("跳过不可见形状: fill={}, stroke={}", 
                         shape->fill.type, shape->stroke.type);
            continue;
        }
        
        for (NSVGpath* path = shape->paths; path != nullptr; path = path->next) {
            path_count++;
            
            if (!path->pts || path->npts < 1) {
                spdlog::debug("跳过点数不足的路径: npts={}", path->npts);
                continue;
            }
            
            // 第一个点标记为MoveTo
            SVGPathPoint pt0;
            pt0.x = path->pts[0];
            pt0.y = path->pts[1];
            pt0.is_move_to = true;
            points.push_back(pt0);
            total_points++;
            
            // 处理路径点（Nanosvg已经将曲线离散化为线段）
            for (int i = 1; i < path->npts; ++i) {
                SVGPathPoint pt;
                pt.x = path->pts[i * 2];
                pt.y = path->pts[i * 2 + 1];
                pt.is_move_to = false;
                points.push_back(pt);
                total_points++;
            }
        }
    }
    
    spdlog::info("SVG路径解析完成: {} 个形状, {} 个路径, {} 个路径点", 
                 shape_count, path_count, total_points);
    
    if (points.empty()) {
        spdlog::warn("未找到任何路径点。SVG图像尺寸: {}x{}, 形状数量: {}", 
                     image->width, image->height, shape_count);
    }
    
    return points;
}

std::vector<SVGShapePaths> NanosvgWrapper::getFilledShapePaths(double tolerance) const
{
    std::vector<SVGShapePaths> result;
    if (!loaded_ || !svg_image_) {
        return result;
    }

    const double tol = std::max(1e-4, tolerance);
    const double tol_sq = tol * tol;
    constexpr double kMergeTolSq = 1e-12;

    NSVGimage* image = static_cast<NSVGimage*>(svg_image_);
    for (NSVGshape* shape = image->shapes; shape != nullptr; shape = shape->next) {
        const bool is_visible = (shape->flags & NSVG_FLAGS_VISIBLE) != 0;
        const bool has_fill = (shape->fill.type != NSVG_PAINT_NONE);
        if (!is_visible || !has_fill || !shape->paths) {
            continue;
        }

        SVGShapePaths shape_paths;
        shape_paths.fill_rule = (shape->fillRule == NSVG_FILLRULE_EVENODD)
                                    ? SVGFillRule::EVENODD
                                    : SVGFillRule::NONZERO;

        for (NSVGpath* path = shape->paths; path != nullptr; path = path->next) {
            if (!path->pts || path->npts < 4 || ((path->npts - 1) % 3) != 0) {
                continue;
            }

            std::vector<Vec2d> polyline;
            polyline.reserve(static_cast<size_t>(path->npts) * 2);
            polyline.push_back(Vec2d{path->pts[0], path->pts[1]});

            for (int i = 0; i + 3 < path->npts; i += 3) {
                const Vec2d p0{path->pts[(i + 0) * 2], path->pts[(i + 0) * 2 + 1]};
                const Vec2d p1{path->pts[(i + 1) * 2], path->pts[(i + 1) * 2 + 1]};
                const Vec2d p2{path->pts[(i + 2) * 2], path->pts[(i + 2) * 2 + 1]};
                const Vec2d p3{path->pts[(i + 3) * 2], path->pts[(i + 3) * 2 + 1]};
                flattenCubicRecursive(p0, p1, p2, p3, tol_sq, 0, polyline);
            }

            if (polyline.size() < 3) {
                continue;
            }

            std::vector<Vec2d> deduped;
            deduped.reserve(polyline.size());
            deduped.push_back(polyline.front());
            for (size_t i = 1; i < polyline.size(); ++i) {
                const auto& prev = deduped.back();
                const auto& curr = polyline[i];
                const double dx = curr.x - prev.x;
                const double dy = curr.y - prev.y;
                if ((dx * dx + dy * dy) > kMergeTolSq) {
                    deduped.push_back(curr);
                }
            }

            if (deduped.size() < 3) {
                continue;
            }

            const auto& first = deduped.front();
            const auto& last = deduped.back();
            const double close_dx = first.x - last.x;
            const double close_dy = first.y - last.y;
            if (!path->closed || (close_dx * close_dx + close_dy * close_dy) > kMergeTolSq) {
                deduped.push_back(first);
            }

            SVGPathLoop loop;
            loop.points.reserve(deduped.size());
            for (size_t i = 0; i < deduped.size(); ++i) {
                SVGPathPoint pt;
                pt.x = deduped[i].x;
                pt.y = deduped[i].y;
                pt.is_move_to = (i == 0);
                loop.points.push_back(pt);
            }

            if (loop.points.size() >= 4) {
                shape_paths.loops.push_back(std::move(loop));
            }
        }

        if (!shape_paths.loops.empty()) {
            result.push_back(std::move(shape_paths));
        }
    }

    return result;
}

void NanosvgWrapper::getBounds(double& x_min, double& y_min, double& x_max, double& y_max) const
{
    x_min = bounds_x_min_;
    y_min = bounds_y_min_;
    x_max = bounds_x_max_;
    y_max = bounds_y_max_;
}

void NanosvgWrapper::getCanvasBounds(double& x_min, double& y_min, double& x_max, double& y_max) const
{
    x_min = 0.0;
    y_min = 0.0;
    x_max = 0.0;
    y_max = 0.0;
    if (!loaded_ || !svg_image_) {
        return;
    }

    const NSVGimage* image = static_cast<const NSVGimage*>(svg_image_);
    if (!image) {
        return;
    }

    x_max = static_cast<double>(image->width);
    y_max = static_cast<double>(image->height);
}

unsigned char* NanosvgWrapper::renderToImage(int width, int height) const
{
    if (!loaded_ || !svg_image_ || width <= 0 || height <= 0) {
        return nullptr;
    }
    
    NSVGimage* image = static_cast<NSVGimage*>(svg_image_);
    
    // 分配图像数据（RGBA，每像素4字节）
    unsigned char* img_data = new unsigned char[width * height * 4];
    if (!img_data) {
        spdlog::error("无法分配图像内存");
        return nullptr;
    }
    
    // 创建rasterizer
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (!rast) {
        delete[] img_data;
        spdlog::error("无法创建rasterizer");
        return nullptr;
    }
    
    // 计算缩放因子以适应图像尺寸
    float scale = 1.0f;
    if (image->width > 0 && image->height > 0) {
        float scale_x = static_cast<float>(width) / image->width;
        float scale_y = static_cast<float>(height) / image->height;
        scale = std::min(scale_x, scale_y);  // 保持宽高比
    }
    
    // 渲染SVG到图像
    nsvgRasterize(rast, image, 0, 0, scale, img_data, width, height, width * 4);
    
    // 清理rasterizer
    nsvgDeleteRasterizer(rast);
    
    spdlog::info("SVG渲染为图像完成: {}x{} 像素", width, height);
    
    return img_data;
}

void NanosvgWrapper::clear()
{
    if (svg_image_) {
        nsvgDelete(static_cast<NSVGimage*>(svg_image_));
        svg_image_ = nullptr;
    }
    loaded_ = false;
    bounds_x_min_ = bounds_y_min_ = bounds_x_max_ = bounds_y_max_ = 0.0;
}

} // namespace nbcam
