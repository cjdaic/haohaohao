#include "application_controller.h"

#include "../core/mesh/mesh_preprocessor.h"
#include "../core/param/patch_parameterizer.h"
#include "../core/pattern/contour_strategy.h"
#include "../core/pattern/hatch_strategy.h"
#include "../core/pattern/line_hatch_strategy.h"
#include "../core/pattern/nanosvg_wrapper.h"
#include "../core/pattern/ring_fill_strategy.h"
#include "../core/pattern/svg_to_uv_mapper.h"
#include "../core/process/curvature_model.h"
#include "../core/executor/data_generator.h"

#include <QDateTime>
#include <spdlog/spdlog.h>
#include <clipper2/clipper.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr double kDefaultMachineMinMm = -32.768;
constexpr double kDefaultMachineMaxMm = 32.767;

bool isFiniteUV(const nbcam::UVCoord& uv)
{
    return std::isfinite(uv.u) && std::isfinite(uv.v);
}

bool computeMeshBounds(const nbcam::TriangleMesh& mesh, std::array<double, 6>& bounds)
{
    if (mesh.vertices.empty()) {
        return false;
    }

    double min_x = std::numeric_limits<double>::infinity();
    double max_x = -std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();
    double min_z = std::numeric_limits<double>::infinity();
    double max_z = -std::numeric_limits<double>::infinity();

    bool found = false;
    for (const auto& vertex : mesh.vertices) {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) {
            continue;
        }
        min_x = std::min(min_x, vertex.x);
        max_x = std::max(max_x, vertex.x);
        min_y = std::min(min_y, vertex.y);
        max_y = std::max(max_y, vertex.y);
        min_z = std::min(min_z, vertex.z);
        max_z = std::max(max_z, vertex.z);
        found = true;
    }

    if (!found) {
        return false;
    }

    bounds = {min_x, max_x, min_y, max_y, min_z, max_z};
    return true;
}

std::string describeBounds(const std::array<double, 6>& bounds)
{
    return fmt::format("X=[{:.3f},{:.3f}] Y=[{:.3f},{:.3f}] Z=[{:.3f},{:.3f}]",
                       bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]);
}

bool containsIgnoreCase(std::string text, std::string needle)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text.find(needle) != std::string::npos;
}

Clipper2Lib::FillRule resolveSvgFillRule(const std::string& svg_filepath)
{
    if (svg_filepath.empty()) {
        return Clipper2Lib::FillRule::NonZero;
    }

    std::ifstream in(svg_filepath, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        return Clipper2Lib::FillRule::NonZero;
    }

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // SVG默认是 nonzero；仅当明确声明 evenodd 时才切换。
    if (containsIgnoreCase(content, "fill-rule=\"evenodd\"") ||
        containsIgnoreCase(content, "fill-rule:evenodd") ||
        containsIgnoreCase(content, "clip-rule=\"evenodd\"") ||
        containsIgnoreCase(content, "clip-rule:evenodd")) {
        return Clipper2Lib::FillRule::EvenOdd;
    }
    return Clipper2Lib::FillRule::NonZero;
}

std::string normalizeStrategyName(const std::string& strategy)
{
    if (strategy == "line_hatch" ||
        strategy == "line" ||
        strategy == "line_hatch_x" ||
        strategy == "轴向往返填充" ||
        strategy == "直线往返" ||
        strategy == "栅格填充") {
        return "line_hatch";
    }
    if (strategy == "arc_hatch" ||
        strategy == "轴向往返填充(圆弧)" ||
        strategy == "圆弧往返") {
        return "arc_hatch";
    }
    if (strategy == "contour" ||
        strategy == "轮廓填充" ||
        strategy == "轮廓偏置回型(忽略空洞)") {
        return "contour_offset";
    }
    if (strategy == "ring" ||
        strategy == "回型填充" ||
        strategy == "回型填充(内->外)") {
        return "ring_inout";
    }
    if (strategy == "回型填充(外->内)") {
        return "ring_outin";
    }
    if (strategy == "z_layer" ||
        strategy == "Z轴分层填充") {
        return "z_layer";
    }
    return "line_hatch";
}

std::unique_ptr<nbcam::IFillStrategy> createFillStrategy(const std::string& strategy)
{
    const std::string normalized = normalizeStrategyName(strategy);
    if (normalized == "line_hatch" || normalized == "arc_hatch") {
        return std::make_unique<nbcam::LineHatchStrategy>();
    }
    if (normalized == "contour_offset") {
        return std::make_unique<nbcam::ContourStrategy>();
    }
    if (normalized == "ring_inout" || normalized == "ring_outin") {
        return std::make_unique<nbcam::RingFillStrategy>();
    }
    if (normalized == "z_layer") {
        return std::make_unique<nbcam::LineHatchStrategy>();
    }
    if (normalized == "hatch") {
        return std::make_unique<nbcam::LineHatchStrategy>();
    }
    if (normalized == "contour") {
        return std::make_unique<nbcam::ContourStrategy>();
    }
    if (normalized == "ring") {
        return std::make_unique<nbcam::RingFillStrategy>();
    }
    return std::make_unique<nbcam::LineHatchStrategy>();
}

nbcam::FillStrategy toFillStrategyEnum(const std::string& strategy)
{
    const std::string normalized = normalizeStrategyName(strategy);
    if (normalized == "contour" || normalized == "contour_offset") {
        return nbcam::FillStrategy::CONTOUR;
    }
    if (normalized == "ring" || normalized == "ring_inout" || normalized == "ring_outin") {
        return nbcam::FillStrategy::RING;
    }
    if (normalized == "arc_hatch") {
        return nbcam::FillStrategy::ARC_HATCH;
    }
    return nbcam::FillStrategy::HATCH;
}

double resolveAngleFromDirection(double angle_deg, const std::string& direction_mode)
{
    if (direction_mode == "u_axis" || direction_mode == "锁定U轴") {
        return 0.0;
    }
    if (direction_mode == "v_axis" || direction_mode == "锁定V轴") {
        return 90.0;
    }
    return angle_deg;
}

int grayscaleBucketForPlan(double gray01)
{
    const int gray256 = static_cast<int>(std::floor(std::clamp(gray01, 0.0, 1.0) * 256.0));
    return std::clamp(gray256 / 10, 0, 25);
}

nbcam::PathPoint cloneJobPoint(const nbcam::PathPoint& src)
{
    nbcam::PathPoint dst;
    dst.u = src.u;
    dst.v = src.v;
    dst.x = src.x;
    dst.y = src.y;
    dst.z = src.z;
    dst.a = src.a;
    dst.b = src.b;
    dst.laser = src.laser;
    dst.grayscale = src.grayscale;
    if (src.params_override) {
        dst.params_override = std::make_unique<nbcam::ProcessParams>(*src.params_override);
    }
    return dst;
}

void resampleJobSegmentsToConvexArc(nbcam::LaserJob& job,
                                    double center_x,
                                    double center_z,
                                    double radius_mm,
                                    double forced_speed_mm_s)
{
    if (!(radius_mm > 1e-9)) {
        return;
    }

    constexpr double kTickSec = 0.00001;  // 10us
    constexpr int kMaxSamplesPerRow = 50000;
    const int laser_on_delay_us = std::max(0, nbcam::DataGenerator::LASER_ON_DELAY);

    for (auto& segment : job.segments) {
        if (segment.type != nbcam::SegmentType::MARK || segment.points.size() < 2) {
            continue;
        }

        const auto& first = segment.points.front();
        const auto& last = segment.points.back();
        if (!std::isfinite(first.x) || !std::isfinite(first.y) || !std::isfinite(first.z) ||
            !std::isfinite(last.x) || !std::isfinite(last.y) || !std::isfinite(last.z)) {
            continue;
        }

        const double z_row = first.z;
        double theta_1 = std::atan2(first.x - center_x, first.y - center_z);
        double theta_2 = std::atan2(last.x - center_x, last.y - center_z);
        double delta = theta_2 - theta_1;
        if (delta > M_PI) {
            theta_2 -= 2.0 * M_PI;
            delta = theta_2 - theta_1;
        } else if (delta < -M_PI) {
            theta_2 += 2.0 * M_PI;
            delta = theta_2 - theta_1;
        }

        if (std::abs(delta) < 1e-9) {
            continue;
        }

        double speed_mm_s = forced_speed_mm_s;
        if (!(std::isfinite(speed_mm_s) && speed_mm_s > 1e-6)) {
            speed_mm_s = job.process_defaults.speed_mm_s;
            if (segment.params_override && std::isfinite(segment.params_override->speed_mm_s) &&
                segment.params_override->speed_mm_s > 1e-6) {
                speed_mm_s = segment.params_override->speed_mm_s;
            } else {
                for (const auto& p : segment.points) {
                    if (p.params_override && std::isfinite(p.params_override->speed_mm_s) &&
                        p.params_override->speed_mm_s > 1e-6) {
                        speed_mm_s = p.params_override->speed_mm_s;
                        break;
                    }
                }
            }
        }
        speed_mm_s = std::max(1e-6, speed_mm_s);

        const double arc_length_mm = std::abs(delta) * radius_mm;
        int n_max = static_cast<int>(arc_length_mm / (speed_mm_s * kTickSec)) + 1;
        n_max = std::max(2, std::min(n_max, kMaxSamplesPerRow));

        std::vector<nbcam::PathPoint> arc_points;
        arc_points.reserve(static_cast<size_t>(n_max));
        const bool keep_override = segment.points.front().params_override != nullptr;
        for (int i = 1; i <= n_max; ++i) {
            const double ratio = static_cast<double>(i) / static_cast<double>(n_max);
            const double theta = theta_1 + delta * ratio;

            nbcam::PathPoint point;
            point.u = ratio;
            point.v = z_row;
            point.x = center_x + radius_mm * std::sin(theta);
            point.y = center_z + radius_mm * std::cos(theta);
            point.z = z_row;
            point.laser = ((i * 10) < laser_on_delay_us) ? 0 : 1;
            if (keep_override) {
                point.params_override = std::make_unique<nbcam::ProcessParams>(*segment.points.front().params_override);
            }
            arc_points.push_back(std::move(point));
        }

        if (arc_points.size() >= 2) {
            segment.points = std::move(arc_points);
            segment.strategy = nbcam::FillStrategy::ARC_HATCH;
        }
    }
}

double cross2D(const nbcam::UVCoord& o, const nbcam::UVCoord& a, const nbcam::UVCoord& b)
{
    return (a.u - o.u) * (b.v - o.v) - (a.v - o.v) * (b.u - o.u);
}

std::vector<nbcam::UVCoord> buildConvexHullBoundary(const std::vector<nbcam::UVCoord>& points)
{
    std::vector<nbcam::UVCoord> clean;
    clean.reserve(points.size());

    for (const auto& uv : points) {
        if (isFiniteUV(uv)) {
            clean.push_back(uv);
        }
    }
    if (clean.size() < 3) {
        return {};
    }

    std::sort(clean.begin(), clean.end(), [](const nbcam::UVCoord& a, const nbcam::UVCoord& b) {
        if (a.u == b.u) {
            return a.v < b.v;
        }
        return a.u < b.u;
    });

    clean.erase(std::unique(clean.begin(), clean.end(), [](const nbcam::UVCoord& a, const nbcam::UVCoord& b) {
                   return a.u == b.u && a.v == b.v;
               }),
               clean.end());

    if (clean.size() < 3) {
        return {};
    }

    std::vector<nbcam::UVCoord> lower;
    for (const auto& p : clean) {
        while (lower.size() >= 2 && cross2D(lower[lower.size() - 2], lower.back(), p) <= 0.0) {
            lower.pop_back();
        }
        lower.push_back(p);
    }

    std::vector<nbcam::UVCoord> upper;
    for (size_t i = clean.size(); i-- > 0;) {
        const auto& p = clean[i];
        while (upper.size() >= 2 && cross2D(upper[upper.size() - 2], upper.back(), p) <= 0.0) {
            upper.pop_back();
        }
        upper.push_back(p);
    }

    lower.pop_back();
    upper.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

std::uint64_t edgeKey(size_t a, size_t b)
{
    const auto lo = static_cast<std::uint64_t>(std::min(a, b));
    const auto hi = static_cast<std::uint64_t>(std::max(a, b));
    return (lo << 32) | hi;
}

double polygonSignedArea(const std::vector<nbcam::UVCoord>& polygon)
{
    if (polygon.size() < 3) {
        return 0.0;
    }

    double area2 = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const auto& p = polygon[i];
        const auto& q = polygon[(i + 1) % polygon.size()];
        area2 += p.u * q.v - q.u * p.v;
    }
    return 0.5 * area2;
}

std::vector<nbcam::UVCoord> fallbackBoundaryFromVertices(const nbcam::Patch& patch,
                                                         const std::vector<nbcam::UVCoord>& uv_coords)
{
    std::unordered_set<size_t> unique_vertices;
    for (size_t i = 0; i + 1 < patch.boundary_edges.size(); i += 2) {
        unique_vertices.insert(patch.boundary_edges[i]);
        unique_vertices.insert(patch.boundary_edges[i + 1]);
    }

    std::vector<nbcam::UVCoord> points;
    points.reserve(unique_vertices.size());
    for (size_t idx : unique_vertices) {
        if (idx < uv_coords.size() && isFiniteUV(uv_coords[idx])) {
            points.push_back(uv_coords[idx]);
        }
    }

    if (points.size() < 3) {
        return {};
    }

    nbcam::UVCoord center{0.0, 0.0};
    for (const auto& uv : points) {
        center.u += uv.u;
        center.v += uv.v;
    }
    center.u /= static_cast<double>(points.size());
    center.v /= static_cast<double>(points.size());

    std::sort(points.begin(), points.end(), [&](const nbcam::UVCoord& a, const nbcam::UVCoord& b) {
        return std::atan2(a.v - center.v, a.u - center.u) < std::atan2(b.v - center.v, b.u - center.u);
    });

    return points;
}

std::vector<nbcam::UVCoord> buildPatchBoundary(const nbcam::Patch& patch,
                                               const std::vector<nbcam::UVCoord>& uv_coords)
{
    if (patch.boundary_edges.size() < 6) {
        return fallbackBoundaryFromVertices(patch, uv_coords);
    }

    std::unordered_map<size_t, std::vector<size_t>> adjacency;
    std::unordered_set<std::uint64_t> all_edges;
    for (size_t i = 0; i + 1 < patch.boundary_edges.size(); i += 2) {
        const size_t a = patch.boundary_edges[i];
        const size_t b = patch.boundary_edges[i + 1];
        if (a == b || a >= uv_coords.size() || b >= uv_coords.size()) {
            continue;
        }
        if (!isFiniteUV(uv_coords[a]) || !isFiniteUV(uv_coords[b])) {
            continue;
        }
        adjacency[a].push_back(b);
        adjacency[b].push_back(a);
        all_edges.insert(edgeKey(a, b));
    }

    if (all_edges.empty()) {
        return fallbackBoundaryFromVertices(patch, uv_coords);
    }

    std::unordered_set<std::uint64_t> visited_edges;
    std::vector<std::vector<size_t>> loops;

    auto pickNext = [&](size_t curr, size_t prev) -> size_t {
        const auto it = adjacency.find(curr);
        if (it == adjacency.end()) {
            return std::numeric_limits<size_t>::max();
        }
        for (size_t candidate : it->second) {
            if (candidate == prev && it->second.size() > 1) {
                continue;
            }
            const auto key = edgeKey(curr, candidate);
            if (all_edges.count(key) && !visited_edges.count(key)) {
                return candidate;
            }
        }
        for (size_t candidate : it->second) {
            const auto key = edgeKey(curr, candidate);
            if (all_edges.count(key) && !visited_edges.count(key)) {
                return candidate;
            }
        }
        return std::numeric_limits<size_t>::max();
    };

    for (const auto& [start, neighbors] : adjacency) {
        for (size_t next : neighbors) {
            const auto first_edge = edgeKey(start, next);
            if (!all_edges.count(first_edge) || visited_edges.count(first_edge)) {
                continue;
            }

            std::vector<size_t> loop;
            loop.push_back(start);
            size_t prev = start;
            size_t curr = next;
            visited_edges.insert(first_edge);

            const size_t max_steps = all_edges.size() + 4;
            for (size_t step = 0; step < max_steps; ++step) {
                loop.push_back(curr);
                if (curr == start) {
                    break;
                }
                const size_t candidate = pickNext(curr, prev);
                if (candidate == std::numeric_limits<size_t>::max()) {
                    break;
                }
                visited_edges.insert(edgeKey(curr, candidate));
                prev = curr;
                curr = candidate;
            }

            if (loop.size() >= 4 && loop.front() == loop.back()) {
                loop.pop_back();
                loops.push_back(std::move(loop));
            }
        }
    }

    if (loops.empty()) {
        return fallbackBoundaryFromVertices(patch, uv_coords);
    }

    std::vector<nbcam::UVCoord> best_loop;
    double best_area = 0.0;
    for (const auto& loop_indices : loops) {
        std::vector<nbcam::UVCoord> loop;
        loop.reserve(loop_indices.size());
        for (size_t idx : loop_indices) {
            if (idx < uv_coords.size() && isFiniteUV(uv_coords[idx])) {
                loop.push_back(uv_coords[idx]);
            }
        }
        if (loop.size() < 3) {
            continue;
        }
        const double area = std::abs(polygonSignedArea(loop));
        if (area > best_area) {
            best_area = area;
            best_loop = std::move(loop);
        }
    }

    if (best_loop.size() < 3) {
        return fallbackBoundaryFromVertices(patch, uv_coords);
    }
    if (polygonSignedArea(best_loop) < 0.0) {
        std::reverse(best_loop.begin(), best_loop.end());
    }
    return best_loop;
}

double pointToSegmentDistance(const nbcam::UVCoord& p, const nbcam::UVCoord& a, const nbcam::UVCoord& b)
{
    const double dx = b.u - a.u;
    const double dy = b.v - a.v;
    const double len_sq = dx * dx + dy * dy;
    if (len_sq <= 1e-12) {
        const double ox = p.u - a.u;
        const double oy = p.v - a.v;
        return std::sqrt(ox * ox + oy * oy);
    }
    const double t = std::max(0.0, std::min(1.0, ((p.u - a.u) * dx + (p.v - a.v) * dy) / len_sq));
    const double proj_u = a.u + t * dx;
    const double proj_v = a.v + t * dy;
    const double ox = p.u - proj_u;
    const double oy = p.v - proj_v;
    return std::sqrt(ox * ox + oy * oy);
}

bool isPointInsidePatchDomain(const nbcam::UVCoord& p, const std::vector<nbcam::UVCoord>& polygon)
{
    if (polygon.size() < 3) {
        return false;
    }

    constexpr double kEdgeEps = 0.05;  // UV单位，容忍边界上的数值误差
    for (size_t i = 0; i < polygon.size(); ++i) {
        const auto& a = polygon[i];
        const auto& b = polygon[(i + 1) % polygon.size()];
        if (pointToSegmentDistance(p, a, b) <= kEdgeEps) {
            return true;
        }
    }

    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const auto& pi = polygon[i];
        const auto& pj = polygon[j];
        const bool intersect = ((pi.v > p.v) != (pj.v > p.v)) &&
                               (p.u < (pj.u - pi.u) * (p.v - pi.v) / ((pj.v - pi.v) + 1e-12) + pi.u);
        if (intersect) {
            inside = !inside;
        }
    }
    return inside;
}

std::vector<nbcam::UVCoord> collectPatchUVCloud(const nbcam::Patch& patch,
                                                const nbcam::TriangleMesh& mesh,
                                                const std::vector<nbcam::UVCoord>& uv_coords)
{
    std::vector<nbcam::UVCoord> patch_uv_cloud;
    patch_uv_cloud.reserve(patch.triangle_indices.size() * 3);
    for (size_t tri_idx : patch.triangle_indices) {
        if (tri_idx >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[tri_idx];
        for (size_t v_idx : {tri.v0, tri.v1, tri.v2}) {
            if (v_idx < uv_coords.size() && isFiniteUV(uv_coords[v_idx])) {
                patch_uv_cloud.push_back(uv_coords[v_idx]);
            }
        }
    }
    return patch_uv_cloud;
}

void alignPatchUvAxesToModelXY(const nbcam::Patch& patch,
                               const nbcam::TriangleMesh& mesh,
                               std::vector<nbcam::UVCoord>& uv_coords)
{
    const double abs_nx = std::abs(patch.normal_x);
    const double abs_ny = std::abs(patch.normal_y);
    const double abs_nz = std::abs(patch.normal_z);

    int normal_axis = 0;  // 0:X, 1:Y, 2:Z
    double normal_axis_abs = abs_nx;
    if (abs_ny > normal_axis_abs) {
        normal_axis = 1;
        normal_axis_abs = abs_ny;
    }
    if (abs_nz > normal_axis_abs) {
        normal_axis = 2;
        normal_axis_abs = abs_nz;
    }

    // 仅在法向有明显主轴时做自动对齐，避免弱相关时引入随机倾斜。
    if (normal_axis_abs < 0.5) {
        return;
    }

    int axis0 = 0;
    int axis1 = 1;
    if (normal_axis == 0) {
        axis0 = 1;  // Y
        axis1 = 2;  // Z
    } else if (normal_axis == 1) {
        axis0 = 0;  // X
        axis1 = 2;  // Z
    } else {
        axis0 = 0;  // X
        axis1 = 1;  // Y
    }

    const auto getAxisCoord = [](const auto& vtx, int axis) {
        if (axis == 0) {
            return vtx.x;
        }
        if (axis == 1) {
            return vtx.y;
        }
        return vtx.z;
    };

    std::unordered_set<size_t> unique_vertices;
    unique_vertices.reserve(patch.triangle_indices.size() * 3);
    for (size_t tri_idx : patch.triangle_indices) {
        if (tri_idx >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[tri_idx];
        unique_vertices.insert(tri.v0);
        unique_vertices.insert(tri.v1);
        unique_vertices.insert(tri.v2);
    }

    std::vector<size_t> valid_indices;
    valid_indices.reserve(unique_vertices.size());
    double u_mean = 0.0;
    double v_mean = 0.0;
    double axis0_mean = 0.0;
    double axis1_mean = 0.0;
    for (size_t idx : unique_vertices) {
        if (idx >= uv_coords.size() || idx >= mesh.vertices.size()) {
            continue;
        }
        const auto& uv = uv_coords[idx];
        const auto& vtx = mesh.vertices[idx];
        const double c0 = getAxisCoord(vtx, axis0);
        const double c1 = getAxisCoord(vtx, axis1);
        if (!isFiniteUV(uv) || !std::isfinite(c0) || !std::isfinite(c1)) {
            continue;
        }
        valid_indices.push_back(idx);
        u_mean += uv.u;
        v_mean += uv.v;
        axis0_mean += c0;
        axis1_mean += c1;
    }

    if (valid_indices.size() < 3) {
        return;
    }

    const double inv_n = 1.0 / static_cast<double>(valid_indices.size());
    u_mean *= inv_n;
    v_mean *= inv_n;
    axis0_mean *= inv_n;
    axis1_mean *= inv_n;

    double cov_u_axis0 = 0.0;
    double cov_v_axis0 = 0.0;
    double cov_u_axis1 = 0.0;
    double cov_v_axis1 = 0.0;
    for (size_t idx : valid_indices) {
        const auto& uv = uv_coords[idx];
        const auto& vtx = mesh.vertices[idx];
        const double c0 = getAxisCoord(vtx, axis0);
        const double c1 = getAxisCoord(vtx, axis1);
        const double du = uv.u - u_mean;
        const double dv = uv.v - v_mean;
        const double d0 = c0 - axis0_mean;
        const double d1 = c1 - axis1_mean;
        cov_u_axis0 += du * d0;
        cov_v_axis0 += dv * d0;
        cov_u_axis1 += du * d1;
        cov_v_axis1 += dv * d1;
    }

    const double cov_axis0_norm = std::hypot(cov_u_axis0, cov_v_axis0);
    const double cov_axis1_norm = std::hypot(cov_u_axis1, cov_v_axis1);
    if (cov_axis0_norm <= 1e-12 || cov_axis1_norm <= 1e-12) {
        return;
    }

    // 旋转UV，使切平面第1坐标方向与U轴对齐，再按第2坐标方向决定V翻转。
    const double theta = std::atan2(cov_v_axis0, cov_u_axis0);
    const double c = std::cos(theta);
    const double s = std::sin(theta);
    const double cov_axis1_v_after_rotate = -cov_u_axis1 * s + cov_v_axis1 * c;
    const bool flip_v = (cov_axis1_v_after_rotate < 0.0);

    for (size_t idx : valid_indices) {
        auto& uv = uv_coords[idx];
        const double du = uv.u - u_mean;
        const double dv = uv.v - v_mean;
        const double ru = du * c + dv * s;
        double rv = -du * s + dv * c;
        if (flip_v) {
            rv = -rv;
        }
        uv.u = ru + u_mean;
        uv.v = rv + v_mean;
    }
}

std::vector<nbcam::UVPathPoint> clipUVPathToPatchDomain(const std::vector<nbcam::UVPathPoint>& path,
                                                        const std::vector<nbcam::UVCoord>& patch_domain)
{
    if (path.empty() || patch_domain.size() < 3) {
        return path;
    }

    std::vector<nbcam::UVPathPoint> clipped;
    clipped.reserve(path.size());
    bool pending_break = false;

    for (const auto& src_pt : path) {
        if (!std::isfinite(src_pt.u) || !std::isfinite(src_pt.v)) {
            pending_break = true;
            continue;
        }
        const nbcam::UVCoord uv{src_pt.u, src_pt.v};
        if (!isPointInsidePatchDomain(uv, patch_domain)) {
            pending_break = true;
            continue;
        }

        nbcam::UVPathPoint dst = src_pt;
        dst.is_jump_before = src_pt.is_jump_before || pending_break || clipped.empty();
        clipped.push_back(dst);
        pending_break = false;
    }

    return clipped;
}

std::vector<nbcam::UVPathPoint> densifyUVPathSegments(const std::vector<nbcam::UVPathPoint>& path, double max_step)
{
    if (path.size() < 2 || !std::isfinite(max_step) || max_step <= 1e-6) {
        return path;
    }

    std::vector<nbcam::UVPathPoint> dense;
    dense.reserve(path.size() * 2);
    dense.push_back(path.front());

    constexpr int kMaxSubdivision = 256;

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        const auto& a = path[i];
        const auto& b = path[i + 1];

        if (!b.is_jump_before &&
            std::isfinite(a.u) && std::isfinite(a.v) &&
            std::isfinite(b.u) && std::isfinite(b.v)) {
            const double dx = b.u - a.u;
            const double dy = b.v - a.v;
            const double len = std::sqrt(dx * dx + dy * dy);
            if (std::isfinite(len) && len > max_step) {
                int subdiv = static_cast<int>(std::ceil(len / max_step));
                subdiv = std::max(1, std::min(subdiv, kMaxSubdivision));
                for (int s = 1; s < subdiv; ++s) {
                    const double t = static_cast<double>(s) / static_cast<double>(subdiv);
                    nbcam::UVPathPoint mid;
                    mid.u = a.u + dx * t;
                    mid.v = a.v + dy * t;
                    mid.grayscale = a.grayscale * (1.0 - t) + b.grayscale * t;
                    mid.is_jump_before = false;
                    mid.is_arrow_tip = false;
                    dense.push_back(mid);
                }
            }
        }

        dense.push_back(b);
    }

    return dense;
}

double svgSubPathArea(const std::vector<nbcam::SVGPathPoint>& path)
{
    if (path.size() < 3) {
        return 0.0;
    }
    double area2 = 0.0;
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& p = path[i];
        const auto& q = path[(i + 1) % path.size()];
        area2 += p.x * q.y - q.x * p.y;
    }
    return 0.5 * area2;
}

std::vector<std::vector<nbcam::SVGPathPoint>> splitSvgSubPaths(const std::vector<nbcam::SVGPathPoint>& points)
{
    std::vector<std::vector<nbcam::SVGPathPoint>> result;
    std::vector<nbcam::SVGPathPoint> current;
    for (const auto& p : points) {
        if (p.is_move_to && !current.empty()) {
            result.push_back(current);
            current.clear();
        }
        current.push_back(p);
    }
    if (!current.empty()) {
        result.push_back(current);
    }
    return result;
}

double uvContourSignedArea(const std::vector<nbcam::UVCoord>& contour)
{
    if (contour.size() < 3) {
        return 0.0;
    }

    double area2 = 0.0;
    for (size_t i = 0; i < contour.size(); ++i) {
        const auto& p = contour[i];
        const auto& q = contour[(i + 1) % contour.size()];
        area2 += p.u * q.v - q.u * p.v;
    }
    return 0.5 * area2;
}

size_t findContourLeftTopStartIndex(const std::vector<nbcam::UVCoord>& contour)
{
    if (contour.empty()) {
        return 0;
    }

    size_t best_idx = 0;
    bool initialized = false;
    double best_u = 0.0;
    double best_v = 0.0;
    for (size_t i = 0; i < contour.size(); ++i) {
        const auto& uv = contour[i];
        if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) {
            continue;
        }
        if (!initialized || uv.u < best_u - 1e-12 ||
            (std::abs(uv.u - best_u) <= 1e-12 && uv.v > best_v + 1e-12)) {
            best_idx = i;
            best_u = uv.u;
            best_v = uv.v;
            initialized = true;
        }
    }
    return best_idx;
}

void normalizeContourStartLeftTop(std::vector<nbcam::UVCoord>& contour)
{
    if (contour.size() < 2) {
        return;
    }
    const size_t start_idx = findContourLeftTopStartIndex(contour);
    if (start_idx > 0 && start_idx < contour.size()) {
        std::rotate(contour.begin(), contour.begin() + static_cast<std::ptrdiff_t>(start_idx), contour.end());
    }
}

std::pair<double, double> contourStartAnchor(const std::vector<nbcam::UVCoord>& contour)
{
    if (contour.empty()) {
        return {0.0, 0.0};
    }
    const auto& p = contour.front();
    if (std::isfinite(p.u) && std::isfinite(p.v)) {
        return {p.u, p.v};
    }

    const size_t idx = findContourLeftTopStartIndex(contour);
    if (idx < contour.size() &&
        std::isfinite(contour[idx].u) &&
        std::isfinite(contour[idx].v)) {
        return {contour[idx].u, contour[idx].v};
    }
    return {0.0, 0.0};
}

double anchorDistance(const std::pair<double, double>& a, const std::pair<double, double>& b)
{
    const double dx = a.first - b.first;
    const double dy = a.second - b.second;
    return std::sqrt(dx * dx + dy * dy);
}

std::vector<size_t> buildContourOrderByTsp(const std::vector<std::vector<nbcam::UVCoord>>& contours)
{
    const size_t n = contours.size();
    std::vector<size_t> order;
    order.reserve(n);
    if (n == 0) {
        return order;
    }

    std::vector<std::pair<double, double>> anchors;
    anchors.reserve(n);
    for (const auto& loop : contours) {
        anchors.push_back(contourStartAnchor(loop));
    }

    size_t start_idx = 0;
    for (size_t i = 1; i < n; ++i) {
        const auto& cand = anchors[i];
        const auto& best = anchors[start_idx];
        if (cand.first < best.first - 1e-12 ||
            (std::abs(cand.first - best.first) <= 1e-12 && cand.second > best.second + 1e-12)) {
            start_idx = i;
        }
    }

    std::vector<char> visited(n, 0);
    size_t current = start_idx;
    visited[current] = 1;
    order.push_back(current);
    for (size_t k = 1; k < n; ++k) {
        size_t next_idx = n;
        double next_dist = std::numeric_limits<double>::infinity();
        for (size_t j = 0; j < n; ++j) {
            if (visited[j]) {
                continue;
            }
            const double d = anchorDistance(anchors[current], anchors[j]);
            if (d < next_dist) {
                next_dist = d;
                next_idx = j;
            }
        }
        if (next_idx >= n) {
            break;
        }
        visited[next_idx] = 1;
        order.push_back(next_idx);
        current = next_idx;
    }

    // 2-opt refinement for open path (keep first node fixed).
    if (order.size() >= 4) {
        bool improved = true;
        size_t guard = 0;
        while (improved && guard < 64) {
            improved = false;
            ++guard;
            for (size_t i = 1; i + 2 < order.size(); ++i) {
                for (size_t k = i + 1; k + 1 < order.size(); ++k) {
                    const size_t a = order[i - 1];
                    const size_t b = order[i];
                    const size_t c = order[k];
                    const size_t d = order[k + 1];
                    const double old_cost = anchorDistance(anchors[a], anchors[b]) +
                                            anchorDistance(anchors[c], anchors[d]);
                    const double new_cost = anchorDistance(anchors[a], anchors[c]) +
                                            anchorDistance(anchors[b], anchors[d]);
                    if (new_cost + 1e-9 < old_cost) {
                        std::reverse(order.begin() + static_cast<std::ptrdiff_t>(i),
                                     order.begin() + static_cast<std::ptrdiff_t>(k + 1));
                        improved = true;
                    }
                }
            }
        }
    }

    return order;
}

void reorderContoursWithTsp(std::vector<std::vector<nbcam::UVCoord>>& contours)
{
    if (contours.size() <= 1) {
        return;
    }

    for (auto& loop : contours) {
        normalizeContourStartLeftTop(loop);
    }

    const std::vector<size_t> order = buildContourOrderByTsp(contours);
    if (order.size() != contours.size()) {
        return;
    }

    std::vector<std::vector<nbcam::UVCoord>> reordered;
    reordered.reserve(contours.size());
    for (size_t idx : order) {
        if (idx < contours.size()) {
            reordered.push_back(std::move(contours[idx]));
        }
    }
    if (reordered.size() == contours.size()) {
        contours = std::move(reordered);
    }
}

bool getPatchUVBounds(const nbcam::Patch& patch,
                      const nbcam::TriangleMesh& mesh,
                      const std::vector<nbcam::UVCoord>& uv_coords,
                      double& u_min,
                      double& u_max,
                      double& v_min,
                      double& v_max)
{
    bool initialized = false;
    std::vector<double> u_samples;
    std::vector<double> v_samples;
    const auto collectVertex = [&](size_t idx) {
        if (idx >= uv_coords.size() || !isFiniteUV(uv_coords[idx])) {
            return;
        }
        const auto& uv = uv_coords[idx];
        u_samples.push_back(uv.u);
        v_samples.push_back(uv.v);
        if (!initialized) {
            u_min = u_max = uv.u;
            v_min = v_max = uv.v;
            initialized = true;
            return;
        }
        u_min = std::min(u_min, uv.u);
        u_max = std::max(u_max, uv.u);
        v_min = std::min(v_min, uv.v);
        v_max = std::max(v_max, uv.v);
    };

    for (size_t i = 0; i + 1 < patch.boundary_edges.size(); i += 2) {
        const size_t a = patch.boundary_edges[i];
        const size_t b = patch.boundary_edges[i + 1];
        for (size_t idx : {a, b}) {
            collectVertex(idx);
        }
    }

    for (size_t tri_idx : patch.triangle_indices) {
        if (tri_idx >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[tri_idx];
        collectVertex(tri.v0);
        collectVertex(tri.v1);
        collectVertex(tri.v2);
    }

    if (!initialized || u_max <= u_min || v_max <= v_min) {
        return false;
    }

    if (u_samples.size() >= 8 && v_samples.size() >= 8) {
        std::sort(u_samples.begin(), u_samples.end());
        std::sort(v_samples.begin(), v_samples.end());
        const auto pickPercentile = [](const std::vector<double>& values, double p) {
            const double pos = p * static_cast<double>(values.size() - 1);
            const size_t lo = static_cast<size_t>(std::floor(pos));
            const size_t hi = static_cast<size_t>(std::ceil(pos));
            if (lo == hi) {
                return values[lo];
            }
            const double t = pos - static_cast<double>(lo);
            return values[lo] * (1.0 - t) + values[hi] * t;
        };

        const double raw_u_range = u_max - u_min;
        const double raw_v_range = v_max - v_min;
        const double robust_u_min = pickPercentile(u_samples, 0.05);
        const double robust_u_max = pickPercentile(u_samples, 0.95);
        const double robust_v_min = pickPercentile(v_samples, 0.05);
        const double robust_v_max = pickPercentile(v_samples, 0.95);
        const double robust_u_range = robust_u_max - robust_u_min;
        const double robust_v_range = robust_v_max - robust_v_min;

        if (robust_u_range > 1e-9 && raw_u_range > robust_u_range * 1.5) {
            u_min = robust_u_min;
            u_max = robust_u_max;
        }
        if (robust_v_range > 1e-9 && raw_v_range > robust_v_range * 1.5) {
            v_min = robust_v_min;
            v_max = robust_v_max;
        }
    }

    return u_max > u_min && v_max > v_min;
}

struct MappedSvgShapeContours {
    Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::NonZero;
    std::vector<std::vector<nbcam::UVCoord>> loops;
};

Clipper2Lib::FillRule toClipperFillRule(nbcam::SVGFillRule rule)
{
    return (rule == nbcam::SVGFillRule::EVENODD)
               ? Clipper2Lib::FillRule::EvenOdd
               : Clipper2Lib::FillRule::NonZero;
}

bool computeSvgShapeBounds(const std::vector<nbcam::SVGShapePaths>& svg_shapes,
                           double& x_min,
                           double& x_max,
                           double& y_min,
                           double& y_max)
{
    bool initialized = false;
    for (const auto& shape : svg_shapes) {
        for (const auto& loop : shape.loops) {
            for (const auto& p : loop.points) {
                if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                    continue;
                }
                if (!initialized) {
                    x_min = x_max = p.x;
                    y_min = y_max = p.y;
                    initialized = true;
                } else {
                    x_min = std::min(x_min, p.x);
                    x_max = std::max(x_max, p.x);
                    y_min = std::min(y_min, p.y);
                    y_max = std::max(y_max, p.y);
                }
            }
        }
    }
    return initialized && x_max > x_min && y_max > y_min;
}

struct SvgAspectFitFrame {
    double u_min = 0.0;
    double u_max = 1.0;
    double v_min = 0.0;
    double v_max = 1.0;
    double u_range = 1.0;
    double v_range = 1.0;
};

bool computeSvgAspectFitFrame(double patch_u_min,
                              double patch_u_max,
                              double patch_v_min,
                              double patch_v_max,
                              double content_aspect_wh,
                              SvgAspectFitFrame& out)
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

    double fit_u_range = patch_u_range;
    double fit_v_range = patch_v_range;
    if (patch_aspect > content_aspect_wh) {
        fit_v_range = patch_v_range;
        fit_u_range = fit_v_range * content_aspect_wh;
    } else {
        fit_u_range = patch_u_range;
        fit_v_range = fit_u_range / content_aspect_wh;
    }

    out.u_range = fit_u_range;
    out.v_range = fit_v_range;
    out.u_min = patch_center_u - fit_u_range * 0.5;
    out.u_max = patch_center_u + fit_u_range * 0.5;
    out.v_min = patch_center_v - fit_v_range * 0.5;
    out.v_max = patch_center_v + fit_v_range * 0.5;
    return out.u_range > 1e-9 && out.v_range > 1e-9;
}

std::vector<std::vector<nbcam::SVGPathPoint>> extractSvgBoundaryLoopsFromShapes(const std::vector<nbcam::SVGShapePaths>& svg_shapes)
{
    std::vector<std::vector<nbcam::SVGPathPoint>> svg_boundary_loops;
    if (svg_shapes.empty()) {
        return svg_boundary_loops;
    }

    constexpr int kClipperPrecision = 8;
    constexpr double kMergeTol = 1e-9;
    Clipper2Lib::PathsD union_all;

    for (const auto& shape : svg_shapes) {
        Clipper2Lib::PathsD subject_paths;
        subject_paths.reserve(shape.loops.size());

        for (const auto& loop : shape.loops) {
            if (loop.points.size() < 3) {
                continue;
            }

            Clipper2Lib::PathD path;
            path.reserve(loop.points.size());
            for (const auto& pt : loop.points) {
                if (!std::isfinite(pt.x) || !std::isfinite(pt.y)) {
                    continue;
                }
                if (!path.empty()) {
                    const auto& prev = path.back();
                    if (std::hypot(prev.x - pt.x, prev.y - pt.y) <= kMergeTol) {
                        continue;
                    }
                }
                path.emplace_back(pt.x, pt.y);
            }
            if (path.size() < 3) {
                continue;
            }
            if (std::hypot(path.front().x - path.back().x, path.front().y - path.back().y) <= kMergeTol) {
                path.pop_back();
            }
            if (path.size() >= 3) {
                subject_paths.push_back(std::move(path));
            }
        }

        if (subject_paths.empty()) {
            continue;
        }

        const auto shape_filled = Clipper2Lib::Union(subject_paths, toClipperFillRule(shape.fill_rule), kClipperPrecision);
        if (shape_filled.empty()) {
            continue;
        }

        if (union_all.empty()) {
            union_all = shape_filled;
        } else {
            union_all = Clipper2Lib::Union(union_all, shape_filled, Clipper2Lib::FillRule::NonZero, kClipperPrecision);
        }
    }

    svg_boundary_loops.reserve(union_all.size());
    for (const auto& path : union_all) {
        if (path.size() < 3) {
            continue;
        }
        std::vector<nbcam::SVGPathPoint> loop;
        loop.reserve(path.size());
        for (size_t i = 0; i < path.size(); ++i) {
            const auto& pt = path[i];
            if (!std::isfinite(pt.x) || !std::isfinite(pt.y)) {
                continue;
            }
            nbcam::SVGPathPoint svg_pt;
            svg_pt.x = pt.x;
            svg_pt.y = pt.y;
            svg_pt.is_move_to = (i == 0);
            loop.push_back(svg_pt);
        }
        if (loop.size() >= 3) {
            svg_boundary_loops.push_back(std::move(loop));
        }
    }

    std::sort(svg_boundary_loops.begin(), svg_boundary_loops.end(),
              [](const std::vector<nbcam::SVGPathPoint>& a, const std::vector<nbcam::SVGPathPoint>& b) {
                  return std::abs(svgSubPathArea(a)) > std::abs(svgSubPathArea(b));
              });

    return svg_boundary_loops;
}

std::vector<std::vector<nbcam::SVGPathPoint>> collectRawSvgLoops(const std::vector<nbcam::SVGShapePaths>& svg_shapes)
{
    std::vector<std::vector<nbcam::SVGPathPoint>> loops;
    loops.reserve(svg_shapes.size());
    for (const auto& shape : svg_shapes) {
        for (const auto& loop : shape.loops) {
            if (loop.points.size() < 3) {
                continue;
            }
            std::vector<nbcam::SVGPathPoint> raw_loop;
            raw_loop.reserve(loop.points.size());
            for (size_t i = 0; i < loop.points.size(); ++i) {
                const auto& p = loop.points[i];
                if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                    continue;
                }
                nbcam::SVGPathPoint svg_pt;
                svg_pt.x = p.x;
                svg_pt.y = p.y;
                svg_pt.grayscale = p.grayscale;
                svg_pt.is_move_to = (i == 0);
                raw_loop.push_back(svg_pt);
            }
            if (raw_loop.size() >= 3) {
                loops.push_back(std::move(raw_loop));
            }
        }
    }
    std::sort(loops.begin(), loops.end(),
              [](const std::vector<nbcam::SVGPathPoint>& a, const std::vector<nbcam::SVGPathPoint>& b) {
                  return std::abs(svgSubPathArea(a)) > std::abs(svgSubPathArea(b));
              });
    return loops;
}

std::vector<std::vector<nbcam::UVCoord>> mapSvgBoundaryLoopsToPatchUV(const std::vector<std::vector<nbcam::SVGPathPoint>>& svg_loops,
                                                                       const ApplicationController::PatternPlanOptions& options,
                                                                       double svg_x_min,
                                                                       double svg_x_max,
                                                                       double svg_y_min,
                                                                       double svg_y_max,
                                                                       double patch_u_min,
                                                                       double patch_u_max,
                                                                       double patch_v_min,
                                                                       double patch_v_max)
{
    std::vector<std::vector<nbcam::UVCoord>> mapped_loops;
    if (svg_loops.empty()) {
        return mapped_loops;
    }

    const double svg_w = svg_x_max - svg_x_min;
    const double svg_h = svg_y_max - svg_y_min;
    if (!(svg_w > 1e-9) || !(svg_h > 1e-9)) {
        return mapped_loops;
    }

    SvgAspectFitFrame fit_frame;
    const double svg_aspect_wh = svg_w / svg_h;
    if (!computeSvgAspectFitFrame(patch_u_min, patch_u_max, patch_v_min, patch_v_max, svg_aspect_wh, fit_frame)) {
        return mapped_loops;
    }

    const double center_u = (patch_u_min + patch_u_max) * 0.5;
    const double center_v = (patch_v_min + patch_v_max) * 0.5;
    const double cos_r = std::cos(options.tex_rotation_deg * M_PI / 180.0);
    const double sin_r = std::sin(options.tex_rotation_deg * M_PI / 180.0);
    const double sx = (std::abs(options.tex_scale_x) < 1e-9) ? 1.0 : options.tex_scale_x;
    const double sy = (std::abs(options.tex_scale_y) < 1e-9) ? 1.0 : options.tex_scale_y;
    constexpr double kMergeTol = 1e-9;

    const auto mapNormalizedSvgToPatchUv = [&](double nx, double ny) {
        const double u_transformed = fit_frame.u_min + nx * fit_frame.u_range;
        const double v_transformed = fit_frame.v_min + ny * fit_frame.v_range;

        const double u_rot = u_transformed - center_u - options.tex_translate_x;
        const double v_rot = v_transformed - center_v - options.tex_translate_y;

        const double u_scaled = u_rot * cos_r + v_rot * sin_r;
        const double v_scaled = -u_rot * sin_r + v_rot * cos_r;

        const double u_rel = u_scaled / sx;
        const double v_rel = v_scaled / sy;
        return nbcam::UVCoord{center_u + u_rel, center_v + v_rel};
    };

    mapped_loops.reserve(svg_loops.size());
    for (const auto& svg_loop : svg_loops) {
        if (svg_loop.size() < 3) {
            continue;
        }

        std::vector<nbcam::UVCoord> uv_loop;
        uv_loop.reserve(svg_loop.size());
        for (const auto& p : svg_loop) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                continue;
            }
            const double nx = (p.x - svg_x_min) / svg_w;
            const double ny = (p.y - svg_y_min) / svg_h;
            const auto uv = mapNormalizedSvgToPatchUv(nx, ny);
            if (!uv_loop.empty()) {
                const auto& prev = uv_loop.back();
                if (std::hypot(prev.u - uv.u, prev.v - uv.v) <= kMergeTol) {
                    continue;
                }
            }
            uv_loop.push_back(uv);
        }

        if (uv_loop.size() < 3) {
            continue;
        }
        if (std::hypot(uv_loop.front().u - uv_loop.back().u,
                       uv_loop.front().v - uv_loop.back().v) <= kMergeTol) {
            uv_loop.pop_back();
        }
        if (uv_loop.size() >= 3) {
            mapped_loops.push_back(std::move(uv_loop));
        }
    }

    std::sort(mapped_loops.begin(), mapped_loops.end(),
              [](const std::vector<nbcam::UVCoord>& a, const std::vector<nbcam::UVCoord>& b) {
                  return std::abs(uvContourSignedArea(a)) > std::abs(uvContourSignedArea(b));
              });

    return mapped_loops;
}

std::vector<MappedSvgShapeContours> mapSvgContoursToPatchUV(const std::vector<nbcam::SVGShapePaths>& svg_shapes,
                                                            const ApplicationController::PatternPlanOptions& options,
                                                            double patch_u_min,
                                                            double patch_u_max,
                                                            double patch_v_min,
                                                            double patch_v_max)
{
    std::vector<MappedSvgShapeContours> mapped_shapes;
    if (svg_shapes.empty()) {
        return mapped_shapes;
    }

    bool initialized = false;
    double svg_x_min = 0.0;
    double svg_x_max = 0.0;
    double svg_y_min = 0.0;
    double svg_y_max = 0.0;
    for (const auto& shape : svg_shapes) {
        for (const auto& loop : shape.loops) {
            for (const auto& p : loop.points) {
                if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                    continue;
                }
                if (!initialized) {
                    svg_x_min = svg_x_max = p.x;
                    svg_y_min = svg_y_max = p.y;
                    initialized = true;
                } else {
                    svg_x_min = std::min(svg_x_min, p.x);
                    svg_x_max = std::max(svg_x_max, p.x);
                    svg_y_min = std::min(svg_y_min, p.y);
                    svg_y_max = std::max(svg_y_max, p.y);
                }
            }
        }
    }
    if (!initialized) {
        return mapped_shapes;
    }

    const double svg_w = svg_x_max - svg_x_min;
    const double svg_h = svg_y_max - svg_y_min;
    if (svg_w < 1e-9 || svg_h < 1e-9) {
        return mapped_shapes;
    }

    const double center_u = (patch_u_min + patch_u_max) * 0.5;
    const double center_v = (patch_v_min + patch_v_max) * 0.5;
    const double u_range = patch_u_max - patch_u_min;
    const double v_range = patch_v_max - patch_v_min;
    const double cos_r = std::cos(options.tex_rotation_deg * M_PI / 180.0);
    const double sin_r = std::sin(options.tex_rotation_deg * M_PI / 180.0);
    const double sx = (std::abs(options.tex_scale_x) < 1e-9) ? 1.0 : options.tex_scale_x;
    const double sy = (std::abs(options.tex_scale_y) < 1e-9) ? 1.0 : options.tex_scale_y;
    constexpr double kMergeTol = 1e-9;

    const auto mapNormalizedSvgToPatchUv = [&](double nx, double ny) {
        const double u_transformed = patch_u_min + nx * u_range;
        const double v_transformed = patch_v_min + ny * v_range;

        const double u_rot = u_transformed - center_u - options.tex_translate_x;
        const double v_rot = v_transformed - center_v - options.tex_translate_y;

        const double u_scaled = u_rot * cos_r + v_rot * sin_r;
        const double v_scaled = -u_rot * sin_r + v_rot * cos_r;

        const double u_rel = u_scaled / sx;
        const double v_rel = v_scaled / sy;
        return nbcam::UVCoord{center_u + u_rel, center_v + v_rel};
    };

    mapped_shapes.reserve(svg_shapes.size());
    for (const auto& shape : svg_shapes) {
        MappedSvgShapeContours mapped;
        mapped.fill_rule = toClipperFillRule(shape.fill_rule);
        mapped.loops.reserve(shape.loops.size());
        for (const auto& loop : shape.loops) {
            if (loop.points.size() < 3) {
                continue;
            }

            std::vector<nbcam::UVCoord> uv_loop;
            uv_loop.reserve(loop.points.size());
            for (const auto& p : loop.points) {
                if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                    continue;
                }
                const double nx = (p.x - svg_x_min) / svg_w;
                const double ny = (p.y - svg_y_min) / svg_h;
                const auto uv = mapNormalizedSvgToPatchUv(nx, ny);
                if (!uv_loop.empty()) {
                    const auto& prev = uv_loop.back();
                    if (std::hypot(prev.u - uv.u, prev.v - uv.v) <= kMergeTol) {
                        continue;
                    }
                }
                uv_loop.push_back(uv);
            }
            if (uv_loop.size() < 3) {
                continue;
            }
            if (std::hypot(uv_loop.front().u - uv_loop.back().u,
                           uv_loop.front().v - uv_loop.back().v) <= kMergeTol) {
                uv_loop.pop_back();
            }
            if (uv_loop.size() >= 3) {
                mapped.loops.push_back(std::move(uv_loop));
            }
        }

        if (!mapped.loops.empty()) {
            mapped_shapes.push_back(std::move(mapped));
        }
    }

    return mapped_shapes;
}

std::vector<std::vector<nbcam::UVCoord>> unionSvgContours(const std::vector<MappedSvgShapeContours>& mapped_shapes)
{
    std::vector<std::vector<nbcam::UVCoord>> contour_uv_loops;
    if (mapped_shapes.empty()) {
        return contour_uv_loops;
    }

    constexpr int kClipperPrecision = 8;
    Clipper2Lib::PathsD union_all;

    for (const auto& shape : mapped_shapes) {
        Clipper2Lib::PathsD subject_paths;
        subject_paths.reserve(shape.loops.size());
        for (const auto& loop : shape.loops) {
            if (loop.size() < 3) {
                continue;
            }
            Clipper2Lib::PathD p;
            p.reserve(loop.size());
            for (const auto& uv : loop) {
                if (std::isfinite(uv.u) && std::isfinite(uv.v)) {
                    p.emplace_back(uv.u, uv.v);
                }
            }
            if (p.size() >= 3) {
                subject_paths.push_back(std::move(p));
            }
        }
        if (subject_paths.empty()) {
            continue;
        }

        const auto shape_filled = Clipper2Lib::Union(subject_paths, shape.fill_rule, kClipperPrecision);
        if (shape_filled.empty()) {
            continue;
        }

        if (union_all.empty()) {
            union_all = shape_filled;
        } else {
            union_all = Clipper2Lib::Union(union_all, shape_filled, Clipper2Lib::FillRule::NonZero, kClipperPrecision);
        }
    }

    contour_uv_loops.reserve(union_all.size());
    for (const auto& path : union_all) {
        if (path.size() < 3) {
            continue;
        }
        std::vector<nbcam::UVCoord> loop;
        loop.reserve(path.size());
        for (const auto& pt : path) {
            if (std::isfinite(pt.x) && std::isfinite(pt.y)) {
                loop.push_back(nbcam::UVCoord{pt.x, pt.y});
            }
        }
        if (loop.size() >= 3) {
            contour_uv_loops.push_back(std::move(loop));
        }
    }

    std::sort(contour_uv_loops.begin(), contour_uv_loops.end(),
              [](const std::vector<nbcam::UVCoord>& a, const std::vector<nbcam::UVCoord>& b) {
                  return std::abs(uvContourSignedArea(a)) > std::abs(uvContourSignedArea(b));
              });

    return contour_uv_loops;
}

std::vector<nbcam::UVCoord> pickLargestContour(const std::vector<std::vector<nbcam::UVCoord>>& contours)
{
    std::vector<nbcam::UVCoord> best;
    double best_area = 0.0;
    for (const auto& contour : contours) {
        if (contour.size() < 3) {
            continue;
        }
        const double area = std::abs(uvContourSignedArea(contour));
        if (area > best_area) {
            best_area = area;
            best = contour;
        }
    }
    return best;
}

std::vector<nbcam::UVCoord> mapSvgBoundingRectToPatchUV(double svg_x_min,
                                                        double svg_x_max,
                                                        double svg_y_min,
                                                        double svg_y_max,
                                                        const ApplicationController::PatternPlanOptions& options,
                                                        double patch_u_min,
                                                        double patch_u_max,
                                                        double patch_v_min,
                                                        double patch_v_max)
{
    if ((svg_x_max - svg_x_min) < 1e-9 || (svg_y_max - svg_y_min) < 1e-9) {
        return {};
    }

    std::vector<std::vector<nbcam::SVGPathPoint>> rect_loops;
    rect_loops.push_back({
        nbcam::SVGPathPoint{svg_x_min, svg_y_min, true},
        nbcam::SVGPathPoint{svg_x_max, svg_y_min, false},
        nbcam::SVGPathPoint{svg_x_max, svg_y_max, false},
        nbcam::SVGPathPoint{svg_x_min, svg_y_max, false}
    });

    auto mapped = mapSvgBoundaryLoopsToPatchUV(rect_loops, options,
                                               svg_x_min, svg_x_max, svg_y_min, svg_y_max,
                                               patch_u_min, patch_u_max, patch_v_min, patch_v_max);
    if (mapped.empty()) {
        return {};
    }
    return mapped.front();
}

struct UvStretchSample {
    double u = 0.0;
    double v = 0.0;
    double stretch = 1.0;
};

std::vector<UvStretchSample> buildPatchUvStretchSamples(const nbcam::Patch& patch,
                                                        const nbcam::TriangleMesh& mesh,
                                                        const std::vector<nbcam::UVCoord>& uv_coords)
{
    std::vector<UvStretchSample> samples;
    samples.reserve(patch.triangle_indices.size());
    for (size_t tri_idx : patch.triangle_indices) {
        if (tri_idx >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[tri_idx];
        if (tri.v0 >= mesh.vertices.size() || tri.v1 >= mesh.vertices.size() || tri.v2 >= mesh.vertices.size() ||
            tri.v0 >= uv_coords.size() || tri.v1 >= uv_coords.size() || tri.v2 >= uv_coords.size()) {
            continue;
        }

        const auto& uv0 = uv_coords[tri.v0];
        const auto& uv1 = uv_coords[tri.v1];
        const auto& uv2 = uv_coords[tri.v2];
        if (!isFiniteUV(uv0) || !isFiniteUV(uv1) || !isFiniteUV(uv2)) {
            continue;
        }

        const double area_uv = 0.5 * std::abs((uv1.u - uv0.u) * (uv2.v - uv0.v) - (uv1.v - uv0.v) * (uv2.u - uv0.u));
        if (!(area_uv > 1e-14)) {
            continue;
        }

        const auto& p0 = mesh.vertices[tri.v0];
        const auto& p1 = mesh.vertices[tri.v1];
        const auto& p2 = mesh.vertices[tri.v2];
        const double ax = p1.x - p0.x;
        const double ay = p1.y - p0.y;
        const double az = p1.z - p0.z;
        const double bx = p2.x - p0.x;
        const double by = p2.y - p0.y;
        const double bz = p2.z - p0.z;
        const double cx = ay * bz - az * by;
        const double cy = az * bx - ax * bz;
        const double cz = ax * by - ay * bx;
        const double area_xyz = 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
        if (!(area_xyz > 1e-14)) {
            continue;
        }

        const double stretch = std::clamp(std::sqrt(area_xyz / area_uv), 0.2, 5.0);
        samples.push_back(UvStretchSample{
            (uv0.u + uv1.u + uv2.u) / 3.0,
            (uv0.v + uv1.v + uv2.v) / 3.0,
            stretch
        });
    }
    return samples;
}

double sampleLocalStretch(const std::vector<UvStretchSample>& samples, double u, double v)
{
    if (samples.empty()) {
        return 1.0;
    }

    double weighted_sum = 0.0;
    double weight_sum = 0.0;
    for (const auto& s : samples) {
        const double du = u - s.u;
        const double dv = v - s.v;
        const double d2 = du * du + dv * dv;
        const double w = 1.0 / (1e-10 + d2);
        weighted_sum += w * s.stretch;
        weight_sum += w;
    }
    if (weight_sum <= 1e-12) {
        return 1.0;
    }
    return std::clamp(weighted_sum / weight_sum, 0.2, 5.0);
}

double robustMeanStretch(const std::vector<UvStretchSample>& samples)
{
    if (samples.empty()) {
        return 1.0;
    }
    std::vector<double> vals;
    vals.reserve(samples.size());
    for (const auto& s : samples) {
        vals.push_back(s.stretch);
    }
    std::sort(vals.begin(), vals.end());
    return vals[vals.size() / 2];
}

std::pair<double, double> computeUvCentroid(const std::vector<nbcam::UVCoord>& points,
                                            double fallback_u,
                                            double fallback_v)
{
    double sum_u = 0.0;
    double sum_v = 0.0;
    size_t count = 0;
    for (const auto& uv : points) {
        if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) {
            continue;
        }
        sum_u += uv.u;
        sum_v += uv.v;
        ++count;
    }
    if (count == 0) {
        return {fallback_u, fallback_v};
    }
    return {sum_u / static_cast<double>(count), sum_v / static_cast<double>(count)};
}

std::pair<double, double> computeUvPathCentroid(const std::vector<nbcam::UVPathPoint>& points,
                                                double fallback_u,
                                                double fallback_v)
{
    double sum_u = 0.0;
    double sum_v = 0.0;
    size_t count = 0;
    for (const auto& pt : points) {
        if (!std::isfinite(pt.u) || !std::isfinite(pt.v)) {
            continue;
        }
        sum_u += pt.u;
        sum_v += pt.v;
        ++count;
    }
    if (count == 0) {
        return {fallback_u, fallback_v};
    }
    return {sum_u / static_cast<double>(count), sum_v / static_cast<double>(count)};
}

void applyInverseStretchPrewarpPoint(nbcam::UVCoord& uv,
                                     const std::vector<UvStretchSample>& samples,
                                     double mean_stretch,
                                     double center_u,
                                     double center_v,
                                     double strength,
                                     double patch_u_min,
                                     double patch_u_max,
                                     double patch_v_min,
                                     double patch_v_max)
{
    if (!std::isfinite(uv.u) || !std::isfinite(uv.v) || samples.empty()) {
        return;
    }
    const double local_stretch = sampleLocalStretch(samples, uv.u, uv.v);
    if (!(local_stretch > 1e-9) || !(mean_stretch > 1e-9)) {
        return;
    }

    const double alpha = std::clamp(strength, 0.0, 2.0);
    double scale = std::pow(mean_stretch / local_stretch, alpha);
    scale = std::clamp(scale, 0.25, 4.0);

    uv.u = center_u + (uv.u - center_u) * scale;
    uv.v = center_v + (uv.v - center_v) * scale;
    uv.u = std::clamp(uv.u, patch_u_min, patch_u_max);
    uv.v = std::clamp(uv.v, patch_v_min, patch_v_max);
}

void applyInverseStretchPrewarpContours(std::vector<std::vector<nbcam::UVCoord>>& loops,
                                        const std::vector<UvStretchSample>& samples,
                                        double mean_stretch,
                                        double center_u,
                                        double center_v,
                                        double strength,
                                        double patch_u_min,
                                        double patch_u_max,
                                        double patch_v_min,
                                        double patch_v_max)
{
    for (auto& loop : loops) {
        const auto [loop_center_u, loop_center_v] = computeUvCentroid(loop, center_u, center_v);
        for (auto& uv : loop) {
            applyInverseStretchPrewarpPoint(uv, samples, mean_stretch, loop_center_u, loop_center_v, strength,
                                            patch_u_min, patch_u_max, patch_v_min, patch_v_max);
        }
    }
}

void applyInverseStretchPrewarpBoundary(std::vector<nbcam::UVCoord>& boundary,
                                        const std::vector<UvStretchSample>& samples,
                                        double mean_stretch,
                                        double center_u,
                                        double center_v,
                                        double strength,
                                        double patch_u_min,
                                        double patch_u_max,
                                        double patch_v_min,
                                        double patch_v_max)
{
    const auto [boundary_center_u, boundary_center_v] = computeUvCentroid(boundary, center_u, center_v);
    for (auto& uv : boundary) {
        applyInverseStretchPrewarpPoint(uv, samples, mean_stretch, boundary_center_u, boundary_center_v, strength,
                                        patch_u_min, patch_u_max, patch_v_min, patch_v_max);
    }
}

void applyInverseStretchPrewarpPath(std::vector<nbcam::UVPathPoint>& uv_path,
                                    const std::vector<UvStretchSample>& samples,
                                    double mean_stretch,
                                    double center_u,
                                    double center_v,
                                    double strength,
                                    double patch_u_min,
                                    double patch_u_max,
                                    double patch_v_min,
                                    double patch_v_max)
{
    const auto [path_center_u, path_center_v] = computeUvPathCentroid(uv_path, center_u, center_v);
    for (auto& pt : uv_path) {
        nbcam::UVCoord uv{pt.u, pt.v};
        applyInverseStretchPrewarpPoint(uv, samples, mean_stretch, path_center_u, path_center_v, strength,
                                        patch_u_min, patch_u_max, patch_v_min, patch_v_max);
        pt.u = uv.u;
        pt.v = uv.v;
    }
}

nbcam::PathPoint clonePathPointWithLaser(const nbcam::PathPoint& src, int laser_override)
{
    nbcam::PathPoint dst;
    dst.u = src.u;
    dst.v = src.v;
    dst.x = src.x;
    dst.y = src.y;
    dst.z = src.z;
    dst.a = src.a;
    dst.b = src.b;
    dst.laser = (laser_override >= 0) ? laser_override : src.laser;
    dst.grayscale = src.grayscale;
    if (src.params_override) {
        dst.params_override = std::make_unique<nbcam::ProcessParams>(*src.params_override);
    }
    return dst;
}

std::vector<std::vector<nbcam::PathPoint>> clonePathLoopsWithLaser(
    const std::vector<std::vector<nbcam::PathPoint>>& src,
    int laser_override)
{
    std::vector<std::vector<nbcam::PathPoint>> dst;
    dst.reserve(src.size());
    for (const auto& loop : src) {
        std::vector<nbcam::PathPoint> copied_loop;
        copied_loop.reserve(loop.size());
        for (const auto& point : loop) {
            copied_loop.push_back(clonePathPointWithLaser(point, laser_override));
        }
        dst.push_back(std::move(copied_loop));
    }
    return dst;
}

double sampleSvgPointGrayscale(double uv_u,
                               double uv_v,
                               const ApplicationController::PatternPlanOptions& options,
                               double patch_u_min,
                               double patch_u_max,
                               double patch_v_min,
                               double patch_v_max,
                               const std::vector<unsigned char>& rgba,
                               int mask_w,
                               int mask_h);

bool buildContourMarkPathOnPatch(const std::vector<nbcam::UVCoord>& contour_uv,
                                 double densify_step,
                                 const nbcam::TriangleMesh& mesh,
                                 const std::vector<nbcam::UVCoord>& uv_coords,
                                 const std::vector<size_t>& patch_triangle_indices,
                                 const ApplicationController::PatternPlanOptions& options,
                                 double patch_u_min,
                                 double patch_u_max,
                                 double patch_v_min,
                                 double patch_v_max,
                                 const std::vector<unsigned char>& rgba_mask,
                                 int mask_w,
                                 int mask_h,
                                 std::vector<nbcam::UVPathPoint>& contour_uv_out,
                                 std::vector<nbcam::PathPoint>& contour_xyz_out)
{
    contour_uv_out.clear();
    contour_xyz_out.clear();
    if (contour_uv.size() < 3 || uv_coords.empty()) {
        return false;
    }

    constexpr double kCloseTol = 1e-6;
    std::vector<nbcam::UVPathPoint> contour_path;
    contour_path.reserve(contour_uv.size() + 1);
    for (const auto& uv : contour_uv) {
        if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) {
            continue;
        }

        if (!contour_path.empty()) {
            const auto& prev = contour_path.back();
            if (std::hypot(uv.u - prev.u, uv.v - prev.v) <= kCloseTol) {
                continue;
            }
        }

        nbcam::UVPathPoint pt;
        pt.u = uv.u;
        pt.v = uv.v;
        pt.is_jump_before = false;
        pt.is_arrow_tip = false;
        contour_path.push_back(pt);
    }

    if (contour_path.size() < 3) {
        return false;
    }

    const auto& first = contour_path.front();
    const auto& last = contour_path.back();
    if (std::hypot(first.u - last.u, first.v - last.v) > kCloseTol) {
        nbcam::UVPathPoint close = first;
        close.is_jump_before = false;
        close.is_arrow_tip = true;
        contour_path.push_back(close);
    }

    for (auto& uv : contour_path) {
        uv.is_jump_before = false;
        uv.is_arrow_tip = false;
    }
    contour_path.front().is_jump_before = true;

    contour_path = densifyUVPathSegments(contour_path, std::max(1e-6, densify_step));

    if (contour_path.size() < 3) {
        return false;
    }

    contour_path.front().is_jump_before = true;
    contour_path.back().is_arrow_tip = true;

    nbcam::UVMapper mapper;
    auto mapped = mapper.mapUVToXYZ(contour_path, mesh, uv_coords, &patch_triangle_indices);
    if (mapped.size() != contour_path.size()) {
        return false;
    }

    contour_uv_out = contour_path;
    for (auto& uv : contour_uv_out) {
        uv.grayscale = sampleSvgPointGrayscale(uv.u, uv.v, options,
                                               patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                               rgba_mask, mask_w, mask_h);
    }
    contour_xyz_out.reserve(mapped.size());
    for (size_t i = 0; i < mapped.size(); ++i) {
        const auto& p = mapped[i];
        if (p.laser == 0 ||
            !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
            return false;
        }
        nbcam::PathPoint point = clonePathPointWithLaser(p, 1);
        if (i < contour_uv_out.size()) {
            point.grayscale = contour_uv_out[i].grayscale;
        }
        contour_xyz_out.push_back(std::move(point));
    }

    return contour_xyz_out.size() >= 2;
}

bool prependContourSegmentsToJob(nbcam::LaserJob& job,
                                 const std::vector<std::vector<nbcam::PathPoint>>& contour_loops_xyz)
{
    if (contour_loops_xyz.empty()) {
        return false;
    }

    std::vector<nbcam::PathSegment> merged_segments;
    merged_segments.reserve(job.segments.size() + contour_loops_xyz.size() * 2 + 2);
    bool has_contour = false;

    for (const auto& contour_xyz : contour_loops_xyz) {
        if (contour_xyz.size() < 2) {
            continue;
        }

        if (!merged_segments.empty() && !merged_segments.back().points.empty()) {
            nbcam::PathSegment jump_between_contours;
            jump_between_contours.type = nbcam::SegmentType::JUMP;
            jump_between_contours.strategy = nbcam::FillStrategy::CONTOUR;
            jump_between_contours.points.reserve(2);
            jump_between_contours.points.push_back(clonePathPointWithLaser(merged_segments.back().points.back(), 0));
            jump_between_contours.points.push_back(clonePathPointWithLaser(contour_xyz.front(), 0));
            merged_segments.push_back(std::move(jump_between_contours));
        }

        nbcam::PathSegment contour_segment;
        contour_segment.type = nbcam::SegmentType::MARK;
        contour_segment.strategy = nbcam::FillStrategy::CONTOUR;
        contour_segment.points.reserve(contour_xyz.size());
        for (const auto& p : contour_xyz) {
            contour_segment.points.push_back(clonePathPointWithLaser(p, 1));
        }
        merged_segments.push_back(std::move(contour_segment));
        has_contour = true;
    }

    if (!has_contour) {
        return false;
    }

    if (!job.segments.empty() &&
        !merged_segments.empty() &&
        !merged_segments.back().points.empty() &&
        !job.segments.front().points.empty()) {
        nbcam::PathSegment jump_to_fill;
        jump_to_fill.type = nbcam::SegmentType::JUMP;
        jump_to_fill.strategy = nbcam::FillStrategy::CONTOUR;
        jump_to_fill.points.reserve(2);
        jump_to_fill.points.push_back(clonePathPointWithLaser(merged_segments.back().points.back(), 0));
        jump_to_fill.points.push_back(clonePathPointWithLaser(job.segments.front().points.front(), 0));
        merged_segments.push_back(std::move(jump_to_fill));
    }

    for (auto& segment : job.segments) {
        merged_segments.push_back(std::move(segment));
    }
    job.segments = std::move(merged_segments);

    for (size_t i = 0; i < job.segments.size(); ++i) {
        job.segments[i].id = static_cast<int>(i);
    }

    return true;
}

bool renderSvgMaskRgba(nbcam::NanosvgWrapper& parser,
                       int width,
                       int height,
                       std::vector<unsigned char>& rgba_out)
{
    if (!parser.isLoaded() || width <= 0 || height <= 0) {
        return false;
    }
    unsigned char* img_data = parser.renderToImage(width, height);
    if (!img_data) {
        return false;
    }
    rgba_out.assign(img_data, img_data + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    delete[] img_data;
    return !rgba_out.empty();
}

bool loadSvgMaskRgba(const std::string& svg_filepath,
                     int width,
                     int height,
                     std::vector<unsigned char>& rgba_out)
{
    if (svg_filepath.empty() || width <= 0 || height <= 0) {
        return false;
    }

    struct MaskCacheEntry {
        std::filesystem::file_time_type mtime{};
        bool has_mtime = false;
        std::vector<unsigned char> rgba;
    };

    static std::unordered_map<std::string, MaskCacheEntry> s_cache;

    const std::string key = svg_filepath + "|" + std::to_string(width) + "x" + std::to_string(height);
    std::filesystem::file_time_type mtime{};
    bool has_mtime = false;
    {
        std::error_code ec;
        mtime = std::filesystem::last_write_time(svg_filepath, ec);
        has_mtime = !ec;
    }

    const auto it = s_cache.find(key);
    if (it != s_cache.end()) {
        const bool mtime_match =
            (!has_mtime && !it->second.has_mtime) ||
            (has_mtime && it->second.has_mtime && it->second.mtime == mtime);
        if (mtime_match && !it->second.rgba.empty()) {
            rgba_out = it->second.rgba;
            return true;
        }
    }

    nbcam::NanosvgWrapper parser;
    if (!parser.loadFromFile(svg_filepath)) {
        return false;
    }
    if (!renderSvgMaskRgba(parser, width, height, rgba_out)) {
        return false;
    }

    MaskCacheEntry entry;
    entry.mtime = mtime;
    entry.has_mtime = has_mtime;
    entry.rgba = rgba_out;
    s_cache[key] = std::move(entry);
    return !rgba_out.empty();
}

double sampleSvgMaskAlphaBilinear(const std::vector<unsigned char>& rgba,
                                  int mask_w,
                                  int mask_h,
                                  double u_norm,
                                  double v_norm)
{
    if (rgba.empty() || mask_w <= 0 || mask_h <= 0 || !std::isfinite(u_norm) || !std::isfinite(v_norm)) {
        return 0.0;
    }
    const double x = std::clamp(u_norm, 0.0, 1.0) * static_cast<double>(mask_w - 1);
    const double y = std::clamp(v_norm, 0.0, 1.0) * static_cast<double>(mask_h - 1);
    const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, mask_w - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, mask_h - 1);
    const int x1 = std::min(x0 + 1, mask_w - 1);
    const int y1 = std::min(y0 + 1, mask_h - 1);
    const double tx = x - static_cast<double>(x0);
    const double ty = y - static_cast<double>(y0);

    const auto alphaAt = [&](int px, int py) -> double {
        const size_t idx = (static_cast<size_t>(py) * static_cast<size_t>(mask_w) + static_cast<size_t>(px)) * 4 + 3;
        if (idx >= rgba.size()) {
            return 0.0;
        }
        return static_cast<double>(rgba[idx]);
    };

    const double a00 = alphaAt(x0, y0);
    const double a10 = alphaAt(x1, y0);
    const double a01 = alphaAt(x0, y1);
    const double a11 = alphaAt(x1, y1);
    const double a0 = a00 * (1.0 - tx) + a10 * tx;
    const double a1 = a01 * (1.0 - tx) + a11 * tx;
    return a0 * (1.0 - ty) + a1 * ty;
}

double sampleSvgMaskGrayBilinear(const std::vector<unsigned char>& rgba,
                                 int mask_w,
                                 int mask_h,
                                 double u_norm,
                                 double v_norm)
{
    if (rgba.empty() || mask_w <= 0 || mask_h <= 0 || !std::isfinite(u_norm) || !std::isfinite(v_norm)) {
        return 0.0;
    }

    const double x = std::clamp(u_norm, 0.0, 1.0) * static_cast<double>(mask_w - 1);
    const double y = std::clamp(v_norm, 0.0, 1.0) * static_cast<double>(mask_h - 1);
    const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, mask_w - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, mask_h - 1);
    const int x1 = std::min(x0 + 1, mask_w - 1);
    const int y1 = std::min(y0 + 1, mask_h - 1);
    const double tx = x - static_cast<double>(x0);
    const double ty = y - static_cast<double>(y0);

    const auto grayAt = [&](int px, int py) -> double {
        const size_t idx = (static_cast<size_t>(py) * static_cast<size_t>(mask_w) + static_cast<size_t>(px)) * 4;
        if (idx + 3 >= rgba.size()) {
            return 0.0;
        }
        const double r = static_cast<double>(rgba[idx + 0]);
        const double g = static_cast<double>(rgba[idx + 1]);
        const double b = static_cast<double>(rgba[idx + 2]);
        const double alpha = static_cast<double>(rgba[idx + 3]) / 255.0;
        return (0.299 * r + 0.587 * g + 0.114 * b) * alpha;
    };

    const double g00 = grayAt(x0, y0);
    const double g10 = grayAt(x1, y0);
    const double g01 = grayAt(x0, y1);
    const double g11 = grayAt(x1, y1);
    const double g0 = g00 * (1.0 - tx) + g10 * tx;
    const double g1 = g01 * (1.0 - tx) + g11 * tx;
    return g0 * (1.0 - ty) + g1 * ty;
}

bool mapUvPointToSvgMaskNorm(double uv_u,
                             double uv_v,
                             const ApplicationController::PatternPlanOptions& options,
                             double patch_u_min,
                             double patch_u_max,
                             double patch_v_min,
                             double patch_v_max,
                             int mask_w,
                             int mask_h,
                             double& u_norm,
                             double& v_norm)
{
    if (mask_w <= 1 || mask_h <= 1) {
        return false;
    }

    const double u_range = patch_u_max - patch_u_min;
    const double v_range = patch_v_max - patch_v_min;
    if (!(u_range > 1e-9) || !(v_range > 1e-9)) {
        return false;
    }

    SvgAspectFitFrame fit_frame;
    const double mask_aspect_wh = static_cast<double>(mask_w) / static_cast<double>(mask_h);
    if (!computeSvgAspectFitFrame(patch_u_min, patch_u_max, patch_v_min, patch_v_max, mask_aspect_wh, fit_frame)) {
        return false;
    }

    const double center_u = (patch_u_min + patch_u_max) * 0.5;
    const double center_v = (patch_v_min + patch_v_max) * 0.5;
    const double sx = (std::abs(options.tex_scale_x) < 1e-9) ? 1.0 : options.tex_scale_x;
    const double sy = (std::abs(options.tex_scale_y) < 1e-9) ? 1.0 : options.tex_scale_y;
    const double cos_r = std::cos(options.tex_rotation_deg * M_PI / 180.0);
    const double sin_r = std::sin(options.tex_rotation_deg * M_PI / 180.0);

    const double u_rel = uv_u - center_u;
    const double v_rel = uv_v - center_v;
    const double u_scaled = u_rel * sx;
    const double v_scaled = v_rel * sy;
    const double u_rot = u_scaled * cos_r - v_scaled * sin_r;
    const double v_rot = u_scaled * sin_r + v_scaled * cos_r;
    const double u_transformed = u_rot + options.tex_translate_x + center_u;
    const double v_transformed = v_rot + options.tex_translate_y + center_v;
    if (!std::isfinite(u_transformed) || !std::isfinite(v_transformed)) {
        return false;
    }

    u_norm = (u_transformed - fit_frame.u_min) / fit_frame.u_range;
    v_norm = (v_transformed - fit_frame.v_min) / fit_frame.v_range;
    if (!std::isfinite(u_norm) || !std::isfinite(v_norm)) {
        return false;
    }
    if (u_norm < 0.0 || u_norm > 1.0 || v_norm < 0.0 || v_norm > 1.0) {
        return false;
    }

    return true;
}

double sampleSvgPointGrayscale(double uv_u,
                               double uv_v,
                               const ApplicationController::PatternPlanOptions& options,
                               double patch_u_min,
                               double patch_u_max,
                               double patch_v_min,
                               double patch_v_max,
                               const std::vector<unsigned char>& rgba,
                               int mask_w,
                               int mask_h)
{
    double u_norm = 0.0;
    double v_norm = 0.0;
    if (!mapUvPointToSvgMaskNorm(uv_u, uv_v, options,
                                 patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                 mask_w, mask_h,
                                 u_norm, v_norm)) {
        return 0.0;
    }
    return std::clamp(sampleSvgMaskGrayBilinear(rgba, mask_w, mask_h, u_norm, v_norm) / 255.0, 0.0, 1.0);
}

template <typename PathPointT>
void applySvgGrayscaleToPath(std::vector<PathPointT>& path,
                             const ApplicationController::PatternPlanOptions& options,
                             double patch_u_min,
                             double patch_u_max,
                             double patch_v_min,
                             double patch_v_max,
                             const std::vector<unsigned char>& rgba,
                             int mask_w,
                             int mask_h)
{
    if (path.empty() || rgba.empty()) {
        return;
    }
    for (auto& pt : path) {
        pt.grayscale = sampleSvgPointGrayscale(pt.u, pt.v, options,
                                               patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                               rgba, mask_w, mask_h);
    }
}

bool isUvPointInsideSvgMask(const nbcam::UVPathPoint& uv_pt,
                            const ApplicationController::PatternPlanOptions& options,
                            double patch_u_min,
                            double patch_u_max,
                            double patch_v_min,
                            double patch_v_max,
                            const std::vector<unsigned char>& rgba,
                            int mask_w,
                            int mask_h)
{
    if (rgba.empty() || mask_w <= 1 || mask_h <= 1) {
        return false;
    }

    double u_norm = 0.0;
    double v_norm = 0.0;
    if (!mapUvPointToSvgMaskNorm(uv_pt.u, uv_pt.v, options,
                                 patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                 mask_w, mask_h,
                                 u_norm, v_norm)) {
        return false;
    }

    const double alpha = sampleSvgMaskAlphaBilinear(rgba, mask_w, mask_h, u_norm, v_norm);
    return alpha > 24.0;
}

void clipPathBySvgMask(std::vector<nbcam::UVPathPoint>& uv_path,
                       std::vector<bool>& break_flags,
                       const ApplicationController::PatternPlanOptions& options,
                       double patch_u_min,
                       double patch_u_max,
                       double patch_v_min,
                       double patch_v_max,
                       const std::vector<unsigned char>& rgba,
                       int mask_w,
                       int mask_h)
{
    if (uv_path.empty() || rgba.empty()) {
        return;
    }

    const double u_range = patch_u_max - patch_u_min;
    const double v_range = patch_v_max - patch_v_min;
    SvgAspectFitFrame fit_frame;
    const double mask_aspect_wh =
        (mask_h > 0) ? (static_cast<double>(mask_w) / static_cast<double>(mask_h)) : 1.0;
    if (u_range > 1e-9 && v_range > 1e-9 && mask_w > 1 && mask_h > 1 &&
        computeSvgAspectFitFrame(patch_u_min, patch_u_max, patch_v_min, patch_v_max, mask_aspect_wh, fit_frame)) {
        const double pixel_u = fit_frame.u_range / static_cast<double>(mask_w - 1);
        const double pixel_v = fit_frame.v_range / static_cast<double>(mask_h - 1);
        const double mask_clip_step = std::clamp(std::min(pixel_u, pixel_v) * 0.5, 1e-4, 0.1);
        uv_path = densifyUVPathSegments(uv_path, mask_clip_step);
        break_flags.resize(uv_path.size());
        for (size_t i = 0; i < uv_path.size(); ++i) {
            break_flags[i] = uv_path[i].is_jump_before;
        }
    }

    std::vector<nbcam::UVPathPoint> filtered_path;
    std::vector<bool> filtered_breaks;
    filtered_path.reserve(uv_path.size());
    filtered_breaks.reserve(uv_path.size());

    bool pending_break = false;
    for (size_t i = 0; i < uv_path.size(); ++i) {
        const bool keep = isUvPointInsideSvgMask(uv_path[i], options,
                                                 patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                                 rgba, mask_w, mask_h);
        if (!keep) {
            pending_break = true;
            continue;
        }

        bool break_before = pending_break;
        if (i < break_flags.size() && break_flags[i]) {
            break_before = true;
        }
        nbcam::UVPathPoint pt = uv_path[i];
        pt.is_jump_before = break_before;
        filtered_path.push_back(pt);
        filtered_breaks.push_back(break_before);
        pending_break = false;
    }

    uv_path = std::move(filtered_path);
    break_flags = std::move(filtered_breaks);
    applySvgGrayscaleToPath(uv_path, options,
                            patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                            rgba, mask_w, mask_h);
}

double interpolatePercentile(const std::vector<double>& sorted_values, double p)
{
    if (sorted_values.empty()) {
        return 0.0;
    }
    if (sorted_values.size() == 1) {
        return sorted_values.front();
    }

    p = std::clamp(p, 0.0, 1.0);
    const double pos = p * static_cast<double>(sorted_values.size() - 1);
    const size_t lo = static_cast<size_t>(std::floor(pos));
    const size_t hi = static_cast<size_t>(std::ceil(pos));
    if (lo == hi) {
        return sorted_values[lo];
    }
    const double t = pos - static_cast<double>(lo);
    return sorted_values[lo] * (1.0 - t) + sorted_values[hi] * t;
}

bool solveLinear3x3(double m[3][4], double x[3])
{
    constexpr double kEps = 1e-12;
    for (int col = 0; col < 3; ++col) {
        int pivot = col;
        double best_abs = std::abs(m[col][col]);
        for (int r = col + 1; r < 3; ++r) {
            const double v = std::abs(m[r][col]);
            if (v > best_abs) {
                best_abs = v;
                pivot = r;
            }
        }
        if (best_abs <= kEps) {
            return false;
        }
        if (pivot != col) {
            for (int c = col; c < 4; ++c) {
                std::swap(m[col][c], m[pivot][c]);
            }
        }

        const double div = m[col][col];
        for (int c = col; c < 4; ++c) {
            m[col][c] /= div;
        }

        for (int r = 0; r < 3; ++r) {
            if (r == col) {
                continue;
            }
            const double factor = m[r][col];
            if (std::abs(factor) <= kEps) {
                continue;
            }
            for (int c = col; c < 4; ++c) {
                m[r][c] -= factor * m[col][c];
            }
        }
    }

    x[0] = m[0][3];
    x[1] = m[1][3];
    x[2] = m[2][3];
    return std::isfinite(x[0]) && std::isfinite(x[1]) && std::isfinite(x[2]);
}

bool estimateArcCenterAndRadiusFromPatch(const nbcam::Patch& patch,
                                         const nbcam::TriangleMesh& mesh,
                                         bool row_on_z_axis,
                                         double& out_center_x,
                                         double& out_center_axis2,
                                         double& out_radius)
{
    std::unordered_set<size_t> unique_vertices;
    unique_vertices.reserve(patch.triangle_indices.size() * 3);
    for (size_t tri_idx : patch.triangle_indices) {
        if (tri_idx >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[tri_idx];
        unique_vertices.insert(tri.v0);
        unique_vertices.insert(tri.v1);
        unique_vertices.insert(tri.v2);
    }

    struct XY {
        double x = 0.0;
        double y = 0.0;
    };
    std::vector<XY> points;
    points.reserve(unique_vertices.size());
    for (size_t idx : unique_vertices) {
        if (idx >= mesh.vertices.size()) {
            continue;
        }
        const auto& v = mesh.vertices[idx];
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) {
            continue;
        }
        XY p;
        p.x = v.x;
        p.y = row_on_z_axis ? v.y : v.z;
        points.push_back(p);
    }
    if (points.size() < 6) {
        return false;
    }

    double s_xx = 0.0, s_xy = 0.0, s_yy = 0.0;
    double s_x = 0.0, s_y = 0.0;
    double b_x = 0.0, b_y = 0.0, b_1 = 0.0;
    for (const auto& p : points) {
        const double r2 = p.x * p.x + p.y * p.y;
        s_xx += p.x * p.x;
        s_xy += p.x * p.y;
        s_yy += p.y * p.y;
        s_x += p.x;
        s_y += p.y;
        b_x += -p.x * r2;
        b_y += -p.y * r2;
        b_1 += -r2;
    }

    double mat[3][4] = {
        {s_xx, s_xy, s_x, b_x},
        {s_xy, s_yy, s_y, b_y},
        {s_x,  s_y,  static_cast<double>(points.size()), b_1}
    };
    double sol[3] = {0.0, 0.0, 0.0};  // D, E, F
    if (!solveLinear3x3(mat, sol)) {
        return false;
    }

    const double center_x = -0.5 * sol[0];
    const double center_axis2 = -0.5 * sol[1];
    const double radius_sq = center_x * center_x + center_axis2 * center_axis2 - sol[2];
    if (!(std::isfinite(center_x) && std::isfinite(center_axis2) && radius_sq > 1e-9)) {
        return false;
    }

    std::vector<double> distances;
    distances.reserve(points.size());
    for (const auto& p : points) {
        distances.push_back(std::hypot(p.x - center_x, p.y - center_axis2));
    }
    std::sort(distances.begin(), distances.end());
    const double radius_med = interpolatePercentile(distances, 0.5);
    if (!(std::isfinite(radius_med) && radius_med > 1e-9)) {
        return false;
    }

    out_center_x = center_x;
    out_center_axis2 = center_axis2;
    out_radius = radius_med;
    return true;
}

bool computePatchArcThetaRowBounds(const nbcam::Patch& patch,
                                   const nbcam::TriangleMesh& mesh,
                                   double center_x,
                                   double center_axis2,
                                   bool row_on_z_axis,
                                   double& theta_min,
                                   double& theta_max,
                                   double& row_min,
                                   double& row_max)
{
    std::unordered_set<size_t> unique_vertices;
    unique_vertices.reserve(patch.triangle_indices.size() * 3);
    for (size_t tri_idx : patch.triangle_indices) {
        if (tri_idx >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[tri_idx];
        unique_vertices.insert(tri.v0);
        unique_vertices.insert(tri.v1);
        unique_vertices.insert(tri.v2);
    }

    std::vector<double> theta_samples;
    std::vector<double> row_samples;
    theta_samples.reserve(unique_vertices.size());
    row_samples.reserve(unique_vertices.size());

    for (size_t idx : unique_vertices) {
        if (idx >= mesh.vertices.size()) {
            continue;
        }
        const auto& v = mesh.vertices[idx];
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) {
            continue;
        }
        if (row_on_z_axis) {
            theta_samples.push_back(std::atan2(v.x - center_x, v.y - center_axis2));
            row_samples.push_back(v.z);
        } else {
            theta_samples.push_back(std::atan2(v.x - center_x, v.z - center_axis2));
            row_samples.push_back(v.y);
        }
    }

    if (theta_samples.size() < 3 || row_samples.size() < 3) {
        return false;
    }

    std::sort(row_samples.begin(), row_samples.end());
    row_min = interpolatePercentile(row_samples, 0.01);
    row_max = interpolatePercentile(row_samples, 0.99);
    if (!(std::isfinite(row_min) && std::isfinite(row_max)) || row_max <= row_min + 1e-9) {
        row_min = row_samples.front();
        row_max = row_samples.back();
    }

    auto unwrapTheta = [](std::vector<double> values, bool shift_negative) {
        if (shift_negative) {
            for (double& t : values) {
                if (t < 0.0) {
                    t += 2.0 * M_PI;
                }
            }
        }
        std::sort(values.begin(), values.end());
        return values;
    };

    auto theta_raw = unwrapTheta(theta_samples, false);
    auto theta_shifted = unwrapTheta(theta_samples, true);
    const double span_raw = theta_raw.back() - theta_raw.front();
    const double span_shifted = theta_shifted.back() - theta_shifted.front();
    const std::vector<double>& theta_used = (span_shifted < span_raw) ? theta_shifted : theta_raw;

    theta_min = interpolatePercentile(theta_used, 0.01);
    theta_max = interpolatePercentile(theta_used, 0.99);
    if (!(std::isfinite(theta_min) && std::isfinite(theta_max)) || theta_max <= theta_min + 1e-9) {
        theta_min = theta_used.front();
        theta_max = theta_used.back();
    }

    return (theta_max > theta_min + 1e-6) && (row_max > row_min + 1e-6);
}

bool generateArcHatchPathFromSvgMask(const nbcam::Patch& patch,
                                     const nbcam::TriangleMesh& mesh,
                                     const ApplicationController::PatternPlanOptions& options,
                                     const std::vector<unsigned char>& rgba,
                                     int mask_w,
                                     int mask_h,
                                     std::vector<nbcam::UVPathPoint>& uv_path_out,
                                     std::vector<nbcam::PathPoint>& xyz_path_out,
                                     std::vector<bool>& break_flags_out,
                                     double* out_center_x = nullptr,
                                     double* out_center_axis2 = nullptr,
                                     double* out_radius = nullptr)
{
    uv_path_out.clear();
    xyz_path_out.clear();
    break_flags_out.clear();

    if (rgba.empty() || mask_w <= 1 || mask_h <= 1) {
        return false;
    }
    if (!(options.spacing > 1e-9)) {
        return false;
    }

    const bool row_on_z_axis = (std::abs(patch.normal_z) > std::abs(patch.normal_y));
    double center_x = options.arc_center_x;
    double center_axis2 = options.arc_center_z;
    double radius = options.arc_radius;

    double fit_center_x = 0.0;
    double fit_center_axis2 = 0.0;
    double fit_radius = 0.0;
    const bool fit_ok = estimateArcCenterAndRadiusFromPatch(patch, mesh, row_on_z_axis,
                                                            fit_center_x, fit_center_axis2, fit_radius);
    if (fit_ok) {
        const bool input_invalid = !(std::isfinite(center_x) && std::isfinite(center_axis2) &&
                                     std::isfinite(radius) && radius > 1e-9);
        const double center_delta = std::hypot(center_x - fit_center_x, center_axis2 - fit_center_axis2);
        const bool center_too_far = center_delta > std::max(2.0, fit_radius * 0.25);
        if (input_invalid || center_too_far) {
            center_x = fit_center_x;
            center_axis2 = fit_center_axis2;
        }
        if (!(radius > 1e-9) || std::abs(radius - fit_radius) > std::max(1.0, fit_radius * 0.2)) {
            radius = fit_radius;
        }
    }
    if (!(std::isfinite(center_x) && std::isfinite(center_axis2) && radius > 1e-9)) {
        return false;
    }
    if (out_center_x) {
        *out_center_x = center_x;
    }
    if (out_center_axis2) {
        *out_center_axis2 = center_axis2;
    }
    if (out_radius) {
        *out_radius = radius;
    }

    double theta_min = 0.0;
    double theta_max = 0.0;
    double row_min = 0.0;
    double row_max = 0.0;
    if (!computePatchArcThetaRowBounds(patch,
                                       mesh,
                                       center_x,
                                       center_axis2,
                                       row_on_z_axis,
                                       theta_min,
                                       theta_max,
                                       row_min,
                                       row_max)) {
        return false;
    }

    const double theta_range = theta_max - theta_min;
    const double row_range = row_max - row_min;
    if (!(theta_range > 1e-9) || !(row_range > 1e-9)) {
        return false;
    }

    int row_count = static_cast<int>(std::floor(row_range / options.spacing)) + 1;
    row_count = std::max(1, std::min(row_count, 200000));

    const double arc_len_full = std::abs(theta_range) * radius;
    const double mask_sample_step_mm = std::clamp(options.spacing * 0.5, 0.002, 0.05);
    int theta_samples = static_cast<int>(std::ceil(arc_len_full / mask_sample_step_mm));
    theta_samples = std::max(256, std::min(theta_samples, 20000));
    const double theta_sample_eps = std::abs(theta_range) / static_cast<double>(theta_samples);

    bool serpentine_forward = true;
    for (int row = 0; row < row_count; ++row) {
        const double row_value = std::min(row_min + static_cast<double>(row) * options.spacing, row_max);
        const double v_norm = std::clamp((row_value - row_min) / row_range, 0.0, 1.0);

        std::vector<std::pair<double, double>> intervals;
        intervals.reserve(64);
        bool in_fill = false;
        double seg_start_theta = theta_min;
        double prev_theta = theta_min;

        for (int i = 0; i <= theta_samples; ++i) {
            const double u_norm = static_cast<double>(i) / static_cast<double>(theta_samples);
            const double theta = theta_min + theta_range * u_norm;
            nbcam::UVPathPoint uv_norm_point;
            uv_norm_point.u = u_norm;
            uv_norm_point.v = v_norm;
            const bool inside = isUvPointInsideSvgMask(uv_norm_point, options,
                                                       0.0, 1.0, 0.0, 1.0,
                                                       rgba, mask_w, mask_h);

            if (inside && !in_fill) {
                seg_start_theta = theta;
                in_fill = true;
            }

            if ((!inside || i == theta_samples) && in_fill) {
                const double seg_end_theta = (inside && i == theta_samples) ? theta : prev_theta;
                if (std::abs(seg_end_theta - seg_start_theta) >= theta_sample_eps * 0.5) {
                    intervals.emplace_back(seg_start_theta, seg_end_theta);
                }
                in_fill = false;
            }
            prev_theta = theta;
        }

        if (intervals.empty()) {
            serpentine_forward = !serpentine_forward;
            continue;
        }

        if (!serpentine_forward) {
            std::reverse(intervals.begin(), intervals.end());
        }

        for (const auto& interval : intervals) {
            double theta_start = serpentine_forward ? interval.first : interval.second;
            double theta_end = serpentine_forward ? interval.second : interval.first;
            const double segment_arc_len = std::abs(theta_end - theta_start) * radius;
            const double speed_mm_s = (std::isfinite(options.scan_speed_mm_s) && options.scan_speed_mm_s > 1e-6)
                                          ? options.scan_speed_mm_s
                                          : 300.0;
            int segment_samples = static_cast<int>(segment_arc_len / (speed_mm_s * 0.00001)) + 1;
            segment_samples = std::max(2, std::min(segment_samples, 50000));

            for (int s = 0; s < segment_samples; ++s) {
                const double t = (segment_samples <= 1)
                                     ? 0.0
                                     : static_cast<double>(s) / static_cast<double>(segment_samples - 1);
                const double theta = theta_start + (theta_end - theta_start) * t;
                const double u_norm = std::clamp((theta - theta_min) / theta_range, 0.0, 1.0);
                const double grayscale = std::clamp(sampleSvgMaskGrayBilinear(rgba, mask_w, mask_h, u_norm, v_norm) / 255.0,
                                                    0.0, 1.0);

                nbcam::PathPoint p;
                p.u = u_norm;
                p.v = v_norm;
                p.x = center_x + radius * std::sin(theta);
                if (row_on_z_axis) {
                    p.y = center_axis2 + radius * std::cos(theta);
                    p.z = row_value;
                } else {
                    p.y = row_value;
                    p.z = center_axis2 + radius * std::cos(theta);
                }
                p.laser = 1;
                p.grayscale = grayscale;

                const bool break_before = (s == 0) && !xyz_path_out.empty();
                xyz_path_out.push_back(std::move(p));
                break_flags_out.push_back(break_before);

                nbcam::UVPathPoint uvp;
                uvp.u = u_norm;
                uvp.v = v_norm;
                uvp.grayscale = grayscale;
                uvp.is_jump_before = break_before;
                uvp.is_arrow_tip = (s == segment_samples - 1);
                uv_path_out.push_back(uvp);
            }
        }

        serpentine_forward = !serpentine_forward;
    }

    return !xyz_path_out.empty();
}

struct LayerPathPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double u = 0.0;
    double v = 0.0;
};

LayerPathPoint lerpLayerPoint(const LayerPathPoint& a, const LayerPathPoint& b, double t)
{
    LayerPathPoint p;
    const double one_minus_t = 1.0 - t;
    p.x = a.x * one_minus_t + b.x * t;
    p.y = a.y * one_minus_t + b.y * t;
    p.z = a.z * one_minus_t + b.z * t;
    p.u = a.u * one_minus_t + b.u * t;
    p.v = a.v * one_minus_t + b.v * t;
    return p;
}

LayerPathPoint quadraticBezierLayerPoint(const LayerPathPoint& p0,
                                         const LayerPathPoint& p1,
                                         const LayerPathPoint& p2,
                                         double t)
{
    const double omt = 1.0 - t;
    const double b0 = omt * omt;
    const double b1 = 2.0 * omt * t;
    const double b2 = t * t;

    LayerPathPoint p;
    p.x = p0.x * b0 + p1.x * b1 + p2.x * b2;
    p.y = p0.y * b0 + p1.y * b1 + p2.y * b2;
    p.z = p0.z * b0 + p1.z * b1 + p2.z * b2;
    p.u = p0.u * b0 + p1.u * b1 + p2.u * b2;
    p.v = p0.v * b0 + p1.v * b1 + p2.v * b2;
    return p;
}

double xyDistanceSq(const LayerPathPoint& a, const LayerPathPoint& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

void appendUniqueLayerPoint(std::vector<LayerPathPoint>& chain, const LayerPathPoint& p, double eps_sq)
{
    if (chain.empty() || xyDistanceSq(chain.back(), p) > eps_sq) {
        chain.push_back(p);
    }
}

std::vector<LayerPathPoint> smoothLayerChainBezier(const std::vector<LayerPathPoint>& chain)
{
    if (chain.size() < 3) {
        return chain;
    }

    std::vector<LayerPathPoint> smoothed;
    smoothed.reserve(chain.size() * 2);
    constexpr double kMergeEpsSq = 1e-10;
    smoothed.push_back(chain.front());

    for (size_t i = 1; i + 1 < chain.size(); ++i) {
        const auto& prev = chain[i - 1];
        const auto& curr = chain[i];
        const auto& next = chain[i + 1];

        const double v1x = curr.x - prev.x;
        const double v1y = curr.y - prev.y;
        const double v2x = next.x - curr.x;
        const double v2y = next.y - curr.y;
        const double l1 = std::sqrt(v1x * v1x + v1y * v1y);
        const double l2 = std::sqrt(v2x * v2x + v2y * v2y);
        if (l1 <= 1e-8 || l2 <= 1e-8) {
            appendUniqueLayerPoint(smoothed, curr, kMergeEpsSq);
            continue;
        }

        const double d1x = v1x / l1;
        const double d1y = v1y / l1;
        const double d2x = v2x / l2;
        const double d2y = v2y / l2;
        const double dot = std::clamp(d1x * d2x + d1y * d2y, -1.0, 1.0);
        if (dot > 0.995) {
            appendUniqueLayerPoint(smoothed, curr, kMergeEpsSq);
            continue;
        }

        const double cut = std::min(l1, l2) * 0.28;
        if (cut <= 1e-5) {
            appendUniqueLayerPoint(smoothed, curr, kMergeEpsSq);
            continue;
        }

        const double t_prev = std::clamp((l1 - cut) / l1, 0.0, 1.0);
        const double t_next = std::clamp(cut / l2, 0.0, 1.0);
        const LayerPathPoint entry = lerpLayerPoint(prev, curr, t_prev);
        const LayerPathPoint exit = lerpLayerPoint(curr, next, t_next);

        appendUniqueLayerPoint(smoothed, entry, kMergeEpsSq);
        const int samples = std::clamp(static_cast<int>(std::round((1.0 - dot) * 8.0)), 4, 12);
        for (int s = 1; s < samples; ++s) {
            const double t = static_cast<double>(s) / static_cast<double>(samples);
            appendUniqueLayerPoint(smoothed, quadraticBezierLayerPoint(entry, curr, exit, t), kMergeEpsSq);
        }
        appendUniqueLayerPoint(smoothed, exit, kMergeEpsSq);
    }

    appendUniqueLayerPoint(smoothed, chain.back(), kMergeEpsSq);
    return smoothed;
}

bool intersectEdgeWithZPlane(const nbcam::Vertex& a,
                             const nbcam::Vertex& b,
                             const nbcam::UVCoord& uv_a,
                             const nbcam::UVCoord& uv_b,
                             double z_plane,
                             LayerPathPoint& out_pt)
{
    constexpr double kEps = 1e-9;
    const double da = a.z - z_plane;
    const double db = b.z - z_plane;

    if ((std::abs(da) <= kEps && std::abs(db) <= kEps) || (da > 0.0 && db > 0.0) || (da < 0.0 && db < 0.0)) {
        return false;
    }

    const double denom = da - db;
    if (std::abs(denom) <= kEps) {
        return false;
    }

    const double t = std::clamp(da / denom, 0.0, 1.0);
    out_pt.x = a.x + (b.x - a.x) * t;
    out_pt.y = a.y + (b.y - a.y) * t;
    out_pt.z = z_plane;
    out_pt.u = uv_a.u + (uv_b.u - uv_a.u) * t;
    out_pt.v = uv_a.v + (uv_b.v - uv_a.v) * t;
    return std::isfinite(out_pt.x) && std::isfinite(out_pt.y) &&
           std::isfinite(out_pt.u) && std::isfinite(out_pt.v);
}

struct QuantizedXYKey {
    long long x = 0;
    long long y = 0;

    bool operator==(const QuantizedXYKey& other) const
    {
        return x == other.x && y == other.y;
    }
};

struct QuantizedXYKeyHash {
    size_t operator()(const QuantizedXYKey& key) const noexcept
    {
        const auto hx = std::hash<long long>{}(key.x);
        const auto hy = std::hash<long long>{}(key.y);
        return hx ^ (hy << 1);
    }
};

std::vector<std::vector<LayerPathPoint>> chainLayerSegments(
    const std::vector<std::pair<LayerPathPoint, LayerPathPoint>>& segments,
    double merge_tol)
{
    std::vector<std::vector<LayerPathPoint>> chains;
    if (segments.empty() || merge_tol <= 1e-12) {
        return chains;
    }

    std::unordered_map<QuantizedXYKey, int, QuantizedXYKeyHash> node_lookup;
    std::vector<LayerPathPoint> nodes;
    struct Edge {
        int a = -1;
        int b = -1;
    };
    std::vector<Edge> edges;
    edges.reserve(segments.size());

    const auto nodeIdForPoint = [&](const LayerPathPoint& p) {
        const QuantizedXYKey key{
            static_cast<long long>(std::llround(p.x / merge_tol)),
            static_cast<long long>(std::llround(p.y / merge_tol))
        };
        auto it = node_lookup.find(key);
        if (it != node_lookup.end()) {
            return it->second;
        }
        const int id = static_cast<int>(nodes.size());
        nodes.push_back(p);
        node_lookup.emplace(key, id);
        return id;
    };

    for (const auto& seg : segments) {
        const int a = nodeIdForPoint(seg.first);
        const int b = nodeIdForPoint(seg.second);
        if (a == b) {
            continue;
        }
        edges.push_back({a, b});
    }

    if (edges.empty()) {
        return chains;
    }

    std::vector<std::vector<int>> adjacency(nodes.size());
    for (int ei = 0; ei < static_cast<int>(edges.size()); ++ei) {
        adjacency[edges[ei].a].push_back(ei);
        adjacency[edges[ei].b].push_back(ei);
    }

    std::vector<bool> edge_used(edges.size(), false);

    const auto hasUnusedEdge = [&](int node_id) {
        for (int ei : adjacency[node_id]) {
            if (!edge_used[ei]) {
                return true;
            }
        }
        return false;
    };

    const auto consumeChainFrom = [&](int start_node) {
        std::vector<int> chain_node_ids;
        chain_node_ids.reserve(64);
        chain_node_ids.push_back(start_node);

        int current = start_node;
        int guard = 0;
        const int max_steps = static_cast<int>(edges.size()) + 4;
        while (guard++ < max_steps) {
            int next_edge = -1;
            for (int ei : adjacency[current]) {
                if (!edge_used[ei]) {
                    next_edge = ei;
                    break;
                }
            }
            if (next_edge < 0) {
                break;
            }

            edge_used[next_edge] = true;
            const Edge& e = edges[next_edge];
            const int next_node = (e.a == current) ? e.b : e.a;
            chain_node_ids.push_back(next_node);
            current = next_node;
        }

        std::vector<LayerPathPoint> chain;
        chain.reserve(chain_node_ids.size());
        for (int nid : chain_node_ids) {
            if (nid >= 0 && nid < static_cast<int>(nodes.size())) {
                chain.push_back(nodes[nid]);
            }
        }
        if (chain.size() >= 2) {
            chains.push_back(std::move(chain));
        }
    };

    for (int node_id = 0; node_id < static_cast<int>(nodes.size()); ++node_id) {
        if (adjacency[node_id].size() == 1 && hasUnusedEdge(node_id)) {
            consumeChainFrom(node_id);
        }
    }
    for (int node_id = 0; node_id < static_cast<int>(nodes.size()); ++node_id) {
        while (hasUnusedEdge(node_id)) {
            consumeChainFrom(node_id);
        }
    }

    return chains;
}

void clipLayerPathBySvgMask(std::vector<nbcam::UVPathPoint>& uv_path,
                            std::vector<nbcam::PathPoint>& xyz_path,
                            std::vector<bool>& break_flags,
                            const ApplicationController::PatternPlanOptions& options,
                            double patch_u_min,
                            double patch_u_max,
                            double patch_v_min,
                            double patch_v_max,
                            const std::vector<unsigned char>& rgba,
                            int mask_w,
                            int mask_h)
{
    if (uv_path.empty() || xyz_path.empty() || rgba.empty()) {
        return;
    }

    const size_t count = std::min(uv_path.size(), xyz_path.size());
    std::vector<nbcam::UVPathPoint> filtered_uv;
    std::vector<nbcam::PathPoint> filtered_xyz;
    std::vector<bool> filtered_breaks;
    filtered_uv.reserve(count);
    filtered_xyz.reserve(count);
    filtered_breaks.reserve(count);

    const auto clonePoint = [](const nbcam::PathPoint& src) {
        nbcam::PathPoint dst;
        dst.u = src.u;
        dst.v = src.v;
        dst.x = src.x;
        dst.y = src.y;
        dst.z = src.z;
        dst.a = src.a;
        dst.b = src.b;
        dst.laser = src.laser;
        dst.grayscale = src.grayscale;
        if (src.params_override) {
            dst.params_override = std::make_unique<nbcam::ProcessParams>(*src.params_override);
        }
        return dst;
    };

    bool pending_break = false;
    for (size_t i = 0; i < count; ++i) {
        const bool keep = isUvPointInsideSvgMask(uv_path[i], options,
                                                 patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                                 rgba, mask_w, mask_h);
        if (!keep) {
            pending_break = true;
            continue;
        }

        bool break_before = pending_break;
        if (i < break_flags.size() && break_flags[i]) {
            break_before = true;
        }
        nbcam::UVPathPoint uv = uv_path[i];
        uv.is_jump_before = break_before;
        filtered_uv.push_back(uv);
        filtered_xyz.push_back(clonePoint(xyz_path[i]));
        filtered_breaks.push_back(break_before);
        pending_break = false;
    }

    uv_path = std::move(filtered_uv);
    xyz_path = std::move(filtered_xyz);
    break_flags = std::move(filtered_breaks);
}

bool generateZLayerContourPath(const nbcam::Patch& patch,
                               const nbcam::TriangleMesh& mesh,
                               const std::vector<nbcam::UVCoord>& uv_coords,
                               const ApplicationController::PatternPlanOptions& options,
                               bool apply_svg_mask,
                               double patch_u_min,
                               double patch_u_max,
                               double patch_v_min,
                               double patch_v_max,
                               const std::vector<unsigned char>& rgba_mask,
                               int mask_w,
                               int mask_h,
                               std::vector<nbcam::UVPathPoint>& uv_path_out,
                               std::vector<nbcam::PathPoint>& xyz_path_out,
                               std::vector<bool>& break_flags_out)
{
    uv_path_out.clear();
    xyz_path_out.clear();
    break_flags_out.clear();

    const double layer_height = (options.layer_height > 1e-9) ? options.layer_height
                                                               : std::max(0.05, options.spacing);
    if (!(layer_height > 1e-9)) {
        return false;
    }

    double z_min = std::numeric_limits<double>::max();
    double z_max = std::numeric_limits<double>::lowest();
    bool has_z = false;
    for (size_t tri_idx : patch.triangle_indices) {
        if (tri_idx >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[tri_idx];
        for (size_t vi : {tri.v0, tri.v1, tri.v2}) {
            if (vi >= mesh.vertices.size()) {
                continue;
            }
            const double z = mesh.vertices[vi].z;
            if (!std::isfinite(z)) {
                continue;
            }
            if (!has_z) {
                z_min = z_max = z;
                has_z = true;
            } else {
                z_min = std::min(z_min, z);
                z_max = std::max(z_max, z);
            }
        }
    }
    if (!has_z) {
        return false;
    }

    std::vector<double> layer_levels;
    if (z_max - z_min <= layer_height * 0.5) {
        layer_levels.push_back((z_min + z_max) * 0.5);
    } else {
        const int layer_count = std::max(1, static_cast<int>(std::floor((z_max - z_min) / layer_height)) + 1);
        layer_levels.reserve(static_cast<size_t>(layer_count) + 1);
        for (int li = 0; li < layer_count; ++li) {
            layer_levels.push_back(z_min + static_cast<double>(li) * layer_height);
        }
        if (layer_levels.empty() || std::abs(layer_levels.back() - z_max) > layer_height * 0.35) {
            layer_levels.push_back(z_max);
        }
    }

    const double merge_tol = std::max(1e-4, layer_height * 0.02);

    for (size_t li = 0; li < layer_levels.size(); ++li) {
        const double z_plane = layer_levels[li];
        std::vector<std::pair<LayerPathPoint, LayerPathPoint>> segments;
        segments.reserve(patch.triangle_indices.size());

        for (size_t tri_idx : patch.triangle_indices) {
            if (tri_idx >= mesh.triangles.size()) {
                continue;
            }
            const auto& tri = mesh.triangles[tri_idx];
            if (tri.v0 >= mesh.vertices.size() || tri.v1 >= mesh.vertices.size() || tri.v2 >= mesh.vertices.size() ||
                tri.v0 >= uv_coords.size() || tri.v1 >= uv_coords.size() || tri.v2 >= uv_coords.size()) {
                continue;
            }

            const auto& p0 = mesh.vertices[tri.v0];
            const auto& p1 = mesh.vertices[tri.v1];
            const auto& p2 = mesh.vertices[tri.v2];
            const auto& uv0 = uv_coords[tri.v0];
            const auto& uv1 = uv_coords[tri.v1];
            const auto& uv2 = uv_coords[tri.v2];
            if (!isFiniteUV(uv0) || !isFiniteUV(uv1) || !isFiniteUV(uv2)) {
                continue;
            }

            std::vector<LayerPathPoint> intersections;
            intersections.reserve(3);
            LayerPathPoint ip;
            if (intersectEdgeWithZPlane(p0, p1, uv0, uv1, z_plane, ip)) {
                intersections.push_back(ip);
            }
            if (intersectEdgeWithZPlane(p1, p2, uv1, uv2, z_plane, ip)) {
                bool duplicated = false;
                for (const auto& existing : intersections) {
                    if (xyDistanceSq(existing, ip) <= merge_tol * merge_tol) {
                        duplicated = true;
                        break;
                    }
                }
                if (!duplicated) {
                    intersections.push_back(ip);
                }
            }
            if (intersectEdgeWithZPlane(p2, p0, uv2, uv0, z_plane, ip)) {
                bool duplicated = false;
                for (const auto& existing : intersections) {
                    if (xyDistanceSq(existing, ip) <= merge_tol * merge_tol) {
                        duplicated = true;
                        break;
                    }
                }
                if (!duplicated) {
                    intersections.push_back(ip);
                }
            }

            if (intersections.size() == 2) {
                segments.emplace_back(intersections[0], intersections[1]);
            }
        }

        auto chains = chainLayerSegments(segments, merge_tol);
        if (chains.empty()) {
            continue;
        }

        std::sort(chains.begin(), chains.end(), [](const std::vector<LayerPathPoint>& a,
                                                   const std::vector<LayerPathPoint>& b) {
            auto avg_x = [](const std::vector<LayerPathPoint>& chain) {
                double sum = 0.0;
                for (const auto& p : chain) {
                    sum += p.x;
                }
                return chain.empty() ? 0.0 : sum / static_cast<double>(chain.size());
            };
            return avg_x(a) < avg_x(b);
        });

        if (li % 2 == 1) {
            std::reverse(chains.begin(), chains.end());
            for (auto& chain : chains) {
                std::reverse(chain.begin(), chain.end());
            }
        }

        for (const auto& chain_raw : chains) {
            if (chain_raw.size() < 2) {
                continue;
            }
            const auto chain = smoothLayerChainBezier(chain_raw);
            if (chain.size() < 2) {
                continue;
            }

            for (size_t i = 0; i < chain.size(); ++i) {
                bool break_before = (i == 0);
                nbcam::UVPathPoint uv;
                uv.u = chain[i].u;
                uv.v = chain[i].v;
                uv.is_jump_before = break_before;
                uv.is_arrow_tip = false;
                uv_path_out.push_back(uv);

                nbcam::PathPoint xyz;
                xyz.u = chain[i].u;
                xyz.v = chain[i].v;
                xyz.x = chain[i].x;
                xyz.y = chain[i].y;
                xyz.z = chain[i].z;
                xyz.laser = 1;
                xyz_path_out.push_back(std::move(xyz));
                break_flags_out.push_back(break_before);
            }
        }
    }

    if (apply_svg_mask) {
        clipLayerPathBySvgMask(uv_path_out, xyz_path_out, break_flags_out,
                               options, patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                               rgba_mask, mask_w, mask_h);
        applySvgGrayscaleToPath(uv_path_out, options,
                                patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                rgba_mask, mask_w, mask_h);
        applySvgGrayscaleToPath(xyz_path_out, options,
                                patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                rgba_mask, mask_w, mask_h);
    }

    return !uv_path_out.empty() && !xyz_path_out.empty();
}

nbcam::PathPoint clonePathPoint(const nbcam::PathPoint& src)
{
    nbcam::PathPoint dst;
    dst.u = src.u;
    dst.v = src.v;
    dst.x = src.x;
    dst.y = src.y;
    dst.z = src.z;
    dst.a = src.a;
    dst.b = src.b;
    dst.laser = src.laser;
    dst.grayscale = src.grayscale;
    if (src.params_override) {
        dst.params_override = std::make_unique<nbcam::ProcessParams>(*src.params_override);
    }
    return dst;
}

void applyZLayerOrdering(std::vector<nbcam::PathPoint>& xyz_path,
                         std::vector<bool>& break_flags,
                         double layer_height)
{
    if (xyz_path.empty() || layer_height <= 1e-9) {
        return;
    }

    double z_min = xyz_path.front().z;
    double z_max = xyz_path.front().z;
    for (const auto& p : xyz_path) {
        if (!std::isfinite(p.z)) {
            continue;
        }
        z_min = std::min(z_min, p.z);
        z_max = std::max(z_max, p.z);
    }
    if (z_max - z_min <= layer_height) {
        return;
    }

    const int layer_count = std::max(1, static_cast<int>(std::floor((z_max - z_min) / layer_height)) + 1);
    std::vector<std::vector<size_t>> layer_indices(layer_count);
    for (size_t i = 0; i < xyz_path.size(); ++i) {
        const double z = xyz_path[i].z;
        int layer = static_cast<int>(std::floor((z - z_min) / layer_height));
        layer = std::max(0, std::min(layer, layer_count - 1));
        layer_indices[layer].push_back(i);
    }

    std::vector<nbcam::PathPoint> reordered;
    std::vector<bool> reordered_breaks;
    reordered.reserve(xyz_path.size());
    reordered_breaks.reserve(xyz_path.size());

    for (int layer = 0; layer < layer_count; ++layer) {
        bool first_in_layer = true;
        for (size_t idx : layer_indices[layer]) {
            reordered.push_back(clonePathPoint(xyz_path[idx]));
            const bool original_break = (idx < break_flags.size()) ? break_flags[idx] : false;
            reordered_breaks.push_back(first_in_layer || original_break);
            first_in_layer = false;
        }
    }

    xyz_path = std::move(reordered);
    break_flags = std::move(reordered_breaks);
}

nbcam::Vertex midpointVertex(const nbcam::Vertex& a, const nbcam::Vertex& b)
{
    nbcam::Vertex mid{};
    mid.x = (a.x + b.x) * 0.5;
    mid.y = (a.y + b.y) * 0.5;
    mid.z = (a.z + b.z) * 0.5;
    mid.nx = (a.nx + b.nx) * 0.5;
    mid.ny = (a.ny + b.ny) * 0.5;
    mid.nz = (a.nz + b.nz) * 0.5;
    const double nlen = std::sqrt(mid.nx * mid.nx + mid.ny * mid.ny + mid.nz * mid.nz);
    if (nlen > 1e-12) {
        mid.nx /= nlen;
        mid.ny /= nlen;
        mid.nz /= nlen;
    }
    return mid;
}

void subdivideMeshUniform(nbcam::TriangleMesh& mesh, int iterations)
{
    if (iterations <= 0 || mesh.triangles.empty() || mesh.vertices.empty()) {
        return;
    }

    for (int iter = 0; iter < iterations; ++iter) {
        std::unordered_map<std::uint64_t, size_t> edge_midpoint;
        std::vector<nbcam::Vertex> new_vertices = mesh.vertices;
        std::vector<nbcam::Triangle> new_triangles;
        new_triangles.reserve(mesh.triangles.size() * 4);

        const auto getMidpointIndex = [&](size_t a, size_t b) {
            const std::uint64_t key = edgeKey(a, b);
            auto it = edge_midpoint.find(key);
            if (it != edge_midpoint.end()) {
                return it->second;
            }
            const size_t idx = new_vertices.size();
            new_vertices.push_back(midpointVertex(new_vertices[a], new_vertices[b]));
            edge_midpoint[key] = idx;
            return idx;
        };

        for (const auto& tri : mesh.triangles) {
            if (tri.v0 >= new_vertices.size() || tri.v1 >= new_vertices.size() || tri.v2 >= new_vertices.size()) {
                continue;
            }
            const size_t m01 = getMidpointIndex(tri.v0, tri.v1);
            const size_t m12 = getMidpointIndex(tri.v1, tri.v2);
            const size_t m20 = getMidpointIndex(tri.v2, tri.v0);

            new_triangles.push_back({tri.v0, m01, m20});
            new_triangles.push_back({tri.v1, m12, m01});
            new_triangles.push_back({tri.v2, m20, m12});
            new_triangles.push_back({m01, m12, m20});
        }

        mesh.vertices = std::move(new_vertices);
        mesh.triangles = std::move(new_triangles);
    }
}

int decideSubdivisionIterations(const nbcam::TriangleMesh& mesh,
                               size_t target_triangles,
                               size_t max_triangles,
                               int max_iterations)
{
    if (mesh.triangles.empty()) {
        return 0;
    }
    target_triangles = std::max(target_triangles, mesh.triangles.size());
    max_triangles = std::max(max_triangles, mesh.triangles.size());
    max_iterations = std::max(0, max_iterations);

    size_t tri_count = mesh.triangles.size();
    int iterations = 0;
    while (tri_count < target_triangles &&
           tri_count * 4 <= max_triangles &&
           iterations < max_iterations) {
        tri_count *= 4;
        ++iterations;
    }
    return iterations;
}

}  // namespace

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent)
    , current_job_(std::make_unique<nbcam::LaserJob>())
    , parameterized_patch_id_(-1)
    , pattern_patch_id_(-1)
    , planned_spacing_hint_mm_(-1.0)
{
    current_job_->clear();
}

ApplicationController::~ApplicationController() = default;

void ApplicationController::resetProcessingStateKeepMeshInternal(bool emit_job_ready)
{
    uv_coords_.clear();
    uv_path_.clear();
    xyz_path_.clear();
    path_break_flags_.clear();
    svg_boundary_.clear();
    parameterized_patch_id_ = -1;
    pattern_patch_id_ = -1;
    fill_strategy_.reset();
    current_fill_strategy_ = nbcam::FillStrategy::HATCH;
    current_strategy_name_ = "line_hatch";
    arc_scan_speed_mm_s_ = -1.0;
    arc_path_prebaked_ = false;
    current_path_uses_grayscale_ = false;
    current_path_all_svg_boundary_ = false;
    pending_svg_boundary_xyz_loops_.clear();
    planned_spacing_hint_mm_ = -1.0;
    if (current_job_) {
        current_job_->clear();
    }
    if (emit_job_ready) {
        emit jobReady(false);
    }
}

bool ApplicationController::rebuildCurrentMeshFromSource(bool emit_model_loaded)
{
    if (!source_mesh_ || !source_mesh_->isValid()) {
        spdlog::error("RebuildCurrentMeshFromSource failed: source mesh is invalid");
        if (emit_model_loaded) {
            emit modelLoaded(false);
        }
        return false;
    }

    if (!current_mesh_) {
        current_mesh_ = std::make_unique<nbcam::TriangleMesh>();
    }
    *current_mesh_ = *source_mesh_;
    if (!current_mesh_->isValid()) {
        spdlog::error("RebuildCurrentMeshFromSource failed: current mesh copy is invalid");
        if (emit_model_loaded) {
            emit modelLoaded(false);
        }
        return false;
    }

    const double scale = model_transform_.uniform_scale;
    for (auto& vertex : current_mesh_->vertices) {
        vertex.x = vertex.x * scale + model_transform_.offset_x_mm;
        vertex.y = vertex.y * scale + model_transform_.offset_y_mm;
        vertex.z = vertex.z * scale + model_transform_.offset_z_mm;
    }

    nbcam::MeshPreprocessor preprocessor;
    preprocessor.unifyNormals(*current_mesh_);

    if (emit_model_loaded) {
        emit modelLoaded(true);
    }
    return true;
}

bool ApplicationController::loadModel(const std::string& filepath)
{
    try {
        source_mesh_ = mesh_io_.loadFromFile(filepath);
        if (!source_mesh_ || !source_mesh_->isValid()) {
            spdlog::error("LoadModel failed: invalid mesh");
            emit modelLoaded(false);
            return false;
        }

        nbcam::MeshPreprocessor preprocessor;
        preprocessor.scale(*source_mesh_, 1.0);
        preprocessor.unifyNormals(*source_mesh_);

        model_transform_ = ModelTransform{};
        resetProcessingStateKeepMeshInternal(true);
        if (!rebuildCurrentMeshFromSource(false)) {
            emit modelLoaded(false);
            return false;
        }

        std::array<double, 6> bounds{};
        if (getCurrentMeshBounds(bounds) &&
            !currentMeshFitsBounds(kDefaultMachineMinMm,
                                   kDefaultMachineMaxMm,
                                   kDefaultMachineMinMm,
                                   kDefaultMachineMaxMm,
                                   kDefaultMachineMinMm,
                                   kDefaultMachineMaxMm)) {
            spdlog::warn("LoadModel: model exceeds processing region {} with bounds {}",
                         describeBounds({kDefaultMachineMinMm, kDefaultMachineMaxMm,
                                         kDefaultMachineMinMm, kDefaultMachineMaxMm,
                                         kDefaultMachineMinMm, kDefaultMachineMaxMm}),
                         describeBounds(bounds));
        }

        spdlog::info("LoadModel done: vertices={}, triangles={}",
                     current_mesh_->getVertexCount(),
                     current_mesh_->getTriangleCount());
        emit modelLoaded(true);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("LoadModel exception: {}", e.what());
        emit modelLoaded(false);
        return false;
    }
}

void ApplicationController::clearModel()
{
    source_mesh_.reset();
    current_mesh_.reset();
    model_transform_ = ModelTransform{};
    resetProcessingStateKeepMeshInternal(true);
}

bool ApplicationController::adaptiveSubdivideCurrentMesh(size_t target_triangles,
                                                         size_t max_triangles,
                                                         int max_iterations,
                                                         int* out_applied_iterations)
{
    if (!current_mesh_ || !current_mesh_->isValid()) {
        spdlog::error("AdaptiveSubdivide: mesh is not loaded or invalid");
        return false;
    }

    target_triangles = std::max<size_t>(target_triangles, current_mesh_->triangles.size());
    max_triangles = std::min<size_t>(std::max<size_t>(max_triangles, current_mesh_->triangles.size()), 80000);
    max_iterations = std::max(0, max_iterations);

    const size_t tri_before = current_mesh_->triangles.size();
    const size_t vtx_before = current_mesh_->vertices.size();
    const int refine_iterations = decideSubdivisionIterations(*current_mesh_,
                                                              target_triangles,
                                                              max_triangles,
                                                              max_iterations);
    if (refine_iterations > 0) {
        subdivideMeshUniform(*current_mesh_, refine_iterations);
    }
    if (out_applied_iterations) {
        *out_applied_iterations = refine_iterations;
    }

    nbcam::MeshPreprocessor preprocessor;
    preprocessor.unifyNormals(*current_mesh_);

    source_mesh_ = std::make_unique<nbcam::TriangleMesh>(*current_mesh_);
    resetProcessingStateKeepMeshInternal(true);

    spdlog::info("AdaptiveSubdivide: iters={}, vertices {} -> {}, triangles {} -> {} (target={}, max=80000)",
                 refine_iterations,
                 vtx_before,
                 current_mesh_->vertices.size(),
                 tri_before,
                 current_mesh_->triangles.size(),
                 target_triangles);
    emit modelLoaded(true);
    emit jobReady(false);
    return true;
}

bool ApplicationController::saveCurrentMesh(const std::string& filepath)
{
    if (!current_mesh_ || !current_mesh_->isValid()) {
        spdlog::error("SaveCurrentMesh: mesh is not loaded or invalid");
        return false;
    }
    return mesh_io_.saveToFile(*current_mesh_, filepath);
}

void ApplicationController::clearParameterization()
{
    uv_coords_.clear();
    parameterized_patch_id_ = -1;
    pattern_patch_id_ = -1;
    planned_spacing_hint_mm_ = -1.0;
}

void ApplicationController::clearProcessingStateKeepMesh()
{
    resetProcessingStateKeepMeshInternal(true);
}

bool ApplicationController::setModelTransform(const ModelTransform& transform)
{
    if (!source_mesh_ || !source_mesh_->isValid()) {
        spdlog::warn("SetModelTransform ignored: source mesh is not ready");
        return false;
    }
    if (!std::isfinite(transform.uniform_scale) || transform.uniform_scale <= 0.0 ||
        !std::isfinite(transform.offset_x_mm) ||
        !std::isfinite(transform.offset_y_mm) ||
        !std::isfinite(transform.offset_z_mm)) {
        spdlog::warn("SetModelTransform rejected: invalid transform scale={}, offset=({}, {}, {})",
                     transform.uniform_scale,
                     transform.offset_x_mm,
                     transform.offset_y_mm,
                     transform.offset_z_mm);
        return false;
    }

    const ModelTransform previous = model_transform_;
    model_transform_ = transform;
    if (!rebuildCurrentMeshFromSource(false)) {
        model_transform_ = previous;
        rebuildCurrentMeshFromSource(false);
        return false;
    }

    const double old_scale = previous.uniform_scale;
    const double new_scale = model_transform_.uniform_scale;
    const double scale_ratio = new_scale / old_scale;
    const auto remap_point = [&previous, &scale_ratio, this](double& x, double& y, double& z) {
        x = (x - previous.offset_x_mm) * scale_ratio + model_transform_.offset_x_mm;
        y = (y - previous.offset_y_mm) * scale_ratio + model_transform_.offset_y_mm;
        z = (z - previous.offset_z_mm) * scale_ratio + model_transform_.offset_z_mm;
    };

    for (auto& point : xyz_path_) {
        remap_point(point.x, point.y, point.z);
    }
    if (current_job_) {
        for (auto& segment : current_job_->segments) {
            for (auto& point : segment.points) {
                remap_point(point.x, point.y, point.z);
            }
        }
    }

    emit modelLoaded(true);
    emit jobReady(current_job_ && current_job_->isValid());
    return true;
}

bool ApplicationController::autoCenterModelToBounds(double x_min, double x_max,
                                                    double y_min, double y_max,
                                                    double z_min, double z_max)
{
    if (!source_mesh_ || !source_mesh_->isValid()) {
        return false;
    }

    std::array<double, 6> bounds{};
    if (!getCurrentMeshBounds(bounds)) {
        return false;
    }

    ModelTransform next = model_transform_;
    next.offset_x_mm += ((x_min + x_max) * 0.5) - ((bounds[0] + bounds[1]) * 0.5);
    next.offset_y_mm += ((y_min + y_max) * 0.5) - ((bounds[2] + bounds[3]) * 0.5);
    next.offset_z_mm += ((z_min + z_max) * 0.5) - ((bounds[4] + bounds[5]) * 0.5);
    return setModelTransform(next);
}

bool ApplicationController::getCurrentMeshBounds(std::array<double, 6>& bounds) const
{
    if (!current_mesh_ || !current_mesh_->isValid()) {
        return false;
    }
    return computeMeshBounds(*current_mesh_, bounds);
}

bool ApplicationController::currentMeshFitsBounds(double x_min, double x_max,
                                                  double y_min, double y_max,
                                                  double z_min, double z_max,
                                                  std::string* out_message) const
{
    std::array<double, 6> bounds{};
    if (!getCurrentMeshBounds(bounds)) {
        if (out_message) {
            *out_message = "当前模型边界无效。";
        }
        return false;
    }

    const bool fits =
        bounds[0] >= x_min && bounds[1] <= x_max &&
        bounds[2] >= y_min && bounds[3] <= y_max &&
        bounds[4] >= z_min && bounds[5] <= z_max;
    if (!fits && out_message) {
        *out_message = fmt::format("当前模型超出加工区域。模型边界 {}，加工区域 {}。",
                                   describeBounds(bounds),
                                   describeBounds({x_min, x_max, y_min, y_max, z_min, z_max}));
    }
    return fits;
}

bool ApplicationController::parameterizeMesh(const std::string& algorithm)
{
    if (!current_mesh_) {
        spdlog::error("ParameterizeMesh: mesh is not loaded");
        emit parameterizationCompleted(false);
        return false;
    }

    try {
        if (algorithm == "LSCM") {
            parameterizer_ = std::make_unique<nbcam::LSCMParameterizer>();
        } else if (algorithm == "ARAP") {
            parameterizer_ = std::make_unique<nbcam::ARAPParameterizer>();
        } else if (algorithm == "AUTHALIC") {
            parameterizer_ = std::make_unique<nbcam::AuthalicParameterizer>();
        } else {
            spdlog::warn("ParameterizeMesh: unknown algorithm {}, fallback to LSCM", algorithm);
            parameterizer_ = std::make_unique<nbcam::LSCMParameterizer>();
        }

        const auto result = parameterizer_->parameterize(*current_mesh_);
        if (!result.success) {
            spdlog::error("ParameterizeMesh failed");
            emit parameterizationCompleted(false);
            return false;
        }

        uv_coords_ = result.uv_coords;
        parameterized_patch_id_ = -1;
        pattern_patch_id_ = -1;
        spdlog::info("ParameterizeMesh done: uv_count={}, islands={}", uv_coords_.size(), result.island_count);
        emit parameterizationCompleted(true);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("ParameterizeMesh exception: {}", e.what());
        emit parameterizationCompleted(false);
        return false;
    }
}

bool ApplicationController::generatePattern(const std::string& strategy, double spacing, double angle)
{
    if (uv_coords_.empty()) {
        spdlog::error("GeneratePattern: UV coordinates are empty");
        emit patternGenerated(false);
        return false;
    }

    try {
        const std::string normalized_strategy = normalizeStrategyName(strategy);
        fill_strategy_ = createFillStrategy(normalized_strategy);
        current_fill_strategy_ = toFillStrategyEnum(normalized_strategy);
        current_strategy_name_ = normalized_strategy;
        arc_scan_speed_mm_s_ = -1.0;
        arc_path_prebaked_ = false;
        current_path_uses_grayscale_ = false;
        current_path_all_svg_boundary_ = false;
        pending_svg_boundary_xyz_loops_.clear();

        std::vector<nbcam::UVCoord> boundary;
        if (!svg_boundary_.empty()) {
            boundary = svg_boundary_;
            spdlog::info("GeneratePattern: using SVG boundary, {} points", boundary.size());
        } else {
            boundary = buildConvexHullBoundary(uv_coords_);
            spdlog::info("GeneratePattern: using UV convex hull, {} points", boundary.size());
        }

        if (boundary.size() < 3) {
            spdlog::error("GeneratePattern: no valid boundary");
            emit patternGenerated(false);
            return false;
        }

        double effective_angle = angle;
        if (normalized_strategy == "ring_outin") {
            effective_angle = -1.0;
        }
        uv_path_ = fill_strategy_->generatePath(boundary, spacing, effective_angle);
        path_break_flags_.resize(uv_path_.size());
        for (size_t i = 0; i < uv_path_.size(); ++i) {
            path_break_flags_[i] = uv_path_[i].is_jump_before;
        }

        if (uv_path_.empty()) {
            spdlog::error("GeneratePattern: strategy produced empty path");
            emit patternGenerated(false);
            return false;
        }

        planned_spacing_hint_mm_ = spacing;
        pattern_patch_id_ = -1;
        spdlog::info("GeneratePattern done: strategy={}, points={}", normalized_strategy, uv_path_.size());
        emit patternGenerated(true);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("GeneratePattern exception: {}", e.what());
        emit patternGenerated(false);
        return false;
    }
}

bool ApplicationController::importSVG(const std::string& filepath, double center_u, double center_v, double scale)
{
    if (!current_mesh_) {
        spdlog::error("ImportSVG: mesh is not loaded");
        emit patternGenerated(false);
        return false;
    }

    try {
        nbcam::NanosvgWrapper parser;
        if (!parser.loadFromFile(filepath)) {
            spdlog::error("ImportSVG: failed to load {}", filepath);
            emit patternGenerated(false);
            return false;
        }

        const auto svg_points = parser.getPathPoints(0.01);
        if (svg_points.empty()) {
            spdlog::error("ImportSVG: SVG path is empty");
            emit patternGenerated(false);
            return false;
        }

        nbcam::SVGToUVMapper uv_mapper;
        uv_path_.clear();
        xyz_path_.clear();
        path_break_flags_.clear();
        pattern_patch_id_ = -1;
        current_fill_strategy_ = nbcam::FillStrategy::CONTOUR;
        current_strategy_name_ = "svg_import";
        arc_scan_speed_mm_s_ = -1.0;
        arc_path_prebaked_ = false;
        planned_spacing_hint_mm_ = -1.0;
        current_path_uses_grayscale_ = false;
        current_path_all_svg_boundary_ = false;
        pending_svg_boundary_xyz_loops_.clear();
        uv_mapper.mapToUV(svg_points, uv_path_, center_u, center_v, scale, true);
        current_path_uses_grayscale_ = !uv_path_.empty();

        svg_boundary_.clear();
        svg_boundary_.reserve(uv_path_.size());
        for (const auto& pt : uv_path_) {
            svg_boundary_.push_back({pt.u, pt.v});
        }

        path_break_flags_.assign(uv_path_.size(), false);
        for (size_t i = 0; i < svg_points.size() && i < path_break_flags_.size(); ++i) {
            if (svg_points[i].is_move_to) {
                path_break_flags_[i] = true;
            }
        }

        if (uv_path_.empty()) {
            spdlog::error("ImportSVG: UV path is empty after mapping");
            emit patternGenerated(false);
            return false;
        }

        emit patternGenerated(true);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("ImportSVG exception: {}", e.what());
        emit patternGenerated(false);
        return false;
    }
}

bool ApplicationController::importSVGWithTiling(const std::string& filepath,
                                                double tile_u,
                                                double tile_v,
                                                const std::vector<double>& uv_bounds)
{
    if (!current_mesh_) {
        spdlog::error("ImportSVGWithTiling: mesh is not loaded");
        emit patternGenerated(false);
        return false;
    }

    try {
        nbcam::NanosvgWrapper parser;
        if (!parser.loadFromFile(filepath)) {
            spdlog::error("ImportSVGWithTiling: failed to load {}", filepath);
            emit patternGenerated(false);
            return false;
        }

        const auto svg_points = parser.getPathPoints(0.01);
        if (svg_points.empty()) {
            spdlog::error("ImportSVGWithTiling: SVG path is empty");
            emit patternGenerated(false);
            return false;
        }

        std::vector<double> actual_uv_bounds = uv_bounds;
        if (actual_uv_bounds.size() < 4 && !uv_coords_.empty()) {
            const auto boundary = buildConvexHullBoundary(uv_coords_);
            if (!boundary.empty()) {
                double u_min = boundary[0].u;
                double u_max = boundary[0].u;
                double v_min = boundary[0].v;
                double v_max = boundary[0].v;
                for (const auto& uv : boundary) {
                    u_min = std::min(u_min, uv.u);
                    u_max = std::max(u_max, uv.u);
                    v_min = std::min(v_min, uv.v);
                    v_max = std::max(v_max, uv.v);
                }
                actual_uv_bounds = {u_min, v_min, u_max, v_max};
            }
        }

        nbcam::SVGToUVMapper uv_mapper;
        uv_path_.clear();
        xyz_path_.clear();
        path_break_flags_.clear();
        pattern_patch_id_ = -1;
        current_fill_strategy_ = nbcam::FillStrategy::CONTOUR;
        current_strategy_name_ = "svg_import";
        arc_scan_speed_mm_s_ = -1.0;
        arc_path_prebaked_ = false;
        planned_spacing_hint_mm_ = -1.0;
        current_path_uses_grayscale_ = false;
        uv_mapper.tileToUV(svg_points, uv_path_, tile_u, tile_v, actual_uv_bounds);
        current_path_uses_grayscale_ = !uv_path_.empty();

        svg_boundary_.clear();
        if (actual_uv_bounds.size() >= 4) {
            svg_boundary_ = {
                {actual_uv_bounds[0], actual_uv_bounds[1]},
                {actual_uv_bounds[2], actual_uv_bounds[1]},
                {actual_uv_bounds[2], actual_uv_bounds[3]},
                {actual_uv_bounds[0], actual_uv_bounds[3]},
            };
        }

        path_break_flags_.assign(uv_path_.size(), false);
        if (!svg_points.empty()) {
            const size_t points_per_tile = svg_points.size();
            for (size_t i = 0; i < uv_path_.size(); i += points_per_tile) {
                path_break_flags_[i] = true;
            }
            const size_t tile_count = uv_path_.size() / points_per_tile;
            for (size_t tile_idx = 0; tile_idx < tile_count; ++tile_idx) {
                const size_t base = tile_idx * points_per_tile;
                for (size_t i = 0; i < svg_points.size() && base + i < uv_path_.size(); ++i) {
                    if (svg_points[i].is_move_to) {
                        path_break_flags_[base + i] = true;
                    }
                }
            }
        }

        if (uv_path_.empty()) {
            spdlog::error("ImportSVGWithTiling: UV path is empty after tiling");
            emit patternGenerated(false);
            return false;
        }

        emit patternGenerated(true);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("ImportSVGWithTiling exception: {}", e.what());
        emit patternGenerated(false);
        return false;
    }
}

bool ApplicationController::mapToXYZ(const std::vector<size_t>* patch_triangle_indices)
{
    if (!current_mesh_ || uv_path_.empty() || uv_coords_.empty()) {
        spdlog::error("MapToXYZ: missing mesh/uv_path/uv_coords");
        emit pathMapped(false);
        return false;
    }

    try {
        auto mapped = uv_mapper_.mapUVToXYZ(uv_path_, *current_mesh_, uv_coords_, patch_triangle_indices);

        std::vector<nbcam::PathPoint> filtered_path;
        std::vector<bool> filtered_break_flags;
        filtered_path.reserve(mapped.size());
        filtered_break_flags.reserve(mapped.size());

        bool pending_break = false;
        for (size_t i = 0; i < mapped.size(); ++i) {
            auto& point = mapped[i];
            const bool valid = (point.laser != 0) &&
                               std::isfinite(point.x) &&
                               std::isfinite(point.y) &&
                               std::isfinite(point.z);
            if (!valid) {
                pending_break = true;
                continue;
            }

            bool break_before = pending_break;
            if (i < path_break_flags_.size() && path_break_flags_[i]) {
                break_before = true;
            }

            point.grayscale = std::clamp(uv_path_[i].grayscale, 0.0, 1.0);
            filtered_break_flags.push_back(break_before);
            filtered_path.push_back(std::move(point));
            pending_break = false;
        }

        if (filtered_path.empty()) {
            spdlog::error("MapToXYZ: no valid mapped points");
            emit pathMapped(false);
            return false;
        }

        xyz_path_ = std::move(filtered_path);
        path_break_flags_ = std::move(filtered_break_flags);

        spdlog::info("MapToXYZ done: {} valid points", xyz_path_.size());
        emit pathMapped(true);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("MapToXYZ exception: {}", e.what());
        emit pathMapped(false);
        return false;
    }
}

bool ApplicationController::assignProcessParams(const std::string& model_type)
{
    if (!current_mesh_ || xyz_path_.empty()) {
        spdlog::error("AssignProcessParams: missing mesh or xyz path");
        return false;
    }

    try {
        if (model_type == "curvature") {
            process_model_ = std::make_unique<nbcam::CurvatureModel>();
        } else {
            spdlog::warn("AssignProcessParams: unknown model {}, fallback to curvature", model_type);
            process_model_ = std::make_unique<nbcam::CurvatureModel>();
        }

        process_model_->assignProcessParams(*current_mesh_, xyz_path_);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("AssignProcessParams exception: {}", e.what());
        return false;
    }
}

void ApplicationController::setGrayscalePowerRange(double min_power_w, double max_power_w, double gamma)
{
    grayscale_power_min_w_ = min_power_w;
    grayscale_power_max_w_ = max_power_w;
    grayscale_gamma_ = (std::isfinite(gamma) && gamma > 1e-9) ? gamma : 1.0;
}

bool ApplicationController::planPath(bool apply_postprocess)
{
    if (xyz_path_.empty()) {
        spdlog::error("PlanPath: xyz_path is empty");
        emit jobReady(false);
        return false;
    }

    try {
        current_job_->clear();
        current_job_->meta.job_id = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss").toStdString();
        current_job_->meta.source_model = "model.obj";
        current_job_->meta.created_at = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
        current_job_->meta.units = "mm";

        current_job_->process_defaults.power_w = 20.0;
        current_job_->process_defaults.freq_hz = 20000;
        current_job_->process_defaults.speed_mm_s = 300.0;
        if (!xyz_path_.empty() && xyz_path_[0].params_override) {
            current_job_->process_defaults = *xyz_path_[0].params_override;
        }
        current_job_->grayscale_enabled = current_path_uses_grayscale_;
        current_job_->grayscale_power_min_w = grayscale_power_min_w_;
        current_job_->grayscale_power_max_w = grayscale_power_max_w_;
        current_job_->grayscale_gamma = grayscale_gamma_;
        current_job_->grayscale_source = "svg_luminance";
        if (current_job_->grayscale_enabled) {
            current_job_->process_defaults.power_w = std::min(grayscale_power_min_w_, grayscale_power_max_w_);
            current_job_->process_defaults.grayscale = 0.0;
        }

        constexpr double kJumpThreshold = 0.5;
        int segment_id = 0;
        nbcam::PathSegment* current_segment = nullptr;
        int current_grayscale_bucket = -1;
        nbcam::PathPoint last_valid_job_point;
        bool has_last_valid_job_point = false;

        for (size_t i = 0; i < xyz_path_.size(); ++i) {
            const auto& curr = xyz_path_[i];
            if (!std::isfinite(curr.x) || !std::isfinite(curr.y) || !std::isfinite(curr.z)) {
                current_segment = nullptr;
                current_grayscale_bucket = -1;
                has_last_valid_job_point = false;
                continue;
            }

            const double curr_grayscale = current_path_all_svg_boundary_
                                              ? 1.0
                                              : (current_path_uses_grayscale_
                                                     ? std::clamp(curr.grayscale, 0.0, 1.0)
                                                     : 0.0);
            const int curr_bucket = current_path_all_svg_boundary_
                                        ? 26
                                        : (current_path_uses_grayscale_
                                               ? grayscaleBucketForPlan(curr_grayscale)
                                               : -1);

            bool is_break = false;
            if (i < path_break_flags_.size() && path_break_flags_[i]) {
                is_break = true;
            } else if (i > 0) {
                const auto& prev = xyz_path_[i - 1];
                if (!std::isfinite(prev.x) || !std::isfinite(prev.y) || !std::isfinite(prev.z)) {
                    is_break = true;
                } else {
                    const double dx = curr.x - prev.x;
                    const double dy = curr.y - prev.y;
                    const double dz = curr.z - prev.z;
                    const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dist > kJumpThreshold) {
                        is_break = true;
                    }
                }
            }

            if (is_break) {
                current_segment = nullptr;
                current_grayscale_bucket = -1;
            }

            bool bucket_changed = current_path_uses_grayscale_ &&
                                  current_segment != nullptr &&
                                  current_grayscale_bucket >= 0 &&
                                  curr_bucket != current_grayscale_bucket;

            if (!current_segment) {
                nbcam::PathSegment segment;
                segment.id = segment_id++;
                segment.type = nbcam::SegmentType::MARK;
                segment.strategy = current_fill_strategy_;
                segment.grayscale_bucket = curr_bucket;
                current_job_->segments.push_back(std::move(segment));
                current_segment = &current_job_->segments.back();
                current_grayscale_bucket = curr_bucket;
            } else if (bucket_changed) {
                nbcam::PathSegment segment;
                segment.id = segment_id++;
                segment.type = nbcam::SegmentType::MARK;
                segment.strategy = current_fill_strategy_;
                segment.grayscale_bucket = curr_bucket;
                if (has_last_valid_job_point) {
                    auto bridge = cloneJobPoint(last_valid_job_point);
                    bridge.grayscale = curr_grayscale;
                    if (bridge.params_override) {
                        bridge.params_override->grayscale = bridge.grayscale;
                    }
                    segment.points.push_back(std::move(bridge));
                }
                current_job_->segments.push_back(std::move(segment));
                current_segment = &current_job_->segments.back();
                current_grayscale_bucket = curr_bucket;
            }

            nbcam::PathPoint point;
            point.u = curr.u;
            point.v = curr.v;
            point.x = curr.x;
            point.y = curr.y;
            point.z = curr.z;
            point.a = curr.a;
            point.b = curr.b;
            point.laser = 1;
            point.grayscale = curr_grayscale;
            point.params_override = nullptr;

            if (curr.params_override) {
                point.params_override = std::make_unique<nbcam::ProcessParams>(*curr.params_override);
                point.params_override->grayscale = point.grayscale;
            }

            current_segment->points.push_back(std::move(point));
            last_valid_job_point = cloneJobPoint(current_segment->points.back());
            has_last_valid_job_point = true;
        }

        current_job_->segments.erase(
            std::remove_if(current_job_->segments.begin(), current_job_->segments.end(),
                           [](const nbcam::PathSegment& segment) { return segment.points.size() < 2; }),
            current_job_->segments.end());

        for (size_t i = 0; i < current_job_->segments.size(); ++i) {
            current_job_->segments[i].id = static_cast<int>(i);
        }

        if (current_job_->segments.empty()) {
            spdlog::error("PlanPath: no drawable segments after filtering");
            emit jobReady(false);
            return false;
        }

        if (!current_path_all_svg_boundary_ && !pending_svg_boundary_xyz_loops_.empty()) {
            for (const auto& contour_xyz : pending_svg_boundary_xyz_loops_) {
                if (contour_xyz.size() < 2) {
                    continue;
                }

                nbcam::PathSegment boundary_segment;
                boundary_segment.id = segment_id++;
                boundary_segment.type = nbcam::SegmentType::MARK;
                boundary_segment.strategy = nbcam::FillStrategy::CONTOUR;
                boundary_segment.grayscale_bucket = 26;
                boundary_segment.svg_boundary = true;
                boundary_segment.points.reserve(contour_xyz.size());
                for (const auto& src_point : contour_xyz) {
                    auto point = clonePathPointWithLaser(src_point, 1);
                    point.grayscale = 1.0;
                    if (point.params_override) {
                        point.params_override->grayscale = 1.0;
                    }
                    boundary_segment.points.push_back(std::move(point));
                }
                current_job_->segments.push_back(std::move(boundary_segment));
            }
        }

        if (apply_postprocess && current_strategy_name_ == "arc_hatch" && !arc_path_prebaked_) {
            resampleJobSegmentsToConvexArc(*current_job_,
                                           arc_center_x_,
                                           arc_center_z_,
                                           arc_radius_,
                                           arc_scan_speed_mm_s_);
            spdlog::info("PlanPath: applied convex arc resampling, center=({}, axis2={}), row-axis=z, radius={}, speed_mm_s={}",
                         arc_center_x_,
                         arc_center_z_,
                         arc_radius_,
                         arc_scan_speed_mm_s_);
        } else if (apply_postprocess && current_strategy_name_ == "arc_hatch") {
            spdlog::info("PlanPath: arc_hatch points are prebaked on arc surface, skip resampling");
        }

        if (apply_postprocess) {
            path_planner_.postProcess(*current_job_);
        }

        spdlog::info("PlanPath done: segments={}, points={}",
                     current_job_->segments.size(),
                     current_job_->getTotalPointCount());
        emit jobReady(true);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("PlanPath exception: {}", e.what());
        emit jobReady(false);
        return false;
    }
}

bool ApplicationController::saveJob(const std::string& filepath)
{
    if (!current_job_ || !current_job_->isValid()) {
        spdlog::error("SaveJob: current job is invalid");
        return false;
    }

    try {
        const bool success = serializer_.saveToFile(*current_job_, filepath);
        if (success) {
            spdlog::info("SaveJob done: {}", filepath);
        }
        return success;
    } catch (const std::exception& e) {
        spdlog::error("SaveJob exception: {}", e.what());
        return false;
    }
}

bool ApplicationController::loadJob(const std::string& filepath)
{
    try {
        current_job_ = serializer_.loadFromFile(filepath);
        if (!current_job_ || !current_job_->isValid()) {
            spdlog::error("LoadJob failed: invalid job");
            emit jobReady(false);
            return false;
        }

        planned_spacing_hint_mm_ = -1.0;
        emit jobReady(true);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("LoadJob exception: {}", e.what());
        emit jobReady(false);
        return false;
    }
}

bool ApplicationController::executeJob(nbcam::IJobExecutor* executor)
{
    if (!current_job_ || !current_job_->isValid()) {
        spdlog::error("ExecuteJob: current job is invalid");
        return false;
    }

    if (!executor) {
        spdlog::error("ExecuteJob: executor is null");
        return false;
    }

    try {
        emit executionStatusChanged("Running...");
        const bool success = executor->execute(*current_job_);
        emit executionStatusChanged(success ? "Done" : "Failed");
        return success;
    } catch (const std::exception& e) {
        spdlog::error("ExecuteJob exception: {}", e.what());
        emit executionStatusChanged(QString("Exception: %1").arg(e.what()));
        return false;
    }
}

bool ApplicationController::parameterizePatch(int patch_id,
                                              const std::vector<nbcam::Patch>& patches,
                                              const std::string& algorithm)
{
    if (!current_mesh_) {
        spdlog::error("ParameterizePatch: mesh is not loaded");
        return false;
    }

    if (patch_id < 0 || patch_id >= static_cast<int>(patches.size())) {
        spdlog::error("ParameterizePatch: invalid patch id {}", patch_id);
        return false;
    }

    try {
        nbcam::PatchParameterizer parameterizer;
        const auto result = parameterizer.parameterizePatch(*current_mesh_, patches[patch_id], algorithm);
        if (!result.success) {
            spdlog::error("ParameterizePatch failed: patch={}, algorithm={}", patch_id, algorithm);
            emit parameterizationCompleted(false);
            return false;
        }

        uv_coords_ = result.uv_coords;
        alignPatchUvAxesToModelXY(patches[patch_id], *current_mesh_, uv_coords_);
        parameterized_patch_id_ = patch_id;
        uv_path_.clear();
        xyz_path_.clear();
        path_break_flags_.clear();
        pattern_patch_id_ = -1;
        arc_path_prebaked_ = false;
        current_job_->clear();
        emit parameterizationCompleted(true);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("ParameterizePatch exception: {}", e.what());
        emit parameterizationCompleted(false);
        return false;
    }
}

bool ApplicationController::generatePatternInPatch(int patch_id,
                                                   const std::vector<nbcam::Patch>& patches,
                                                   const std::string& strategy,
                                                   double spacing,
                                                   double angle)
{
    PatternPlanOptions options;
    options.strategy = strategy;
    options.spacing = spacing;
    options.angle_deg = angle;
    options.direction_mode = "angle";
    options.z_layer_enabled = (normalizeStrategyName(strategy) == "z_layer");
    options.layer_height = spacing;
    return generatePatternInPatch(patch_id, patches, options);
}

void ApplicationController::clearPatternForPatch(int patch_id)
{
    if (patch_id < 0 || pattern_patch_id_ != patch_id) {
        return;
    }

    uv_path_.clear();
    xyz_path_.clear();
    path_break_flags_.clear();
    svg_boundary_.clear();
    pattern_patch_id_ = -1;
    planned_spacing_hint_mm_ = -1.0;
    arc_path_prebaked_ = false;
    current_path_all_svg_boundary_ = false;
    pending_svg_boundary_xyz_loops_.clear();
    if (current_job_) {
        current_job_->clear();
    }
    emit jobReady(false);
}

bool ApplicationController::generatePatternInPatch(int patch_id,
                                                   const std::vector<nbcam::Patch>& patches,
                                                   const PatternPlanOptions& options)
{
    if (!current_mesh_) {
        spdlog::error("GeneratePatternInPatch: mesh is not loaded");
        emit patternGenerated(false);
        return false;
    }

    if (patch_id < 0 || patch_id >= static_cast<int>(patches.size())) {
        spdlog::error("GeneratePatternInPatch: invalid patch id {}", patch_id);
        emit patternGenerated(false);
        return false;
    }

    const auto& patch = patches[patch_id];
    try {
        setGrayscalePowerRange(options.grayscale_power_min_w,
                               options.grayscale_power_max_w,
                               options.grayscale_gamma);
        arc_path_prebaked_ = false;
        // 不再切换“映射畸变模式”，默认沿用当前曲面展开的参数化类型。
        bool need_parameterize = true;
        if (parameterized_patch_id_ == patch_id && !uv_coords_.empty() &&
            uv_coords_.size() == current_mesh_->vertices.size()) {
            for (size_t tri_idx : patch.triangle_indices) {
                if (tri_idx >= current_mesh_->triangles.size()) {
                    continue;
                }
                const auto& tri = current_mesh_->triangles[tri_idx];
                for (size_t v_idx : {tri.v0, tri.v1, tri.v2}) {
                    if (v_idx < uv_coords_.size() && isFiniteUV(uv_coords_[v_idx])) {
                        need_parameterize = false;
                        break;
                    }
                }
                if (!need_parameterize) {
                    break;
                }
            }
        }
        if (need_parameterize) {
            std::string patch_algorithm = "LSCM";
            if (parameterizer_) {
                std::string alg_name = parameterizer_->getName();
                std::transform(alg_name.begin(), alg_name.end(), alg_name.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
                if (alg_name.find("ARAP") != std::string::npos) {
                    patch_algorithm = "ARAP";
                } else if (alg_name.find("AUTHALIC") != std::string::npos) {
                    patch_algorithm = "AUTHALIC";
                }
            }

            if (!parameterizePatch(patch_id, patches, patch_algorithm)) {
                spdlog::error("GeneratePatternInPatch: patch parameterization failed, algorithm={}", patch_algorithm);
                emit patternGenerated(false);
                return false;
            }
        }

        if (uv_coords_.empty() || uv_coords_.size() != current_mesh_->vertices.size()) {
            spdlog::error("GeneratePatternInPatch: invalid UV data");
            emit patternGenerated(false);
            return false;
        }

        double patch_u_min = 0.0, patch_u_max = 0.0, patch_v_min = 0.0, patch_v_max = 0.0;
        const bool has_patch_uv_bounds = getPatchUVBounds(patch, *current_mesh_, uv_coords_,
                                                          patch_u_min, patch_u_max, patch_v_min, patch_v_max);
        const bool enable_inverse_stretch_prewarp =
            options.enable_inverse_stretch_prewarp && has_patch_uv_bounds;
        std::vector<UvStretchSample> inverse_stretch_samples;
        double inverse_stretch_mean = 1.0;
        const double prewarp_center_u = (patch_u_min + patch_u_max) * 0.5;
        const double prewarp_center_v = (patch_v_min + patch_v_max) * 0.5;
        if (enable_inverse_stretch_prewarp) {
            inverse_stretch_samples = buildPatchUvStretchSamples(patch, *current_mesh_, uv_coords_);
            inverse_stretch_mean = robustMeanStretch(inverse_stretch_samples);
            if (!inverse_stretch_samples.empty()) {
                spdlog::info("GeneratePatternInPatch: inverse-stretch prewarp enabled, samples={}, mean_stretch={:.4f}, strength={:.2f}",
                             inverse_stretch_samples.size(),
                             inverse_stretch_mean,
                             options.inverse_stretch_prewarp_strength);
            } else {
                spdlog::warn("GeneratePatternInPatch: inverse-stretch prewarp requested but no valid stretch samples, skip");
            }
        }
        const std::string normalized_strategy = normalizeStrategyName(options.strategy);
        current_strategy_name_ = normalized_strategy;
        const bool contour_mode_none =
            (options.contour_mode == PatternPlanOptions::ContourProcessMode::NONE);
        const bool contour_mode_only =
            (options.contour_mode == PatternPlanOptions::ContourProcessMode::CONTOUR_ONLY);
        const bool contour_enabled = !contour_mode_none;
        const bool use_layered_contour =
            !contour_mode_only && (normalized_strategy == "z_layer" || options.z_layer_enabled);
        const bool enforce_svg_region = true;

        std::vector<nbcam::UVCoord> boundary;
        std::vector<std::vector<nbcam::UVCoord>> svg_contours;
        std::vector<unsigned char> svg_mask_rgba;
        constexpr int kSvgMaskBaseSize = 1024;
        int svg_mask_w = kSvgMaskBaseSize;
        int svg_mask_h = kSvgMaskBaseSize;
        const bool need_contour_prefix = contour_enabled && !use_layered_contour;
        const bool need_svg_outer_contour =
            !use_layered_contour && !contour_mode_only &&
            (normalized_strategy != "arc_hatch" || contour_enabled);
        double svg_map_x_min = 0.0;
        double svg_map_x_max = 1.0;
        double svg_map_y_min = 0.0;
        double svg_map_y_max = 1.0;

        if (enforce_svg_region && options.svg_filepath.empty()) {
            spdlog::error("GeneratePatternInPatch: strategy '{}' requires SVG texture region, but svg_filepath is empty",
                         normalized_strategy);
            emit patternGenerated(false);
            return false;
        }

        if (enforce_svg_region && !has_patch_uv_bounds) {
            spdlog::error("GeneratePatternInPatch: failed to compute patch UV bounds for SVG region clipping");
            emit patternGenerated(false);
            return false;
        }

        if (enforce_svg_region) {
            nbcam::NanosvgWrapper parser;
            if (!parser.loadFromFile(options.svg_filepath)) {
                spdlog::error("GeneratePatternInPatch: failed to parse SVG boundary {}",
                              options.svg_filepath);
                emit patternGenerated(false);
                return false;
            }

            // 轮廓提取使用更细离散，尽量贴近SVG真实边界。
            const double contour_flatten_tolerance = std::clamp(options.spacing * 0.2, 0.02, 0.2);
            const auto svg_shapes = parser.getFilledShapePaths(contour_flatten_tolerance);
            if (svg_shapes.empty()) {
                spdlog::error("GeneratePatternInPatch: no filled SVG shapes found in {}",
                              options.svg_filepath);
                emit patternGenerated(false);
                return false;
            }

            double shape_x_min = 0.0, shape_x_max = 0.0, shape_y_min = 0.0, shape_y_max = 0.0;
            if (!computeSvgShapeBounds(svg_shapes, shape_x_min, shape_x_max, shape_y_min, shape_y_max)) {
                spdlog::error("GeneratePatternInPatch: failed to compute SVG geometry bounds");
                emit patternGenerated(false);
                return false;
            }

            double canvas_x_min = 0.0, canvas_x_max = 0.0, canvas_y_min = 0.0, canvas_y_max = 0.0;
            parser.getCanvasBounds(canvas_x_min, canvas_y_min, canvas_x_max, canvas_y_max);
            const bool has_canvas_bounds =
                std::isfinite(canvas_x_min) && std::isfinite(canvas_x_max) &&
                std::isfinite(canvas_y_min) && std::isfinite(canvas_y_max) &&
                (canvas_x_max - canvas_x_min) > 1e-9 &&
                (canvas_y_max - canvas_y_min) > 1e-9;

            // 轮廓与mask都基于同一SVG坐标框进行映射，避免“轮廓和原图不同步”。
            if (has_canvas_bounds) {
                svg_map_x_min = canvas_x_min;
                svg_map_x_max = canvas_x_max;
                svg_map_y_min = canvas_y_min;
                svg_map_y_max = canvas_y_max;
            } else {
                svg_map_x_min = shape_x_min;
                svg_map_x_max = shape_x_max;
                svg_map_y_min = shape_y_min;
                svg_map_y_max = shape_y_max;
            }

            const double svg_map_w = svg_map_x_max - svg_map_x_min;
            const double svg_map_h = svg_map_y_max - svg_map_y_min;
            if (!(svg_map_w > 1e-9) || !(svg_map_h > 1e-9)) {
                spdlog::error("GeneratePatternInPatch: invalid SVG mapping bounds");
                emit patternGenerated(false);
                return false;
            }

            if (svg_map_w >= svg_map_h) {
                svg_mask_w = kSvgMaskBaseSize;
                svg_mask_h = std::clamp(static_cast<int>(std::lround(
                                             static_cast<double>(kSvgMaskBaseSize) * svg_map_h / svg_map_w)),
                                        64, kSvgMaskBaseSize);
            } else {
                svg_mask_h = kSvgMaskBaseSize;
                svg_mask_w = std::clamp(static_cast<int>(std::lround(
                                             static_cast<double>(kSvgMaskBaseSize) * svg_map_w / svg_map_h)),
                                        64, kSvgMaskBaseSize);
            }

            // 先在SVG平面提取轮廓，再映射到patch UV。
            auto svg_boundary_loops = extractSvgBoundaryLoopsFromShapes(svg_shapes);
            if (svg_boundary_loops.empty()) {
                svg_boundary_loops = collectRawSvgLoops(svg_shapes);
            }

            if (need_svg_outer_contour || need_contour_prefix) {
                svg_contours = mapSvgBoundaryLoopsToPatchUV(svg_boundary_loops,
                                                            options,
                                                            svg_map_x_min, svg_map_x_max,
                                                            svg_map_y_min, svg_map_y_max,
                                                            patch_u_min, patch_u_max, patch_v_min, patch_v_max);
                if (svg_contours.empty()) {
                    spdlog::error("GeneratePatternInPatch: failed to map extracted SVG contours onto patch UV region");
                    emit patternGenerated(false);
                    return false;
                }
                if (enable_inverse_stretch_prewarp && !inverse_stretch_samples.empty()) {
                    applyInverseStretchPrewarpContours(svg_contours,
                                                       inverse_stretch_samples,
                                                       inverse_stretch_mean,
                                                       prewarp_center_u,
                                                       prewarp_center_v,
                                                       options.inverse_stretch_prewarp_strength,
                                                       patch_u_min, patch_u_max, patch_v_min, patch_v_max);
                }
                // 轮廓加工顺序：起点固定为左上角，然后用TSP近似最短跳转顺序。
                reorderContoursWithTsp(svg_contours);
                spdlog::info("GeneratePatternInPatch: contour loops reordered by TSP, loops={}", svg_contours.size());
                spdlog::info("GeneratePatternInPatch: extracted SVG contours in svg-space={}, mapped_uv_loops={}",
                             svg_boundary_loops.size(), svg_contours.size());
            }

            if (need_svg_outer_contour) {
                if (svg_contours.size() == 1) {
                    boundary = svg_contours.front();
                } else {
                    std::vector<nbcam::UVCoord> contour_cloud;
                    for (const auto& loop : svg_contours) {
                        contour_cloud.insert(contour_cloud.end(), loop.begin(), loop.end());
                    }
                    boundary = buildConvexHullBoundary(contour_cloud);
                    if (boundary.size() < 3) {
                        boundary = pickLargestContour(svg_contours);
                    }
                    if (boundary.size() < 3) {
                        boundary = mapSvgBoundingRectToPatchUV(svg_map_x_min, svg_map_x_max,
                                                               svg_map_y_min, svg_map_y_max,
                                                               options,
                                                               patch_u_min, patch_u_max, patch_v_min, patch_v_max);
                        if (enable_inverse_stretch_prewarp && !inverse_stretch_samples.empty()) {
                            applyInverseStretchPrewarpBoundary(boundary,
                                                               inverse_stretch_samples,
                                                               inverse_stretch_mean,
                                                               prewarp_center_u,
                                                               prewarp_center_v,
                                                               options.inverse_stretch_prewarp_strength,
                                                               patch_u_min, patch_u_max, patch_v_min, patch_v_max);
                        }
                    }
                }

                if (boundary.size() < 3) {
                    spdlog::error("GeneratePatternInPatch: failed to build fill boundary from SVG contours");
                    emit patternGenerated(false);
                    return false;
                }
            }

            if (!renderSvgMaskRgba(parser, svg_mask_w, svg_mask_h, svg_mask_rgba)) {
                if (!loadSvgMaskRgba(options.svg_filepath, svg_mask_w, svg_mask_h, svg_mask_rgba)) {
                    spdlog::error("GeneratePatternInPatch: failed to build SVG alpha mask for non-line strategy");
                    emit patternGenerated(false);
                    return false;
                }
            }
            SvgAspectFitFrame fit_frame;
            if (computeSvgAspectFitFrame(patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                         svg_map_w / svg_map_h, fit_frame)) {
                spdlog::info("GeneratePatternInPatch: aspect-fit uv frame U=[{:.6f},{:.6f}] V=[{:.6f},{:.6f}]",
                             fit_frame.u_min, fit_frame.u_max, fit_frame.v_min, fit_frame.v_max);
            }
            spdlog::info("GeneratePatternInPatch: svg map frame=[{:.3f},{:.3f}]x[{:.3f},{:.3f}], mask={}x{}",
                         svg_map_x_min, svg_map_x_max, svg_map_y_min, svg_map_y_max, svg_mask_w, svg_mask_h);
        }

        std::vector<nbcam::UVCoord> patch_domain = buildPatchBoundary(patch, uv_coords_);
        if (patch_domain.size() < 3) {
            patch_domain = buildConvexHullBoundary(collectPatchUVCloud(patch, *current_mesh_, uv_coords_));
        }

        std::vector<std::vector<nbcam::UVPathPoint>> contour_uv_loops;
        std::vector<std::vector<nbcam::PathPoint>> contour_xyz_loops;
        if (need_contour_prefix) {
            // 轮廓优先采用更细离散，保证边界拟合精度。
            const double contour_densify_step = std::clamp(options.spacing * 0.25, 0.01, 0.05);
            size_t contour_uv_points_total = 0;
            for (const auto& contour_loop : svg_contours) {
                std::vector<nbcam::UVPathPoint> contour_uv_path;
                std::vector<nbcam::PathPoint> contour_xyz_path;
                if (!buildContourMarkPathOnPatch(contour_loop,
                                                 contour_densify_step,
                                                 *current_mesh_,
                                                 uv_coords_,
                                                 patch.triangle_indices,
                                                 options,
                                                 patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                                 svg_mask_rgba,
                                                 svg_mask_w, svg_mask_h,
                                                 contour_uv_path,
                                                 contour_xyz_path)) {
                    continue;
                }
                contour_uv_points_total += contour_uv_path.size();
                contour_uv_loops.push_back(std::move(contour_uv_path));
                contour_xyz_loops.push_back(std::move(contour_xyz_path));
            }

            if (contour_uv_loops.empty() || contour_xyz_loops.empty()) {
                spdlog::error("GeneratePatternInPatch: failed to build any mapped SVG contour loop on patch");
                emit patternGenerated(false);
                return false;
            }

            size_t contour_xyz_points_total = 0;
            for (const auto& loop : contour_xyz_loops) {
                contour_xyz_points_total += loop.size();
            }
            spdlog::info("GeneratePatternInPatch: contour prefix prepared, loops={}, uv_points={}, xyz_points={}",
                         contour_xyz_loops.size(),
                         contour_uv_points_total,
                         contour_xyz_points_total);
        }

        if (contour_mode_only) {
            uv_path_.clear();
            xyz_path_.clear();
            path_break_flags_.clear();
            pending_svg_boundary_xyz_loops_.clear();

            for (size_t loop_idx = 0; loop_idx < contour_uv_loops.size() && loop_idx < contour_xyz_loops.size(); ++loop_idx) {
                const auto& uv_loop = contour_uv_loops[loop_idx];
                const auto& xyz_loop = contour_xyz_loops[loop_idx];
                const size_t count = std::min(uv_loop.size(), xyz_loop.size());
                for (size_t i = 0; i < count; ++i) {
                    nbcam::UVPathPoint uv_pt = uv_loop[i];
                    const bool jump_before = (i == 0);
                    uv_pt.is_jump_before = jump_before;
                    uv_path_.push_back(uv_pt);
                    xyz_path_.push_back(clonePathPointWithLaser(xyz_loop[i], xyz_loop[i].laser));
                    path_break_flags_.push_back(jump_before);
                }
            }

            if (uv_path_.empty() || xyz_path_.empty()) {
                spdlog::error("GeneratePatternInPatch: contour-only mode produced empty contour path");
                emit patternGenerated(false);
                return false;
            }

            current_fill_strategy_ = nbcam::FillStrategy::CONTOUR;
            current_job_->clear();
            current_path_uses_grayscale_ = !svg_mask_rgba.empty();
            current_path_all_svg_boundary_ = true;
            pending_svg_boundary_xyz_loops_ = clonePathLoopsWithLaser(contour_xyz_loops, 1);
            current_job_->grayscale_enabled = current_path_uses_grayscale_;
            current_job_->grayscale_power_min_w = options.grayscale_power_min_w;
            current_job_->grayscale_power_max_w = options.grayscale_power_max_w;
            current_job_->grayscale_gamma = options.grayscale_gamma;
            current_job_->grayscale_source = "svg_luminance";

            planned_spacing_hint_mm_ = options.spacing;
            pattern_patch_id_ = patch_id;
            if (!planPath()) {
                spdlog::error("GeneratePatternInPatch: contour-only planPath failed");
                emit patternGenerated(false);
                return false;
            }
            emit patternGenerated(true);
            return true;
        }

        if (normalized_strategy == "arc_hatch") {
            current_fill_strategy_ = nbcam::FillStrategy::ARC_HATCH;
            arc_scan_speed_mm_s_ = options.scan_speed_mm_s;
            arc_path_prebaked_ = true;
            double used_center_x = options.arc_center_x;
            double used_center_axis2 = options.arc_center_z;
            double used_radius = options.arc_radius;

            if (!generateArcHatchPathFromSvgMask(patch,
                                                 *current_mesh_,
                                                 options,
                                                 svg_mask_rgba,
                                                 svg_mask_w,
                                                 svg_mask_h,
                                                 uv_path_,
                                                 xyz_path_,
                                                 path_break_flags_,
                                                 &used_center_x,
                                                 &used_center_axis2,
                                                 &used_radius)) {
                spdlog::error("GeneratePatternInPatch: arc_hatch generation on arc surface failed");
                emit patternGenerated(false);
                return false;
            }
            arc_center_x_ = used_center_x;
            arc_center_z_ = used_center_axis2;
            arc_radius_ = used_radius;
            current_path_uses_grayscale_ = !svg_mask_rgba.empty();
            current_path_all_svg_boundary_ = false;
            pending_svg_boundary_xyz_loops_.clear();
            if (need_contour_prefix) {
                pending_svg_boundary_xyz_loops_ = clonePathLoopsWithLaser(contour_xyz_loops, 1);
            }
            spdlog::info("GeneratePatternInPatch: arc_hatch generated directly on arc surface, center=({}, {}, row-axis-auto), R={}, spacing={}",
                         used_center_x,
                         used_center_axis2,
                         used_radius,
                         options.spacing);

            if (!assignProcessParams("curvature")) {
                spdlog::warn("GeneratePatternInPatch: assignProcessParams failed, continue with defaults");
            }

            if (!planPath()) {
                spdlog::error("GeneratePatternInPatch: planPath failed");
                emit patternGenerated(false);
                return false;
            }

            planned_spacing_hint_mm_ = options.spacing;
            pattern_patch_id_ = patch_id;
            emit patternGenerated(true);
            return true;
        }

        if (use_layered_contour) {
            current_fill_strategy_ = nbcam::FillStrategy::CONTOUR;
            const bool apply_svg_mask = (enforce_svg_region && has_patch_uv_bounds && !svg_mask_rgba.empty());
            if (!generateZLayerContourPath(patch, *current_mesh_, uv_coords_, options,
                                           apply_svg_mask,
                                           patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                           svg_mask_rgba, svg_mask_w, svg_mask_h,
                                           uv_path_, xyz_path_, path_break_flags_)) {
                spdlog::error("GeneratePatternInPatch: z-layer contour generation failed");
                emit patternGenerated(false);
                return false;
            }

            current_path_uses_grayscale_ = apply_svg_mask;
            current_path_all_svg_boundary_ = false;

            if (!assignProcessParams("curvature")) {
                spdlog::warn("GeneratePatternInPatch: assignProcessParams failed, continue with defaults");
            }

            if (!planPath()) {
                spdlog::error("GeneratePatternInPatch: planPath failed");
                emit patternGenerated(false);
                return false;
            }

            planned_spacing_hint_mm_ = (options.layer_height > 1e-9) ? options.layer_height : options.spacing;
            pattern_patch_id_ = patch_id;
            emit patternGenerated(true);
            return true;
        }

        if (boundary.size() < 3) {
            spdlog::error("GeneratePatternInPatch: failed to build SVG outer contour boundary");
            emit patternGenerated(false);
            return false;
        }

        fill_strategy_ = createFillStrategy(normalized_strategy);
        current_fill_strategy_ = toFillStrategyEnum(normalized_strategy);
        arc_center_x_ = options.arc_center_x;
        arc_center_z_ = options.arc_center_z;
        arc_radius_ = options.arc_radius;
        arc_scan_speed_mm_s_ = options.scan_speed_mm_s;
        arc_path_prebaked_ = false;
        current_path_all_svg_boundary_ = false;
        pending_svg_boundary_xyz_loops_.clear();

        double effective_angle = resolveAngleFromDirection(options.angle_deg, options.direction_mode);
        if (normalized_strategy == "ring_outin") {
            effective_angle = -1.0;
        }
        uv_path_ = fill_strategy_->generatePath(boundary, options.spacing, effective_angle);
        const double densify_step = std::max(0.5, options.spacing * 2.0);
        uv_path_ = densifyUVPathSegments(uv_path_, densify_step);
        const size_t original_uv_count = uv_path_.size();
        if (patch_domain.size() >= 3) {
            auto clipped_uv_path = clipUVPathToPatchDomain(uv_path_, patch_domain);
            if (!clipped_uv_path.empty() && clipped_uv_path.size() < uv_path_.size()) {
                spdlog::info("GeneratePatternInPatch: clipped UV path to patch domain, {} -> {} points",
                             uv_path_.size(), clipped_uv_path.size());
                uv_path_ = std::move(clipped_uv_path);
            }
        }
        if (uv_path_.size() < 10 && original_uv_count >= 100) {
            auto hull_domain = buildConvexHullBoundary(collectPatchUVCloud(patch, *current_mesh_, uv_coords_));
            if (hull_domain.size() >= 3) {
                auto hull_source_path = fill_strategy_->generatePath(boundary, options.spacing, effective_angle);
                hull_source_path = densifyUVPathSegments(hull_source_path, densify_step);
                auto hull_clipped_path = clipUVPathToPatchDomain(hull_source_path, hull_domain);
                if (hull_clipped_path.size() > uv_path_.size()) {
                    spdlog::warn("GeneratePatternInPatch: patch boundary clipping kept too few points ({}), use convex hull fallback ({})",
                                 uv_path_.size(),
                                 hull_clipped_path.size());
                    uv_path_ = std::move(hull_clipped_path);
                }
            }
        }
        path_break_flags_.resize(uv_path_.size());
        for (size_t i = 0; i < uv_path_.size(); ++i) {
            path_break_flags_[i] = uv_path_[i].is_jump_before;
        }

        const bool apply_svg_alpha_clip = (enforce_svg_region && !svg_mask_rgba.empty() && has_patch_uv_bounds);
        if (apply_svg_alpha_clip) {
            const size_t before_mask = uv_path_.size();
            clipPathBySvgMask(uv_path_, path_break_flags_, options,
                              patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                              svg_mask_rgba, svg_mask_w, svg_mask_h);
            spdlog::info("GeneratePatternInPatch: clipped UV path to SVG drawn region, {} -> {} points",
                         before_mask, uv_path_.size());
        }

        if (!uv_path_.empty() && !svg_mask_rgba.empty()) {
            applySvgGrayscaleToPath(uv_path_, options,
                                    patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                    svg_mask_rgba, svg_mask_w, svg_mask_h);
            current_path_uses_grayscale_ = true;
        }

        if (enable_inverse_stretch_prewarp && !inverse_stretch_samples.empty() && !uv_path_.empty()) {
            const size_t before_prewarp = uv_path_.size();
            applyInverseStretchPrewarpPath(uv_path_,
                                           inverse_stretch_samples,
                                           inverse_stretch_mean,
                                           prewarp_center_u,
                                           prewarp_center_v,
                                           options.inverse_stretch_prewarp_strength,
                                           patch_u_min, patch_u_max, patch_v_min, patch_v_max);
            if (patch_domain.size() >= 3) {
                auto reclip = clipUVPathToPatchDomain(uv_path_, patch_domain);
                if (!reclip.empty()) {
                    uv_path_ = std::move(reclip);
                }
            }
            path_break_flags_.resize(uv_path_.size());
            for (size_t i = 0; i < uv_path_.size(); ++i) {
                path_break_flags_[i] = uv_path_[i].is_jump_before;
            }
            if (!svg_mask_rgba.empty()) {
                applySvgGrayscaleToPath(uv_path_, options,
                                        patch_u_min, patch_u_max, patch_v_min, patch_v_max,
                                        svg_mask_rgba, svg_mask_w, svg_mask_h);
            }
            spdlog::info("GeneratePatternInPatch: inverse-stretch prewarp applied to UV path, points={} -> {}",
                         before_prewarp,
                         uv_path_.size());
        }

        if (uv_path_.empty()) {
            spdlog::error("GeneratePatternInPatch: strategy produced empty path");
            emit patternGenerated(false);
            return false;
        }

        if (!mapToXYZ(&patch.triangle_indices)) {
            spdlog::error("GeneratePatternInPatch: UV->XYZ mapping failed");
            emit patternGenerated(false);
            return false;
        }

        if (!assignProcessParams("curvature")) {
            spdlog::warn("GeneratePatternInPatch: assignProcessParams failed, continue with defaults");
        }

        if (need_contour_prefix) {
            pending_svg_boundary_xyz_loops_ = clonePathLoopsWithLaser(contour_xyz_loops, 1);
        }

        if (!planPath()) {
            spdlog::error("GeneratePatternInPatch: planPath failed");
            emit patternGenerated(false);
            return false;
        }

        planned_spacing_hint_mm_ = options.spacing;
        pattern_patch_id_ = patch_id;
        emit patternGenerated(true);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("GeneratePatternInPatch exception: {}", e.what());
        emit patternGenerated(false);
        return false;
    }
}
