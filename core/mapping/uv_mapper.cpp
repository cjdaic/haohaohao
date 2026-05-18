#include "uv_mapper.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

double pointToLineDistance(double px, double py, double x1, double y1, double x2, double y2)
{
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    const double len_sq = dx * dx + dy * dy;

    if (len_sq < 1e-12) {
        const double dx_p = px - x1;
        const double dy_p = py - y1;
        return std::sqrt(dx_p * dx_p + dy_p * dy_p);
    }

    const double t = std::max(0.0, std::min(1.0, ((px - x1) * dx + (py - y1) * dy) / len_sq));
    const double proj_x = x1 + t * dx;
    const double proj_y = y1 + t * dy;

    const double dx_p = px - proj_x;
    const double dy_p = py - proj_y;
    return std::sqrt(dx_p * dx_p + dy_p * dy_p);
}

}  // namespace

namespace nbcam {

constexpr int MIN_TRIANGLES_FOR_GRID = 128;

static size_t findTriangleLinear(const UVPathPoint& uv_point,
                                 const std::vector<UVCoord>& uv_coords,
                                 const TriangleMesh& mesh,
                                 const std::vector<size_t>& search_list,
                                 double& out_min_distance)
{
    out_min_distance = std::numeric_limits<double>::max();
    size_t best_triangle = mesh.triangles.size();

    for (size_t i : search_list) {
        if (i >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[i];
        if (tri.v0 >= uv_coords.size() || tri.v1 >= uv_coords.size() || tri.v2 >= uv_coords.size()) {
            continue;
        }

        const auto& uv0 = uv_coords[tri.v0];
        const auto& uv1 = uv_coords[tri.v1];
        const auto& uv2 = uv_coords[tri.v2];
        if (std::isnan(uv0.u) || std::isnan(uv0.v) ||
            std::isnan(uv1.u) || std::isnan(uv1.v) ||
            std::isnan(uv2.u) || std::isnan(uv2.v)) {
            continue;
        }

        const double denom = (uv1.v - uv2.v) * (uv0.u - uv2.u) + (uv2.u - uv1.u) * (uv0.v - uv2.v);
        if (std::abs(denom) < 1e-9) {
            continue;
        }

        const double a = ((uv1.v - uv2.v) * (uv_point.u - uv2.u) + (uv2.u - uv1.u) * (uv_point.v - uv2.v)) / denom;
        const double b = ((uv2.v - uv0.v) * (uv_point.u - uv2.u) + (uv0.u - uv2.u) * (uv_point.v - uv2.v)) / denom;
        const double c = 1.0 - a - b;

        if (a >= -1e-6 && b >= -1e-6 && c >= -1e-6) {
            out_min_distance = 0.0;
            return i;
        }

        const double d1 = pointToLineDistance(uv_point.u, uv_point.v, uv0.u, uv0.v, uv1.u, uv1.v);
        const double d2 = pointToLineDistance(uv_point.u, uv_point.v, uv1.u, uv1.v, uv2.u, uv2.v);
        const double d3 = pointToLineDistance(uv_point.u, uv_point.v, uv2.u, uv2.v, uv0.u, uv0.v);
        const double dist = std::min({d1, d2, d3});
        if (dist < out_min_distance) {
            out_min_distance = dist;
            best_triangle = i;
        }
    }

    // 边界附近的浮点误差较明显，适当放宽容差避免丢点。
    const double tolerance = 0.1;
    if (best_triangle < mesh.triangles.size() && out_min_distance < tolerance) {
        return best_triangle;
    }
    return mesh.triangles.size();
}

std::vector<PathPoint> UVMapper::mapUVToXYZ(const std::vector<UVPathPoint>& uv_path,
                                            const TriangleMesh& mesh,
                                            const std::vector<UVCoord>& uv_coords,
                                            const std::vector<size_t>* triangle_indices)
{
    std::vector<PathPoint> xyz_path;
    xyz_path.reserve(uv_path.size());

    std::vector<size_t> search_list;
    if (triangle_indices && !triangle_indices->empty()) {
        search_list = *triangle_indices;
    } else {
        search_list.reserve(mesh.triangles.size());
        for (size_t i = 0; i < mesh.triangles.size(); ++i) {
            search_list.push_back(i);
        }
    }

    UVGrid grid;
    const bool use_grid = (search_list.size() >= static_cast<size_t>(MIN_TRIANGLES_FOR_GRID));
    if (use_grid) {
        grid = buildUVGrid(mesh, uv_coords, search_list);
    }

    for (const auto& uv_point : uv_path) {
        PathPoint point;
        point.u = uv_point.u;
        point.v = uv_point.v;

        size_t tri_idx = mesh.triangles.size();
        if (use_grid) {
            tri_idx = findTriangleWithGrid(uv_point, grid, uv_coords, mesh);
            if (tri_idx >= mesh.triangles.size()) {
                double dum;
                tri_idx = findTriangleLinear(uv_point, uv_coords, mesh, search_list, dum);
            }
        } else {
            double dum;
            tri_idx = findTriangleLinear(uv_point, uv_coords, mesh, search_list, dum);
        }

        if (tri_idx < mesh.triangles.size()) {
            interpolateXYZ(uv_point, tri_idx, uv_coords, mesh, point.x, point.y, point.z);
            point.laser = 1;
        } else {
            spdlog::warn("UV point ({}, {}) has no matching triangle", uv_point.u, uv_point.v);
            point.x = point.y = point.z = 0.0;
            point.laser = 0;
        }

        xyz_path.push_back(std::move(point));
    }

    return xyz_path;
}

UVGrid UVMapper::buildUVGrid(const TriangleMesh& mesh,
                             const std::vector<UVCoord>& uv_coords,
                             const std::vector<size_t>& triangle_indices)
{
    UVGrid grid;
    grid.grid_nu = 64;
    grid.grid_nv = 64;
    grid.u_min = grid.u_max = 0.0;
    grid.v_min = grid.v_max = 0.0;

    bool first = true;
    for (size_t ti : triangle_indices) {
        if (ti >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[ti];
        if (tri.v0 >= uv_coords.size() || tri.v1 >= uv_coords.size() || tri.v2 >= uv_coords.size()) {
            continue;
        }
        for (size_t vi : {tri.v0, tri.v1, tri.v2}) {
            const auto& uv = uv_coords[vi];
            if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) {
                continue;
            }
            if (first) {
                grid.u_min = grid.u_max = uv.u;
                grid.v_min = grid.v_max = uv.v;
                first = false;
            } else {
                grid.u_min = std::min(grid.u_min, uv.u);
                grid.u_max = std::max(grid.u_max, uv.u);
                grid.v_min = std::min(grid.v_min, uv.v);
                grid.v_max = std::max(grid.v_max, uv.v);
            }
        }
    }

    const double eps = 1e-6;
    if (grid.u_max - grid.u_min < eps) {
        grid.u_max = grid.u_min + eps;
    }
    if (grid.v_max - grid.v_min < eps) {
        grid.v_max = grid.v_min + eps;
    }

    grid.cells.resize(grid.grid_nu * grid.grid_nv);

    for (size_t ti : triangle_indices) {
        if (ti >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[ti];
        if (tri.v0 >= uv_coords.size() || tri.v1 >= uv_coords.size() || tri.v2 >= uv_coords.size()) {
            continue;
        }
        const auto& uv0 = uv_coords[tri.v0];
        const auto& uv1 = uv_coords[tri.v1];
        const auto& uv2 = uv_coords[tri.v2];
        const double tu_min = std::min({uv0.u, uv1.u, uv2.u});
        const double tu_max = std::max({uv0.u, uv1.u, uv2.u});
        const double tv_min = std::min({uv0.v, uv1.v, uv2.v});
        const double tv_max = std::max({uv0.v, uv1.v, uv2.v});

        int cx0 = static_cast<int>((tu_min - grid.u_min) / (grid.u_max - grid.u_min) * grid.grid_nu);
        int cx1 = static_cast<int>((tu_max - grid.u_min) / (grid.u_max - grid.u_min) * grid.grid_nu);
        int cy0 = static_cast<int>((tv_min - grid.v_min) / (grid.v_max - grid.v_min) * grid.grid_nv);
        int cy1 = static_cast<int>((tv_max - grid.v_min) / (grid.v_max - grid.v_min) * grid.grid_nv);
        cx0 = std::max(0, std::min(cx0, grid.grid_nu - 1));
        cx1 = std::max(0, std::min(cx1, grid.grid_nu - 1));
        cy0 = std::max(0, std::min(cy0, grid.grid_nv - 1));
        cy1 = std::max(0, std::min(cy1, grid.grid_nv - 1));

        for (int cx = cx0; cx <= cx1; ++cx) {
            for (int cy = cy0; cy <= cy1; ++cy) {
                const int idx = cy * grid.grid_nu + cx;
                grid.cells[idx].push_back(ti);
            }
        }
    }

    return grid;
}

size_t UVMapper::findTriangleWithGrid(const UVPathPoint& uv_point,
                                      const UVGrid& grid,
                                      const std::vector<UVCoord>& uv_coords,
                                      const TriangleMesh& mesh)
{
    if (std::isnan(uv_point.u) || std::isnan(uv_point.v) ||
        !std::isfinite(uv_point.u) || !std::isfinite(uv_point.v)) {
        return mesh.triangles.size();
    }

    int cx = static_cast<int>((uv_point.u - grid.u_min) / (grid.u_max - grid.u_min) * grid.grid_nu);
    int cy = static_cast<int>((uv_point.v - grid.v_min) / (grid.v_max - grid.v_min) * grid.grid_nv);
    cx = std::max(0, std::min(cx, grid.grid_nu - 1));
    cy = std::max(0, std::min(cy, grid.grid_nv - 1));
    const int cell_idx = cy * grid.grid_nu + cx;

    if (cell_idx < 0 || cell_idx >= static_cast<int>(grid.cells.size())) {
        return mesh.triangles.size();
    }

    const auto& cands = grid.cells[cell_idx];
    size_t best = mesh.triangles.size();
    double min_dist = std::numeric_limits<double>::max();

    for (size_t i : cands) {
        if (i >= mesh.triangles.size()) {
            continue;
        }
        const auto& tri = mesh.triangles[i];
        if (tri.v0 >= uv_coords.size() || tri.v1 >= uv_coords.size() || tri.v2 >= uv_coords.size()) {
            continue;
        }

        const auto& uv0 = uv_coords[tri.v0];
        const auto& uv1 = uv_coords[tri.v1];
        const auto& uv2 = uv_coords[tri.v2];

        const double denom = (uv1.v - uv2.v) * (uv0.u - uv2.u) + (uv2.u - uv1.u) * (uv0.v - uv2.v);
        if (std::abs(denom) < 1e-9) {
            continue;
        }

        const double a = ((uv1.v - uv2.v) * (uv_point.u - uv2.u) + (uv2.u - uv1.u) * (uv_point.v - uv2.v)) / denom;
        const double b = ((uv2.v - uv0.v) * (uv_point.u - uv2.u) + (uv0.u - uv2.u) * (uv_point.v - uv2.v)) / denom;
        const double c = 1.0 - a - b;

        if (a >= -1e-6 && b >= -1e-6 && c >= -1e-6) {
            return i;
        }

        const double d1 = pointToLineDistance(uv_point.u, uv_point.v, uv0.u, uv0.v, uv1.u, uv1.v);
        const double d2 = pointToLineDistance(uv_point.u, uv_point.v, uv1.u, uv1.v, uv2.u, uv2.v);
        const double d3 = pointToLineDistance(uv_point.u, uv_point.v, uv2.u, uv2.v, uv0.u, uv0.v);
        const double dist = std::min({d1, d2, d3});
        if (dist < min_dist) {
            min_dist = dist;
            best = i;
        }
    }

    // 边界附近的浮点误差较明显，适当放宽容差避免丢点。
    const double tolerance = 0.1;
    if (best < mesh.triangles.size() && min_dist < tolerance) {
        return best;
    }
    return mesh.triangles.size();
}

size_t UVMapper::findTriangle(const UVPathPoint& uv_point,
                              const std::vector<UVCoord>& uv_coords,
                              const TriangleMesh& mesh,
                              const std::vector<size_t>* triangle_indices) const
{
    std::vector<size_t> search_list;
    if (triangle_indices && !triangle_indices->empty()) {
        search_list = *triangle_indices;
    } else {
        for (size_t i = 0; i < mesh.triangles.size(); ++i) {
            search_list.push_back(i);
        }
    }

    if (std::isnan(uv_point.u) || std::isnan(uv_point.v) ||
        !std::isfinite(uv_point.u) || !std::isfinite(uv_point.v)) {
        return mesh.triangles.size();
    }

    double dum;
    return findTriangleLinear(uv_point, uv_coords, mesh, search_list, dum);
}

void UVMapper::interpolateXYZ(const UVPathPoint& uv_point,
                              size_t triangle_index,
                              const std::vector<UVCoord>& uv_coords,
                              const TriangleMesh& mesh,
                              double& x,
                              double& y,
                              double& z) const
{
    if (triangle_index >= mesh.triangles.size()) {
        x = y = z = 0.0;
        return;
    }

    const auto& tri = mesh.triangles[triangle_index];
    if (tri.v0 >= uv_coords.size() || tri.v1 >= uv_coords.size() || tri.v2 >= uv_coords.size() ||
        tri.v0 >= mesh.vertices.size() || tri.v1 >= mesh.vertices.size() || tri.v2 >= mesh.vertices.size()) {
        x = y = z = 0.0;
        return;
    }

    const auto& uv0 = uv_coords[tri.v0];
    const auto& uv1 = uv_coords[tri.v1];
    const auto& uv2 = uv_coords[tri.v2];

    const double denom = (uv1.v - uv2.v) * (uv0.u - uv2.u) + (uv2.u - uv1.u) * (uv0.v - uv2.v);
    if (std::abs(denom) < 1e-9) {
        x = y = z = 0.0;
        return;
    }

    const double a = ((uv1.v - uv2.v) * (uv_point.u - uv2.u) + (uv2.u - uv1.u) * (uv_point.v - uv2.v)) / denom;
    const double b = ((uv2.v - uv0.v) * (uv_point.u - uv2.u) + (uv0.u - uv2.u) * (uv_point.v - uv2.v)) / denom;
    const double c = 1.0 - a - b;

    const auto& v0 = mesh.vertices[tri.v0];
    const auto& v1 = mesh.vertices[tri.v1];
    const auto& v2 = mesh.vertices[tri.v2];

    x = a * v0.x + b * v1.x + c * v2.x;
    y = a * v0.y + b * v1.y + c * v2.y;
    z = a * v0.z + b * v1.z + c * v2.z;
}

}  // namespace nbcam
