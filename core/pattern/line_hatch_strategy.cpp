#include "line_hatch_strategy.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace nbcam {

namespace {

std::pair<double, double> rotatePoint(double u, double v, double center_u, double center_v, double angle_rad)
{
    const double c = std::cos(angle_rad);
    const double s = std::sin(angle_rad);
    const double du = u - center_u;
    const double dv = v - center_v;
    return {
        center_u + du * c - dv * s,
        center_v + du * s + dv * c,
    };
}

}  // namespace

std::vector<UVPathPoint> LineHatchStrategy::generatePath(
    const std::vector<UVCoord>& boundary,
    double spacing,
    double angle)
{
    std::vector<UVPathPoint> path;

    if (boundary.empty() || spacing <= 0.0) {
        spdlog::warn("LineHatch: invalid boundary or spacing");
        return path;
    }

    std::vector<UVCoord> polygon;
    polygon.reserve(boundary.size());
    for (const auto& uv : boundary) {
        if (std::isfinite(uv.u) && std::isfinite(uv.v)) {
            polygon.push_back(uv);
        }
    }
    if (polygon.size() < 3) {
        spdlog::warn("LineHatch: boundary too small");
        return path;
    }

    if (std::abs(polygon.front().u - polygon.back().u) < 1e-9 &&
        std::abs(polygon.front().v - polygon.back().v) < 1e-9) {
        polygon.pop_back();
    }
    if (polygon.size() < 3) {
        spdlog::warn("LineHatch: closed boundary collapsed");
        return path;
    }

    double center_u = 0.0;
    double center_v = 0.0;
    for (const auto& uv : polygon) {
        center_u += uv.u;
        center_v += uv.v;
    }
    center_u /= static_cast<double>(polygon.size());
    center_v /= static_cast<double>(polygon.size());

    const double angle_rad = angle * M_PI / 180.0;
    std::vector<UVCoord> rotated_polygon;
    rotated_polygon.reserve(polygon.size());
    for (const auto& uv : polygon) {
        auto [ru, rv] = rotatePoint(uv.u, uv.v, center_u, center_v, -angle_rad);
        rotated_polygon.push_back({ru, rv});
    }

    double u_min = rotated_polygon[0].u;
    double u_max = rotated_polygon[0].u;
    double v_min = rotated_polygon[0].v;
    double v_max = rotated_polygon[0].v;
    for (const auto& uv : rotated_polygon) {
        u_min = std::min(u_min, uv.u);
        u_max = std::max(u_max, uv.u);
        v_min = std::min(v_min, uv.v);
        v_max = std::max(v_max, uv.v);
    }

    const double v_range = v_max - v_min;
    if (v_range <= 1e-9) {
        spdlog::warn("LineHatch: invalid V range");
        return path;
    }

    int line_count = static_cast<int>(std::floor(v_range / spacing)) + 1;
    if (line_count <= 0) {
        line_count = 1;
    }

    int stripe_index = 0;
    constexpr double kEps = 1e-9;

    for (int i = 0; i < line_count; ++i) {
        const double v = v_min + i * spacing;
        if (v > v_max + kEps) {
            break;
        }

        std::vector<double> intersections;
        intersections.reserve(rotated_polygon.size());

        for (size_t j = 0; j < rotated_polygon.size(); ++j) {
            const auto& a = rotated_polygon[j];
            const auto& b = rotated_polygon[(j + 1) % rotated_polygon.size()];

            if (std::abs(a.v - b.v) < kEps) {
                continue;
            }

            const double v_low = std::min(a.v, b.v);
            const double v_high = std::max(a.v, b.v);
            if (v < v_low || v >= v_high) {
                continue;
            }

            const double t = (v - a.v) / (b.v - a.v);
            intersections.push_back(a.u + t * (b.u - a.u));
        }

        if (intersections.size() < 2) {
            continue;
        }

        std::sort(intersections.begin(), intersections.end());
        for (size_t k = 0; k + 1 < intersections.size(); k += 2) {
            double x0 = intersections[k];
            double x1 = intersections[k + 1];
            if (x1 - x0 < kEps) {
                continue;
            }

            const bool forward = (stripe_index % 2 == 0);
            const double start_x = forward ? x0 : x1;
            const double end_x = forward ? x1 : x0;
            auto [u0, v0] = rotatePoint(start_x, v, center_u, center_v, angle_rad);
            auto [u1, v1] = rotatePoint(end_x, v, center_u, center_v, angle_rad);

            UVPathPoint p0;
            p0.u = u0;
            p0.v = v0;
            p0.is_jump_before = !path.empty();

            UVPathPoint p1;
            p1.u = u1;
            p1.v = v1;
            p1.is_arrow_tip = true;
            path.push_back(p0);
            path.push_back(p1);
            ++stripe_index;
        }
    }

    spdlog::info("LineHatch generated: {} points", path.size());
    return path;
}

}  // namespace nbcam
