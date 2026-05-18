#include "authalic_parameterizer.h"
#include "cgal_mesh_utils.h"
#include <spdlog/spdlog.h>
#include <CGAL/Surface_mesh_parameterization/Discrete_authalic_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Mean_value_coordinates_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/parameterize.h>
#include <CGAL/boost/graph/properties.h>
#include <vector>
#include <limits>
#include <cmath>

namespace nbcam {

namespace {

bool extractUVFromMap(Surface_mesh& mesh,
                      UV_pmap uv_map,
                      ParameterizationResult& result,
                      const std::map<vertex_descriptor, size_t>& cgal_to_original_index,
                      const char* tag)
{
    result.uv_coords.clear();
    result.uv_coords.resize(mesh.number_of_vertices(), {0.0, 0.0});

    std::vector<bool> valid_uv(mesh.number_of_vertices(), false);
    double u_min = std::numeric_limits<double>::max();
    double u_max = std::numeric_limits<double>::lowest();
    double v_min = std::numeric_limits<double>::max();
    double v_max = std::numeric_limits<double>::lowest();

    for (auto v : vertices(mesh)) {
        const auto it = cgal_to_original_index.find(v);
        if (it == cgal_to_original_index.end()) {
            spdlog::warn("{}: 无法找到CGAL顶点的原始索引，跳过该顶点", tag);
            continue;
        }

        const size_t idx = it->second;
        if (idx >= result.uv_coords.size()) {
            spdlog::warn("{}: 顶点索引 {} 超出范围 (总数: {})", tag, idx, result.uv_coords.size());
            continue;
        }

        const Point_2& uv = uv_map[v];
        const double u = uv.x();
        const double vv = uv.y();

        if (std::isfinite(u) && std::isfinite(vv)) {
            result.uv_coords[idx].u = u;
            result.uv_coords[idx].v = vv;
            valid_uv[idx] = true;
            u_min = std::min(u_min, u);
            u_max = std::max(u_max, u);
            v_min = std::min(v_min, vv);
            v_max = std::max(v_max, vv);
        } else {
            spdlog::warn("{}: 顶点 {} 产生无效UV坐标 ({}, {})", tag, idx, u, vv);
        }
    }

    size_t invalid_count = 0;
    for (size_t i = 0; i < valid_uv.size(); ++i) {
        if (!valid_uv[i]) {
            ++invalid_count;
        }
    }

    if (invalid_count > 0) {
        spdlog::error("{}: 发现 {} 个无效UV坐标", tag, invalid_count);
        if (u_max > u_min && v_max > v_min) {
            for (size_t i = 0; i < valid_uv.size(); ++i) {
                if (!valid_uv[i]) {
                    result.uv_coords[i].u = (u_min + u_max) * 0.5;
                    result.uv_coords[i].v = (v_min + v_max) * 0.5;
                }
            }
        }
    }

    result.island_count = 1;
    result.success = true;
    return true;
}

bool parameterizeWithAuthalic(Surface_mesh& mesh,
                              UV_pmap uv_map,
                              ParameterizationResult& result,
                              const std::map<vertex_descriptor, size_t>& cgal_to_original_index)
{
    halfedge_descriptor bhd = boost::graph_traits<Surface_mesh>::null_halfedge();
    for (auto h : halfedges(mesh)) {
        if (CGAL::is_border(h, mesh)) {
            bhd = h;
            break;
        }
    }

    if (bhd == boost::graph_traits<Surface_mesh>::null_halfedge()) {
        spdlog::error("AUTHALIC参数化失败：无法找到网格边界");
        return false;
    }

    CGAL::Surface_mesh_parameterization::Discrete_authalic_parameterizer_3<Surface_mesh> authalic_parameterizer;
    CGAL::Surface_mesh_parameterization::Error_code err =
        CGAL::Surface_mesh_parameterization::parameterize(mesh, authalic_parameterizer, bhd, uv_map);
    if (err == CGAL::Surface_mesh_parameterization::OK) {
        return extractUVFromMap(mesh, uv_map, result, cgal_to_original_index, "AUTHALIC参数化");
    }

    spdlog::warn("AUTHALIC参数化失败，错误代码: {}，尝试Mean Value Coordinates回退", static_cast<int>(err));
    CGAL::Surface_mesh_parameterization::Mean_value_coordinates_parameterizer_3<Surface_mesh> mvc_parameterizer;
    err = CGAL::Surface_mesh_parameterization::parameterize(mesh, mvc_parameterizer, bhd, uv_map);
    if (err != CGAL::Surface_mesh_parameterization::OK) {
        spdlog::error("AUTHALIC参数化回退失败，错误代码: {}", static_cast<int>(err));
        return false;
    }

    return extractUVFromMap(mesh, uv_map, result, cgal_to_original_index, "AUTHALIC参数化(Mean Value回退)");
}

} // namespace

ParameterizationResult AuthalicParameterizer::parameterize(const TriangleMesh& mesh)
{
    ParameterizationResult result;

    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        spdlog::error("网格为空，无法进行AUTHALIC参数化");
        result.success = false;
        return result;
    }

    spdlog::info("开始AUTHALIC参数化: {} 顶点, {} 三角面", mesh.vertices.size(), mesh.triangles.size());

    try {
        Surface_mesh cgal_mesh;
        std::map<vertex_descriptor, size_t> cgal_to_original_index;
        if (!convertToCGALMesh(mesh, cgal_mesh, cgal_to_original_index)) {
            result.success = false;
            return result;
        }

        if (cgal_mesh.number_of_vertices() == 0 || cgal_mesh.number_of_faces() == 0) {
            spdlog::error("CGAL网格转换后为空");
            result.success = false;
            return result;
        }

        UV_pmap uv_map = cgal_mesh.add_property_map<vertex_descriptor, Point_2>("h:uv").first;
        result.success = parameterizeWithAuthalic(cgal_mesh, uv_map, result, cgal_to_original_index);

        if (result.success) {
            spdlog::info("AUTHALIC参数化完成: {} 顶点, {} 个UV岛", result.uv_coords.size(), result.island_count);
        }
    } catch (const std::exception& e) {
        spdlog::error("AUTHALIC参数化异常: {}", e.what());
        result.success = false;
    }

    return result;
}

} // namespace nbcam

