#include "contour_strategy.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace nbcam {

namespace {

constexpr double kEps = 1e-9;
constexpr double kMinSampleStep = 1e-4;

double signedArea(const std::vector<UVCoord>& polygon)
{
    if (polygon.size() < 3) {
        return 0.0;
    }
    double acc = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const auto& p = polygon[i];
        const auto& q = polygon[(i + 1) % polygon.size()];
        acc += p.u * q.v - q.u * p.v;
    }
    return 0.5 * acc;
}

bool lineIntersection(const UVCoord& p,
                      const UVCoord& r,
                      const UVCoord& q,
                      const UVCoord& s,
                      UVCoord& out)
{
    const double cross = r.u * s.v - r.v * s.u;
    if (std::abs(cross) < kEps) {
        return false;
    }
    const double t = ((q.u - p.u) * s.v - (q.v - p.v) * s.u) / cross;
    out.u = p.u + t * r.u;
    out.v = p.v + t * r.v;
    return std::isfinite(out.u) && std::isfinite(out.v);
}

double uvDistance(const UVCoord& a, const UVCoord& b)
{
    const double du = a.u - b.u;
    const double dv = a.v - b.v;
    return std::sqrt(du * du + dv * dv);
}

UVCoord lerpUV(const UVCoord& a, const UVCoord& b, double t)
{
    return {a.u + (b.u - a.u) * t, a.v + (b.v - a.v) * t};
}

UVCoord evalQuadraticBezier(const UVCoord& p0, const UVCoord& p1, const UVCoord& p2, double t)
{
    const double omt = 1.0 - t;
    const double w0 = omt * omt;
    const double w1 = 2.0 * omt * t;
    const double w2 = t * t;
    return {
        w0 * p0.u + w1 * p1.u + w2 * p2.u,
        w0 * p0.v + w1 * p1.v + w2 * p2.v
    };
}

void appendUniqueUV(std::vector<UVCoord>& out, const UVCoord& pt)
{
    if (!std::isfinite(pt.u) || !std::isfinite(pt.v)) {
        return;
    }
    if (out.empty() || uvDistance(out.back(), pt) > 1e-8) {
        out.push_back(pt);
    }
}

std::vector<UVCoord> buildBezierContourLoop(const std::vector<UVCoord>& polygon, double spacing)
{
    std::vector<UVCoord> loop;
    if (polygon.size() < 3) {
        return loop;
    }

    const size_t n = polygon.size();
    const double sample_step = std::max(spacing * 0.35, kMinSampleStep);

    std::vector<UVCoord> entry(n);
    std::vector<UVCoord> exit(n);
    for (size_t i = 0; i < n; ++i) {
        const auto& prev = polygon[(i + n - 1) % n];
        const auto& curr = polygon[i];
        const auto& next = polygon[(i + 1) % n];
        const double len_prev = uvDistance(curr, prev);
        const double len_next = uvDistance(next, curr);
        if (len_prev < kEps || len_next < kEps) {
            entry[i] = curr;
            exit[i] = curr;
            continue;
        }

        const double min_len = std::min(len_prev, len_next);
        double trim = std::min(min_len * 0.2, spacing * 0.75);
        trim = std::max(trim, min_len * 0.05);
        trim = std::min(trim, min_len * 0.45);

        const UVCoord dir_prev{(curr.u - prev.u) / len_prev, (curr.v - prev.v) / len_prev};
        const UVCoord dir_next{(next.u - curr.u) / len_next, (next.v - curr.v) / len_next};
        entry[i] = {curr.u - dir_prev.u * trim, curr.v - dir_prev.v * trim};
        exit[i] = {curr.u + dir_next.u * trim, curr.v + dir_next.v * trim};
    }

    appendUniqueUV(loop, entry[0]);
    for (size_t i = 0; i < n; ++i) {
        const auto& curr = polygon[i];
        const auto& c0 = entry[i];
        const auto& c2 = exit[i];

        const double corner_len = uvDistance(c0, curr) + uvDistance(curr, c2);
        const int corner_samples = std::clamp(static_cast<int>(std::ceil(corner_len / sample_step)), 2, 24);
        for (int s = 1; s <= corner_samples; ++s) {
            const double t = static_cast<double>(s) / static_cast<double>(corner_samples);
            appendUniqueUV(loop, evalQuadraticBezier(c0, curr, c2, t));
        }

        const auto& bridge_start = c2;
        const auto& bridge_end = entry[(i + 1) % n];
        const double bridge_len = uvDistance(bridge_start, bridge_end);
        const int bridge_samples = std::clamp(static_cast<int>(std::ceil(bridge_len / std::max(spacing * 0.5, sample_step))), 1, 32);
        for (int s = 1; s <= bridge_samples; ++s) {
            const double t = static_cast<double>(s) / static_cast<double>(bridge_samples);
            appendUniqueUV(loop, lerpUV(bridge_start, bridge_end, t));
        }
    }

    if (loop.size() >= 2 && uvDistance(loop.front(), loop.back()) <= 1e-8) {
        loop.pop_back();
    }
    return loop;
}

std::vector<UVCoord> offsetInward(const std::vector<UVCoord>& polygon, double distance)
{
    std::vector<UVCoord> result;
    if (polygon.size() < 3 || distance <= 0.0) {
        return result;
    }
    result.reserve(polygon.size());

    for (size_t i = 0; i < polygon.size(); ++i) {
        const auto& prev = polygon[(i + polygon.size() - 1) % polygon.size()];
        const auto& curr = polygon[i];
        const auto& next = polygon[(i + 1) % polygon.size()];

        UVCoord e1{curr.u - prev.u, curr.v - prev.v};
        UVCoord e2{next.u - curr.u, next.v - curr.v};
        const double len1 = std::sqrt(e1.u * e1.u + e1.v * e1.v);
        const double len2 = std::sqrt(e2.u * e2.u + e2.v * e2.v);
        if (len1 < kEps || len2 < kEps) {
            continue;
        }
        e1.u /= len1;
        e1.v /= len1;
        e2.u /= len2;
        e2.v /= len2;

        // CCW 多边形的左法线指向内部。
        UVCoord n1{-e1.v, e1.u};
        UVCoord n2{-e2.v, e2.u};

        UVCoord p1{prev.u + n1.u * distance, prev.v + n1.v * distance};
        UVCoord p2{curr.u + n2.u * distance, curr.v + n2.v * distance};

        UVCoord inter{};
        if (lineIntersection(p1, e1, p2, e2, inter)) {
            result.push_back(inter);
            continue;
        }

        UVCoord avg_n{n1.u + n2.u, n1.v + n2.v};
        const double avg_len = std::sqrt(avg_n.u * avg_n.u + avg_n.v * avg_n.v);
        if (avg_len < kEps) {
            result.push_back({curr.u + n1.u * distance, curr.v + n1.v * distance});
        } else {
            result.push_back({curr.u + avg_n.u / avg_len * distance, curr.v + avg_n.v / avg_len * distance});
        }
    }

    return result;
}

}  // namespace

std::vector<UVPathPoint> ContourStrategy::generatePath(
    const std::vector<UVCoord>& boundary,
    double spacing,
    double /*angle*/)
{
    std::vector<UVPathPoint> path;

    if (boundary.empty() || spacing <= 0.0) {
        spdlog::warn("ContourFill: invalid boundary or spacing");
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
        return path;
    }

    if (std::abs(polygon.front().u - polygon.back().u) < kEps &&
        std::abs(polygon.front().v - polygon.back().v) < kEps) {
        polygon.pop_back();
    }
    if (polygon.size() < 3) {
        spdlog::warn("ContourFill: boundary collapsed");
        return path;
    }

    if (signedArea(polygon) < 0.0) {
        std::reverse(polygon.begin(), polygon.end());
    }

    std::vector<UVCoord> current = polygon;
    double current_area = std::abs(signedArea(current));
    int layer = 0;
    while (current.size() >= 3 && current_area > kEps) {
        std::vector<UVCoord> contour_loop = buildBezierContourLoop(current, spacing);
        if (contour_loop.size() < 3) {
            contour_loop = current;
        }

        for (size_t i = 0; i < contour_loop.size(); ++i) {
            UVPathPoint pt;
            pt.u = contour_loop[i].u;
            pt.v = contour_loop[i].v;
            pt.is_jump_before = (layer > 0 && i == 0);
            path.push_back(pt);
        }
        UVPathPoint close;
        close.u = contour_loop.front().u;
        close.v = contour_loop.front().v;
        close.is_jump_before = false;
        close.is_arrow_tip = true;
        path.push_back(close);

        auto next = offsetInward(current, spacing);
        if (next.size() < 3) {
            break;
        }

        const double next_area = std::abs(signedArea(next));
        if (next_area >= current_area - kEps) {
            break;
        }

        current = std::move(next);
        current_area = next_area;
        ++layer;
    }

    spdlog::info("ContourFill generated: {} layers, {} points", layer, path.size());
    return path;
}

}  // namespace nbcam
