#include "ring_fill_strategy.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace nbcam {

std::vector<UVPathPoint> RingFillStrategy::generatePath(
    const std::vector<UVCoord>& boundary,
    double spacing,
    double angle)
{
    std::vector<UVPathPoint> path;

    if (boundary.empty() || spacing <= 0.0) {
        spdlog::warn("RingFill: invalid boundary or spacing");
        return path;
    }

    double u_min = boundary[0].u;
    double u_max = boundary[0].u;
    double v_min = boundary[0].v;
    double v_max = boundary[0].v;

    for (const auto& uv : boundary) {
        if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) {
            continue;
        }
        u_min = std::min(u_min, uv.u);
        u_max = std::max(u_max, uv.u);
        v_min = std::min(v_min, uv.v);
        v_max = std::max(v_max, uv.v);
    }

    const double center_u = (u_min + u_max) * 0.5;
    const double center_v = (v_min + v_max) * 0.5;
    const double half_extent_u = (u_max - u_min) * 0.5;
    const double half_extent_v = (v_max - v_min) * 0.5;
    const double target_half = std::max(half_extent_u, half_extent_v);
    if (target_half < 1e-9) {
        spdlog::warn("RingFill: UV extent too small");
        return path;
    }

    std::vector<double> layers;
    const bool outside_to_inside = (angle < 0.0);
    if (outside_to_inside) {
        for (double half = target_half; half >= spacing * 0.5; half -= spacing) {
            layers.push_back(half);
        }
    } else {
        for (double half = spacing * 0.5; half <= target_half + 1e-9; half += spacing) {
            layers.push_back(half);
        }
    }

    for (size_t layer = 0; layer < layers.size(); ++layer) {
        const double half = layers[layer];
        const double ru_min = std::max(center_u - half, u_min);
        const double ru_max = std::min(center_u + half, u_max);
        const double rv_min = std::max(center_v - half, v_min);
        const double rv_max = std::min(center_v + half, v_max);
        if (ru_max - ru_min < 1e-9 || rv_max - rv_min < 1e-9) {
            continue;
        }

        path.push_back(UVPathPoint{ru_min, rv_min, layer > 0, false});
        path.push_back(UVPathPoint{ru_max, rv_min, false, false});
        path.push_back(UVPathPoint{ru_max, rv_max, false, false});
        path.push_back(UVPathPoint{ru_min, rv_max, false, false});
        path.push_back(UVPathPoint{ru_min, rv_min, false, true});
    }

    spdlog::info("RingFill generated: {} layers, {} points", layers.size(), path.size());
    return path;
}

}  // namespace nbcam
