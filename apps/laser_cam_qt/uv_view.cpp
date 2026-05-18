#include "uv_view.h"
#include "../core/param/parameterizer_interface.h"
#include "../core/pattern/fill_strategy_interface.h"
#include "../core/mesh/mesh_io.h"
#include "../core/job/laser_job.h"
#include "../core/pattern/nanosvg_wrapper.h"
#include "svg_raster_cache.h"
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QImage>
#include <QPolygonF>
#include <QPainterPath>
#include <QTransform>
#include <QFont>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr double kMinRange = 1e-9;
constexpr int kMaxFallbackTextureSize = 4096;
constexpr double kMaxScreenMagnitude = 1e6;
constexpr size_t kMaxDisplayPathPoints = 20000;

struct UvAspectFitFrame {
    double u_min = 0.0;
    double u_max = 1.0;
    double v_min = 0.0;
    double v_max = 1.0;
    double u_range = 1.0;
    double v_range = 1.0;
};

bool computeUvAspectFitFrame(double patch_u_min,
                             double patch_u_max,
                             double patch_v_min,
                             double patch_v_max,
                             double content_aspect_wh,
                             UvAspectFitFrame& out)
{
    const double patch_u_range = patch_u_max - patch_u_min;
    const double patch_v_range = patch_v_max - patch_v_min;
    if (!(patch_u_range > 1e-9) || !(patch_v_range > 1e-9) ||
        !(content_aspect_wh > 1e-9) || !std::isfinite(content_aspect_wh)) {
        return false;
    }

    const double patch_center_u = (patch_u_min + patch_u_max) * 0.5;
    const double patch_center_v = (patch_v_min + patch_v_max) * 0.5;
    const double patch_aspect = patch_u_range / patch_v_range;

    double fit_u = patch_u_range;
    double fit_v = patch_v_range;
    if (patch_aspect > content_aspect_wh) {
        fit_v = patch_v_range;
        fit_u = fit_v * content_aspect_wh;
    } else {
        fit_u = patch_u_range;
        fit_v = fit_u / content_aspect_wh;
    }

    out.u_range = fit_u;
    out.v_range = fit_v;
    out.u_min = patch_center_u - fit_u * 0.5;
    out.u_max = patch_center_u + fit_u * 0.5;
    out.v_min = patch_center_v - fit_v * 0.5;
    out.v_max = patch_center_v + fit_v * 0.5;
    return out.u_range > 1e-9 && out.v_range > 1e-9;
}

std::pair<int, int> computeAspectFittedRasterSize(int requested_w,
                                                   int requested_h,
                                                   double content_aspect_wh)
{
    int out_w = std::max(1, requested_w);
    int out_h = std::max(1, requested_h);
    if (!(content_aspect_wh > kMinRange) || !std::isfinite(content_aspect_wh)) {
        return {out_w, out_h};
    }

    const double box_aspect = static_cast<double>(out_w) / static_cast<double>(out_h);
    if (box_aspect > content_aspect_wh) {
        out_h = std::max(1, requested_h);
        out_w = std::max(1, static_cast<int>(std::lround(static_cast<double>(out_h) * content_aspect_wh)));
    } else {
        out_w = std::max(1, requested_w);
        out_h = std::max(1, static_cast<int>(std::lround(static_cast<double>(out_w) / content_aspect_wh)));
    }

    out_w = std::clamp(out_w, 1, std::max(1, requested_w));
    out_h = std::clamp(out_h, 1, std::max(1, requested_h));
    return {out_w, out_h};
}

bool isFinitePoint(const QPointF& p)
{
    return std::isfinite(p.x()) && std::isfinite(p.y());
}

bool isReasonableScreenPoint(const QPointF& p)
{
    return isFinitePoint(p) &&
           std::abs(p.x()) <= kMaxScreenMagnitude &&
           std::abs(p.y()) <= kMaxScreenMagnitude;
}
}  // namespace

UVView::UVView(QWidget *parent)
    : QWidget(parent)
    , u_min_(0.0), u_max_(1.0)
    , v_min_(0.0), v_max_(1.0)
    , scale_x_(1.0), scale_y_(1.0)
    , offset_x_(0.0), offset_y_(0.0)
    , has_bounds_(false)
    , wireframe_mode_(false)
    , validation_mode_(false)
    , highlighted_vertex_index_(static_cast<size_t>(-1))
    , mesh_info_(nullptr)
    , current_job_(nullptr)
{
    setMinimumHeight(200);
    setMouseTracking(true);  // 启用鼠标跟踪
}

UVView::~UVView() = default;

void UVView::setUVCoords(const std::vector<nbcam::UVCoord>& coords)
{
    uv_coords_ = coords;
    updateTransform();
    invalidateHighlightCache();
    update();
}

void UVView::setUVPath(const std::vector<nbcam::UVPathPoint>& path)
{
    uv_path_.clear();
    if (path.empty()) {
        invalidateHighlightCache();
        update();
        return;
    }

    // 仅用于显示，做抽样避免大规模QPainter绘制触发Qt6Gui崩溃。
    if (path.size() > kMaxDisplayPathPoints) {
        const size_t step = std::max<size_t>(1, path.size() / kMaxDisplayPathPoints);
        uv_path_.reserve((path.size() + step - 1) / step);
        for (size_t i = 0; i < path.size(); i += step) {
            uv_path_.push_back(path[i]);
        }
        if (uv_path_.back().u != path.back().u || uv_path_.back().v != path.back().v) {
            uv_path_.push_back(path.back());
        }
    } else {
        uv_path_ = path;
    }
    invalidateHighlightCache();
    update();
}

void UVView::setTriangles(const std::vector<nbcam::Triangle>& triangles)
{
    triangles_ = triangles;
    update();
}

void UVView::setTexture(int patch_id, const std::string& svg_filepath,
                        double u_min, double u_max, double v_min, double v_max,
                        double scale_x, double scale_y,
                        double translate_x, double translate_y,
                        double rotation_deg)
{
    if (patch_id < 0) {
        return;
    }

    if (!std::isfinite(u_min) || !std::isfinite(u_max) ||
        !std::isfinite(v_min) || !std::isfinite(v_max) ||
        !std::isfinite(scale_x) || !std::isfinite(scale_y) ||
        !std::isfinite(translate_x) || !std::isfinite(translate_y) ||
        !std::isfinite(rotation_deg) ||
        u_max <= u_min || v_max <= v_min) {
        textures_.erase(patch_id);
        update();
        return;
    }

    TextureData& tex = textures_[patch_id];
    tex.svg_filepath = svg_filepath;
    tex.u_min = u_min;
    tex.u_max = u_max;
    tex.v_min = v_min;
    tex.v_max = v_max;
    tex.scale_x = scale_x;
    tex.scale_y = scale_y;
    tex.translate_x = translate_x;
    tex.translate_y = translate_y;
    tex.rotation_deg = rotation_deg;
    tex.content_aspect_wh = 1.0;
    tex.image = QImage();

    nbcam::NanosvgWrapper svg_parser;
    if (svg_parser.loadFromFile(svg_filepath)) {
        double x_min = 0.0, y_min = 0.0, x_max = 0.0, y_max = 0.0;
        svg_parser.getCanvasBounds(x_min, y_min, x_max, y_max);
        double w = x_max - x_min;
        double h = y_max - y_min;
        if (!(w > kMinRange) || !(h > kMinRange)) {
            svg_parser.getBounds(x_min, y_min, x_max, y_max);
            w = x_max - x_min;
            h = y_max - y_min;
        }
        if (w > kMinRange && h > kMinRange) {
            tex.content_aspect_wh = w / h;
        }
    }

    const int preview_max = 512;
    auto preview_size = std::make_pair(preview_max, preview_max);
    if (tex.content_aspect_wh > kMinRange) {
        preview_size = computeAspectFittedRasterSize(preview_max, preview_max, tex.content_aspect_wh);
    }

    QImage image;
    if (loadSvgImageCached(svg_filepath, preview_size.first, preview_size.second, image)) {
        tex.image = std::move(image);
    }

    if (!(tex.content_aspect_wh > kMinRange) && !tex.image.isNull() && tex.image.height() > 0) {
        tex.content_aspect_wh =
            static_cast<double>(tex.image.width()) / static_cast<double>(tex.image.height());
    }
    
    update();
}

void UVView::removeTexture(int patch_id)
{
    textures_.erase(patch_id);
    update();
}

void UVView::clear()
{
    uv_coords_.clear();
    uv_path_.clear();
    triangles_.clear();
    textures_.clear();
    job_segment_indices_.clear();
    highlighted_segment_ids_.clear();
    invalidateHighlightCache();
    has_bounds_ = false;
    update();
}

void UVView::updateTransform()
{
    if (uv_coords_.empty()) {
        has_bounds_ = false;
        return;
    }
    
    bool has_valid_coord = false;
    
    for (const auto& coord : uv_coords_) {
        if (!std::isfinite(coord.u) || !std::isfinite(coord.v)) {
            continue;
        }
        if (!has_valid_coord) {
            u_min_ = u_max_ = coord.u;
            v_min_ = v_max_ = coord.v;
            has_valid_coord = true;
            continue;
        }
        u_min_ = std::min(u_min_, coord.u);
        u_max_ = std::max(u_max_, coord.u);
        v_min_ = std::min(v_min_, coord.v);
        v_max_ = std::max(v_max_, coord.v);
    }

    if (!has_valid_coord) {
        has_bounds_ = false;
        return;
    }
    
    // 添加边距
    double u_range = u_max_ - u_min_;
    double v_range = v_max_ - v_min_;
    if (!std::isfinite(u_range) || !std::isfinite(v_range) ||
        u_range <= kMinRange || v_range <= kMinRange) {
        has_bounds_ = false;
        return;
    }

    double margin = std::max(u_range, v_range) * 0.1;
    if (!std::isfinite(margin)) {
        has_bounds_ = false;
        return;
    }
    
    u_min_ -= margin;
    u_max_ += margin;
    v_min_ -= margin;
    v_max_ += margin;
    
    // 计算缩放和偏移
    int w = width();
    int h = height();
    
    if (w > 0 && h > 0) {
        scale_x_ = (w - 40) / (u_max_ - u_min_);
        scale_y_ = (h - 40) / (v_max_ - v_min_);
        
        // 保持纵横比
        double scale = std::min(scale_x_, scale_y_);
        if (!std::isfinite(scale) || scale <= 0.0) {
            has_bounds_ = false;
            return;
        }
        scale_x_ = scale_y_ = scale;
        
        offset_x_ = 20 + (w - 40 - (u_max_ - u_min_) * scale_x_) / 2;
        offset_y_ = 20 + (h - 40 - (v_max_ - v_min_) * scale_y_) / 2;
        if (!std::isfinite(offset_x_) || !std::isfinite(offset_y_)) {
            has_bounds_ = false;
            return;
        }
    } else {
        has_bounds_ = false;
        return;
    }
    
    has_bounds_ = true;
}

QPointF UVView::uvToScreen(double u, double v) const
{
    if (!has_bounds_) {
        return QPointF(0, 0);
    }
    
    // 检查输入与变换是否为有限值
    if (!std::isfinite(u) || !std::isfinite(v) ||
        !std::isfinite(scale_x_) || !std::isfinite(scale_y_) ||
        !std::isfinite(offset_x_) || !std::isfinite(offset_y_)) {
        return QPointF(0, 0);
    }
    
    double x = offset_x_ + (u - u_min_) * scale_x_;
    double y = offset_y_ + (v_max_ - v) * scale_y_;  // Y轴翻转
    
    // 检查计算结果是否有效
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return QPointF(0, 0);
    }
    
    return QPointF(x, y);
}

QPointF UVView::screenToUV(int x, int y) const
{
    if (!has_bounds_) {
        return QPointF(0, 0);
    }

    if (!std::isfinite(scale_x_) || !std::isfinite(scale_y_) ||
        std::abs(scale_x_) <= kMinRange || std::abs(scale_y_) <= kMinRange) {
        return QPointF(0, 0);
    }
    
    double u = u_min_ + (x - offset_x_) / scale_x_;
    double v = v_max_ - (y - offset_y_) / scale_y_;  // Y轴翻转

    if (!std::isfinite(u) || !std::isfinite(v)) {
        return QPointF(0, 0);
    }
    
    return QPointF(u, v);
}

size_t UVView::findNearestVertex(double u, double v) const
{
    if (uv_coords_.empty()) {
        return static_cast<size_t>(-1);
    }
    
    double min_dist = std::numeric_limits<double>::max();
    size_t nearest_idx = static_cast<size_t>(-1);
    
    for (size_t i = 0; i < uv_coords_.size(); ++i) {
        const auto& coord = uv_coords_[i];
        if (!std::isfinite(coord.u) || !std::isfinite(coord.v)) {
            continue;
        }
        
        double dist = std::sqrt(
            (coord.u - u) * (coord.u - u) + 
            (coord.v - v) * (coord.v - v)
        );
        
        // 转换为屏幕距离（更直观）
        QPointF screen_coord = uvToScreen(coord.u, coord.v);
        QPointF screen_click = uvToScreen(u, v);
        double screen_dist = std::sqrt(
            (screen_coord.x() - screen_click.x()) * (screen_coord.x() - screen_click.x()) +
            (screen_coord.y() - screen_click.y()) * (screen_coord.y() - screen_click.y())
        );
        if (!std::isfinite(screen_dist)) {
            continue;
        }
        
        // 使用屏幕距离，阈值10像素
        if (screen_dist < 10.0 && screen_dist < min_dist) {
            min_dist = screen_dist;
            nearest_idx = i;
        }
    }
    
    return nearest_idx;
}

void UVView::setValidationMode(bool enabled)
{
    validation_mode_ = enabled;
    if (!enabled) {
        highlighted_vertex_index_ = static_cast<size_t>(-1);
    }
    update();
}

void UVView::setMeshInfo(nbcam::TriangleMesh* mesh, const std::vector<nbcam::Triangle>& triangles)
{
    mesh_info_ = mesh;
    triangles_info_ = triangles;
}

void UVView::setJob(nbcam::LaserJob* job)
{
    current_job_ = job;
    job_segment_indices_.clear();
    if (current_job_) {
        job_segment_indices_.reserve(current_job_->segments.size());
        for (size_t i = 0; i < current_job_->segments.size(); ++i) {
            job_segment_indices_[current_job_->segments[i].id] = i;
        }
    }
    invalidateHighlightCache();
    update();
}

void UVView::setHighlightedPathSegments(const std::vector<int>& segment_ids)
{
    std::unordered_set<int> next_highlighted_segment_ids;
    next_highlighted_segment_ids.reserve(segment_ids.size());
    for (int id : segment_ids) {
        next_highlighted_segment_ids.insert(id);
    }

    if (next_highlighted_segment_ids == highlighted_segment_ids_) {
        return;
    }

    const bool can_append =
        !highlight_overlay_cache_.isNull() &&
        highlight_overlay_cache_size_ == size() &&
        cached_highlighted_segment_ids_.size() <= next_highlighted_segment_ids.size();
    bool append_only = can_append;
    if (append_only) {
        for (int id : cached_highlighted_segment_ids_) {
            if (next_highlighted_segment_ids.find(id) == next_highlighted_segment_ids.end()) {
                append_only = false;
                break;
            }
        }
    }

    highlighted_segment_ids_ = std::move(next_highlighted_segment_ids);
    if (append_only) {
        QPainter cache_painter(&highlight_overlay_cache_);
        cache_painter.setRenderHint(QPainter::Antialiasing, uv_path_.size() <= 6000);
        for (int id : highlighted_segment_ids_) {
            if (cached_highlighted_segment_ids_.find(id) != cached_highlighted_segment_ids_.end()) {
                continue;
            }
            drawHighlightedSegment(cache_painter, id);
            cached_highlighted_segment_ids_.insert(id);
        }
    } else {
        invalidateHighlightCache();
    }

    update();
}

void UVView::addHighlightedPathSegment(int segment_id)
{
    if (segment_id < 0 || highlighted_segment_ids_.find(segment_id) != highlighted_segment_ids_.end()) {
        return;
    }

    highlighted_segment_ids_.insert(segment_id);
    if (!highlight_overlay_cache_.isNull() && highlight_overlay_cache_size_ == size()) {
        QPainter cache_painter(&highlight_overlay_cache_);
        cache_painter.setRenderHint(QPainter::Antialiasing, uv_path_.size() <= 6000);
        drawHighlightedSegment(cache_painter, segment_id);
        cached_highlighted_segment_ids_.insert(segment_id);
    } else {
        invalidateHighlightCache();
    }
    update();
}

void UVView::invalidateHighlightCache()
{
    highlight_overlay_cache_ = QImage();
    highlight_overlay_cache_size_ = QSize();
    cached_highlighted_segment_ids_.clear();
}

void UVView::drawHighlightedSegment(QPainter& painter, int segment_id)
{
    if (!current_job_ || !has_bounds_) {
        return;
    }

    const auto it = job_segment_indices_.find(segment_id);
    if (it == job_segment_indices_.end() || it->second >= current_job_->segments.size()) {
        return;
    }
    const nbcam::PathSegment& target_segment = current_job_->segments[it->second];
    if (target_segment.points.size() < 2) {
        return;
    }

    const auto uvPathToScreen = [this](double u, double v) -> QPointF {
        const double mirrored_v = v_min_ + v_max_ - v;
        return uvToScreen(u, mirrored_v);
    };

    const bool is_jump = (target_segment.type == nbcam::SegmentType::JUMP);
    const QColor color = is_jump ? QColor(45, 115, 255) : QColor(30, 200, 70);
    const size_t step = std::max<size_t>(1, target_segment.points.size() / 4000);

    painter.setPen(QPen(color, 2.2, is_jump ? Qt::DashLine : Qt::SolidLine));
    painter.setBrush(Qt::NoBrush);
    for (size_t i = 0; i + step < target_segment.points.size(); i += step) {
        const auto& a = target_segment.points[i];
        const auto& b = target_segment.points[i + step];
        if (!std::isfinite(a.u) || !std::isfinite(a.v) ||
            !std::isfinite(b.u) || !std::isfinite(b.v)) {
            continue;
        }
        const QPointF p1 = uvPathToScreen(a.u, a.v);
        const QPointF p2 = uvPathToScreen(b.u, b.v);
        if (!isReasonableScreenPoint(p1) || !isReasonableScreenPoint(p2)) {
            continue;
        }
        painter.drawLine(p1, p2);
    }

    painter.setPen(QPen(color, 1.2));
    painter.setBrush(QBrush(color));
    for (size_t i = 0; i < target_segment.points.size(); i += step) {
        const auto& p = target_segment.points[i];
        if (!std::isfinite(p.u) || !std::isfinite(p.v)) {
            continue;
        }
        const QPointF sp = uvPathToScreen(p.u, p.v);
        if (!isReasonableScreenPoint(sp)) {
            continue;
        }
        painter.drawEllipse(sp, 1.8, 1.8);
    }
}

void UVView::rebuildHighlightCache()
{
    if (!current_job_ || highlighted_segment_ids_.empty() || !has_bounds_ || width() <= 0 || height() <= 0) {
        invalidateHighlightCache();
        return;
    }

    highlight_overlay_cache_ = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    highlight_overlay_cache_.fill(Qt::transparent);
    highlight_overlay_cache_size_ = size();
    cached_highlighted_segment_ids_.clear();
    cached_highlighted_segment_ids_.reserve(highlighted_segment_ids_.size());

    QPainter cache_painter(&highlight_overlay_cache_);
    cache_painter.setRenderHint(QPainter::Antialiasing, uv_path_.size() <= 6000);
    for (int id : highlighted_segment_ids_) {
        drawHighlightedSegment(cache_painter, id);
        cached_highlighted_segment_ids_.insert(id);
    }
}

void UVView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && validation_mode_ && has_bounds_) {
        // 将屏幕坐标转换为UV坐标
        QPointF uv_pos = screenToUV(event->x(), event->y());
        
        // 找到最近的UV顶点
        size_t vertex_idx = findNearestVertex(uv_pos.x(), uv_pos.y());
        
        if (vertex_idx < uv_coords_.size()) {
            highlighted_vertex_index_ = vertex_idx;
            const auto& uv = uv_coords_[vertex_idx];
            highlighted_screen_pos_ = uvToScreen(uv.u, uv.v);
            
            // 发出信号
            emit uvVertexClicked(vertex_idx, uv.u, uv.v);
            
            update();  // 重绘以显示高亮和标签
        } else {
            // 没有找到附近的顶点，清除高亮
            highlighted_vertex_index_ = static_cast<size_t>(-1);
            update();
        }
    }
    
    QWidget::mousePressEvent(event);
}

void UVView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    const bool heavy_path = uv_path_.size() > 6000;
    painter.setRenderHint(QPainter::Antialiasing, !heavy_path);
    
    // 绘制背景
    painter.fillRect(rect(), QColor(255, 255, 255));
    
    if (!has_bounds_) {
        painter.setPen(QPen(Qt::gray, 1));
        painter.drawText(rect(), Qt::AlignCenter, "UV视图\n(无数据)");
        return;
    }
    
    // 绘制纹理（在网格之前绘制，作为背景）
    painter.save();
    if (!heavy_path) {
        for (const auto& pair : textures_) {
        const TextureData& tex = pair.second;
        
        if (tex.image.isNull()) {
            continue;
        }

        if (!std::isfinite(tex.u_min) || !std::isfinite(tex.u_max) ||
            !std::isfinite(tex.v_min) || !std::isfinite(tex.v_max) ||
            !std::isfinite(tex.scale_x) || !std::isfinite(tex.scale_y) ||
            !std::isfinite(tex.translate_x) || !std::isfinite(tex.translate_y) ||
            !std::isfinite(tex.rotation_deg)) {
            continue;
        }
        
        // 计算纹理在UV空间中的等比例映射框（与路径规划一致）
        const double patch_u_range = tex.u_max - tex.u_min;
        const double patch_v_range = tex.v_max - tex.v_min;
        if (!std::isfinite(patch_u_range) || !std::isfinite(patch_v_range) ||
            patch_u_range <= kMinRange || patch_v_range <= kMinRange) {
            continue;
        }
        UvAspectFitFrame fit_frame;
        if (!computeUvAspectFitFrame(tex.u_min, tex.u_max, tex.v_min, tex.v_max,
                                     tex.content_aspect_wh, fit_frame)) {
            continue;
        }
        const double center_u = (tex.u_min + tex.u_max) * 0.5;
        const double center_v = (tex.v_min + tex.v_max) * 0.5;
        
        // 应用变换：缩放、旋转、平移
        // 注意：UV视图中的变换逻辑必须与3D视图中的完全一致
        // 在3D视图中：1) 相对于中心点的坐标 2) 应用缩放 3) 应用旋转 4) 应用平移
        double rotation_rad = tex.rotation_deg * M_PI / 180.0;
        double cos_r = std::cos(rotation_rad);
        double sin_r = std::sin(rotation_rad);
        
        // 计算纹理的四个角点（在UV空间中，相对于中心点）
        // 原始角点（相对于中心点，未缩放）
        double half_u = fit_frame.u_range * 0.5;
        double half_v = fit_frame.v_range * 0.5;
        
        QPointF corners[4] = {
            QPointF(-half_u, -half_v),  // 左下
            QPointF(half_u, -half_v),   // 右下
            QPointF(half_u, half_v),    // 右上
            QPointF(-half_u, half_v)     // 左上
        };
        
        // 应用变换（与3D视图中的逻辑一致）：
        // 1. 应用缩放（相对于中心点）
        // 2. 应用旋转（围绕中心点）
        // 3. 应用平移
        for (int i = 0; i < 4; ++i) {
            // 1. 应用缩放
            double u_scaled = corners[i].x() * tex.scale_x;
            double v_scaled = corners[i].y() * tex.scale_y;
            
            // 2. 应用旋转（围绕中心点）
            double u_rot = u_scaled * cos_r - v_scaled * sin_r;
            double v_rot = u_scaled * sin_r + v_scaled * cos_r;
            
            // 3. 应用平移并移回中心点
            corners[i] = QPointF(
                u_rot + center_u + tex.translate_x,
                v_rot + center_v + tex.translate_y
            );
        }
        
        // 转换为屏幕坐标
        QPointF screen_corners[4];
        bool valid = true;
        for (int i = 0; i < 4; ++i) {
            screen_corners[i] = uvToScreen(corners[i].x(), corners[i].y());
            if (!isReasonableScreenPoint(screen_corners[i])) {
                valid = false;
                break;
            }
        }
        
        if (!valid) {
            continue;
        }
        
        // 使用QPainterPath和setClipPath来绘制变换后的纹理
        QPolygonF polygon;
        for (int i = 0; i < 4; ++i) {
            polygon << screen_corners[i];
        }
        
        // 计算边界框
        QRectF bounds = polygon.boundingRect();
        if (!bounds.isValid() ||
            !std::isfinite(bounds.x()) || !std::isfinite(bounds.y()) ||
            !std::isfinite(bounds.width()) || !std::isfinite(bounds.height()) ||
            bounds.width() < 1.0 || bounds.height() < 1.0) {
            continue;
        }
        
        QPainterPath path;
        path.addPolygon(polygon);
        if (path.isEmpty()) {
            continue;
        }

        // 对于UV预览，使用稳定的矩形缩放+多边形裁剪，避免复杂透视变换触发Qt绘制崩溃。
        int draw_w = static_cast<int>(std::clamp(bounds.width(), 1.0, static_cast<double>(kMaxFallbackTextureSize)));
        int draw_h = static_cast<int>(std::clamp(bounds.height(), 1.0, static_cast<double>(kMaxFallbackTextureSize)));
        int draw_x = static_cast<int>(std::floor(bounds.x()));
        int draw_y = static_cast<int>(std::floor(bounds.y()));
        QRect target_rect(draw_x, draw_y, draw_w, draw_h);
        if (!target_rect.isValid() || tex.image.width() <= 0 || tex.image.height() <= 0) {
            continue;
        }

        painter.save();
        painter.drawImage(target_rect, tex.image);
        painter.setPen(QPen(QColor(80, 80, 80, 180), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(polygon);
        painter.restore();
        }
    }
    painter.restore();
    
    // 绘制网格
    painter.setPen(QPen(QColor(220, 220, 220), 1));
    int grid_count = 10;
    for (int i = 0; i <= grid_count; ++i) {
        double u = u_min_ + (u_max_ - u_min_) * i / grid_count;
        double v = v_min_ + (v_max_ - v_min_) * i / grid_count;
        
        QPointF p1 = uvToScreen(u, v_min_);
        QPointF p2 = uvToScreen(u, v_max_);
        painter.drawLine(p1, p2);
        
        QPointF p3 = uvToScreen(u_min_, v);
        QPointF p4 = uvToScreen(u_max_, v);
        painter.drawLine(p3, p4);
    }
    
    // 绘制UV坐标点（网格顶点）
    if (!uv_coords_.empty()) {
        if (wireframe_mode_) {
            // 线框模式：绘制三角形的边
            painter.setPen(QPen(QColor(100, 150, 255), 1));
            painter.setBrush(Qt::NoBrush);
            
            // 绘制三角形的边
            if (!triangles_.empty()) {
                for (const auto& tri : triangles_) {
                    // 检查索引有效性
                    if (tri.v0 >= uv_coords_.size() || 
                        tri.v1 >= uv_coords_.size() || 
                        tri.v2 >= uv_coords_.size()) {
                        continue;
                    }
                    
                    const auto& v0 = uv_coords_[tri.v0];
                    const auto& v1 = uv_coords_[tri.v1];
                    const auto& v2 = uv_coords_[tri.v2];
                    
                    // 检查UV坐标是否有效
                    if (!std::isfinite(v0.u) || !std::isfinite(v0.v) ||
                        !std::isfinite(v1.u) || !std::isfinite(v1.v) ||
                        !std::isfinite(v2.u) || !std::isfinite(v2.v)) {
                        continue;
                    }
                    
                    QPointF p0 = uvToScreen(v0.u, v0.v);
                    QPointF p1 = uvToScreen(v1.u, v1.v);
                    QPointF p2 = uvToScreen(v2.u, v2.v);
                    
                    // 检查屏幕坐标是否有效
                    if (!isReasonableScreenPoint(p0) ||
                        !isReasonableScreenPoint(p1) ||
                        !isReasonableScreenPoint(p2)) {
                        continue;
                    }
                    
                    // 绘制三角形的三条边
                    painter.drawLine(p0, p1);
                    painter.drawLine(p1, p2);
                    painter.drawLine(p2, p0);
                }
            } else {
                // 如果没有三角形信息，只绘制顶点
                for (const auto& coord : uv_coords_) {
                    QPointF p = uvToScreen(coord.u, coord.v);
                    if (isReasonableScreenPoint(p)) {
                        painter.drawEllipse(p, 1, 1);
                    }
                }
            }
        } else {
            // 正常模式：绘制填充的点
            painter.setPen(QPen(QColor(100, 150, 255), 2));
            painter.setBrush(QBrush(QColor(100, 150, 255)));
            
            for (const auto& coord : uv_coords_) {
                QPointF p = uvToScreen(coord.u, coord.v);
                if (isReasonableScreenPoint(p)) {
                    painter.drawEllipse(p, 2, 2);
                }
            }
        }
    }
    
    // 绘制高亮的UV顶点（校验模式）
    if (validation_mode_ && highlighted_vertex_index_ < uv_coords_.size()) {
        const auto& uv = uv_coords_[highlighted_vertex_index_];
        QPointF screen_pos = uvToScreen(uv.u, uv.v);
        if (!isReasonableScreenPoint(screen_pos)) {
            return;
        }
        
        // 绘制高亮点（较大的红色圆点）
        painter.setPen(QPen(QColor(255, 0, 0), 3));
        painter.setBrush(QBrush(QColor(255, 0, 0)));
        painter.drawEllipse(screen_pos, 6, 6);
        
        // 绘制标签
        QString label = QString("顶点 %1\nUV: (%.6f, %.6f)")
            .arg(highlighted_vertex_index_)
            .arg(uv.u, 0, 'f', 6)
            .arg(uv.v, 0, 'f', 6);
        
        QFont font = painter.font();
        font.setPointSize(9);
        painter.setFont(font);
        
        QFontMetrics fm(font);
        QRect label_rect = fm.boundingRect(label);
        label_rect.moveTopLeft(QPoint(
            static_cast<int>(screen_pos.x() + 10),
            static_cast<int>(screen_pos.y() - label_rect.height() / 2)
        ));
        
        // 绘制标签背景
        painter.setPen(QPen(QColor(0, 0, 0), 1));
        painter.setBrush(QBrush(QColor(255, 255, 255, 230)));
        painter.drawRect(label_rect.adjusted(-4, -2, 4, 2));
        
        // 绘制标签文字
        painter.setPen(QPen(QColor(0, 0, 0), 1));
        painter.drawText(label_rect, Qt::AlignLeft | Qt::AlignVCenter, label);
    }
    
    // 绘制UV路径
    if (!uv_path_.empty()) {
        // 仅在UV视图显示层做垂直镜像，保证显示方向与SVG一致，不影响规划数据本身。
        const auto uvPathToScreen = [this](double u, double v) -> QPointF {
            const double mirrored_v = v_min_ + v_max_ - v;
            return uvToScreen(u, mirrored_v);
        };

        const size_t display_step = heavy_path
            ? std::max<size_t>(2, uv_path_.size() / 3000)
            : std::max<size_t>(1, uv_path_.size() / kMaxDisplayPathPoints);
        size_t seg_counter = 0;
        for (size_t i = 0; i + display_step < uv_path_.size(); i += display_step) {
            if (!std::isfinite(uv_path_[i].u) || !std::isfinite(uv_path_[i].v) ||
                !std::isfinite(uv_path_[i + display_step].u) || !std::isfinite(uv_path_[i + display_step].v)) {
                continue;
            }
            
            QPointF p1 = uvPathToScreen(uv_path_[i].u, uv_path_[i].v);
            QPointF p2 = uvPathToScreen(uv_path_[i + display_step].u, uv_path_[i + display_step].v);
            
            if (!isReasonableScreenPoint(p1) ||
                !isReasonableScreenPoint(p2)) {
                continue;
            }

            const bool is_jump = uv_path_[i + display_step].is_jump_before;
            QPen segment_pen = is_jump
                ? QPen(QColor(120, 120, 120), 1.2, Qt::DashLine)
                : QPen(QColor(220, 20, 20), 1.4, Qt::SolidLine);
            painter.setPen(segment_pen);
            painter.drawLine(p1, p2);

            // 密集路径只做线条预览，箭头稀疏绘制，避免Qt6Gui在复杂矢量绘制时崩溃。
            const bool draw_arrow = (!heavy_path) || ((seg_counter % 24) == 0);
            if (draw_arrow) {
                QPointF dir = p2 - p1;
                double len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
                if (len > 1e-6) {
                    double sx = 8.0 * dir.x() / len;
                    double sy = 8.0 * dir.y() / len;
                    double nx = -dir.y() / len, ny = dir.x() / len;
                    QPointF arrow1(p2.x() - sx + nx * 4, p2.y() - sy + ny * 4);
                    QPointF arrow2(p2.x() - sx - nx * 4, p2.y() - sy - ny * 4);
                    if (isReasonableScreenPoint(arrow1) && isReasonableScreenPoint(arrow2)) {
                        painter.drawLine(p2, arrow1);
                        painter.drawLine(p2, arrow2);
                    }
                }
            }
            ++seg_counter;
        }

        if (!heavy_path) {
            // 绘制路径点（轻量）
            painter.setPen(QPen(QColor(255, 0, 0), 1));
            painter.setBrush(QBrush(QColor(255, 0, 0)));
            for (size_t i = 0; i < uv_path_.size(); i += display_step) {
                const auto& point = uv_path_[i];
                if (!std::isfinite(point.u) || !std::isfinite(point.v)) continue;
                QPointF p = uvPathToScreen(point.u, point.v);
                if (!isReasonableScreenPoint(p)) continue;
                painter.drawEllipse(p, 1.5, 1.5);
            }
        }
    }

    // 选中的路径段高亮叠加层：开光=绿色，关光=蓝色。
    if (current_job_ && !highlighted_segment_ids_.empty()) {
        if (highlight_overlay_cache_.isNull() || highlight_overlay_cache_size_ != size()) {
            rebuildHighlightCache();
        }
        if (!highlight_overlay_cache_.isNull()) {
            painter.drawImage(0, 0, highlight_overlay_cache_);
        }
    }
    
    // 绘制坐标轴标签
    painter.setPen(QPen(Qt::black, 1));
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    
    painter.drawText(20, height() - 10, QString("U: [%1, %2]").arg(u_min_, 0, 'f', 2).arg(u_max_, 0, 'f', 2));
    painter.drawText(width() - 100, height() - 10, QString("V: [%1, %2]").arg(v_min_, 0, 'f', 2).arg(v_max_, 0, 'f', 2));
}

void UVView::setWireframeMode(bool enabled)
{
    wireframe_mode_ = enabled;
    update();
}

void UVView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!uv_coords_.empty()) {
        updateTransform();
    }
    invalidateHighlightCache();
}

void UVView::wheelEvent(QWheelEvent *event)
{
    if (!has_bounds_) {
        event->ignore();
        return;
    }
    
    // 获取滚轮增量（角度）
    int delta = event->angleDelta().y();
    
    // 缩放因子（滚轮向上放大，向下缩小）
    double zoom_factor = 1.0;
    if (delta > 0) {
        zoom_factor = 1.1;  // 放大10%
    } else if (delta < 0) {
        zoom_factor = 1.0 / 1.1;  // 缩小10%
    } else {
        event->ignore();
        return;
    }
    
    // 获取鼠标位置（UV坐标）
    QPointF mouse_pos = event->position();
    
    // 计算鼠标位置对应的UV坐标
    double u_center = u_min_ + (mouse_pos.x() - offset_x_) / scale_x_;
    double v_center = v_max_ - (mouse_pos.y() - offset_y_) / scale_y_;
    
    // 更新缩放
    scale_x_ *= zoom_factor;
    scale_y_ *= zoom_factor;
    
    // 调整偏移，使鼠标位置保持固定
    int w = width();
    int h = height();
    offset_x_ = mouse_pos.x() - (u_center - u_min_) * scale_x_;
    offset_y_ = mouse_pos.y() - (v_max_ - v_center) * scale_y_;
    
    // 限制缩放范围（避免过度缩放）
    double min_scale = 0.01;
    double max_scale = 1000.0;
    if (scale_x_ < min_scale) {
        scale_x_ = min_scale;
        scale_y_ = min_scale;
        updateTransform();  // 重新计算变换
    } else if (scale_x_ > max_scale) {
        scale_x_ = max_scale;
        scale_y_ = max_scale;
        updateTransform();  // 重新计算变换
    }
    
    invalidateHighlightCache();
    update();
    event->accept();
}
