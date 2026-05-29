#include "abf_parameterizer.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cmath>
#include <limits>

#include <Eigen/SparseLU>

#include "../../third_party/openabf/OpenABF.hpp"

namespace nbcam {

ParameterizationResult ABFParameterizer::parameterize(const TriangleMesh& mesh)
{
    ParameterizationResult result;

    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        spdlog::error("网格为空，无法进行ABF参数化");
        result.success = false;
        return result;
    }

    spdlog::info("开始ABF参数化: {} 顶点, {} 三角面", mesh.vertices.size(), mesh.triangles.size());

    try {
        using AbfMesh = OpenABF::detail::ABF::Mesh<double>;
        auto abf_mesh = AbfMesh::New();

        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const auto& src = mesh.vertices[i];
            if (!std::isfinite(src.x) || !std::isfinite(src.y) || !std::isfinite(src.z)) {
                spdlog::error("ABF参数化失败：顶点 {} 坐标无效", i);
                result.success = false;
                return result;
            }
            abf_mesh->insert_vertex(src.x, src.y, src.z);
        }

        std::vector<std::array<size_t, 3>> faces;
        faces.reserve(mesh.triangles.size());
        for (const auto& tri : mesh.triangles) {
            if (tri.v0 >= mesh.vertices.size() ||
                tri.v1 >= mesh.vertices.size() ||
                tri.v2 >= mesh.vertices.size()) {
                continue;
            }
            faces.push_back({tri.v0, tri.v1, tri.v2});
        }

        if (faces.empty()) {
            spdlog::error("ABF参数化失败：没有有效三角形");
            result.success = false;
            return result;
        }

        abf_mesh->insert_faces(faces);

        if (abf_mesh->num_vertices() < 3 || abf_mesh->num_faces() == 0) {
            spdlog::error("ABF参数化失败：OpenABF网格为空");
            result.success = false;
            return result;
        }

        OpenABF::ABFPlusPlus<double> abf;
        abf.setMaxIterations(20);
        abf.setGradientThreshold(0.001);
        abf.compute(abf_mesh);

        OpenABF::AngleBasedLSCM<double, AbfMesh> lscm;
        lscm.compute(abf_mesh);

        result.uv_coords.resize(mesh.vertices.size(), {0.0, 0.0});
        for (const auto& v : abf_mesh->vertices()) {
            if (!v) {
                continue;
            }
            const size_t idx = v->idx;
            if (idx >= result.uv_coords.size()) {
                continue;
            }
            if (std::isfinite(v->pos[0]) && std::isfinite(v->pos[1])) {
                result.uv_coords[idx].u = static_cast<double>(v->pos[0]);
                result.uv_coords[idx].v = static_cast<double>(v->pos[1]);
            } else {
                result.uv_coords[idx].u = std::numeric_limits<double>::quiet_NaN();
                result.uv_coords[idx].v = std::numeric_limits<double>::quiet_NaN();
            }
        }

        size_t finite_count = 0;
        for (const auto& uv : result.uv_coords) {
            if (std::isfinite(uv.u) && std::isfinite(uv.v)) {
                ++finite_count;
            }
        }

        if (finite_count == 0) {
            spdlog::error("ABF参数化失败：没有生成有效UV");
            result.success = false;
            return result;
        }

        result.island_count = static_cast<int>(abf_mesh->boundaries().size());
        if (result.island_count <= 0) {
            result.island_count = 1;
        }
        result.success = true;
        spdlog::info("ABF参数化完成: {} 顶点, {} 个UV岛", result.uv_coords.size(), result.island_count);
    } catch (const OpenABF::SolverException& e) {
        spdlog::error("ABF参数化求解失败: {}", e.what());
        result.success = false;
    } catch (const OpenABF::MeshException& e) {
        spdlog::error("ABF参数化网格失败: {}", e.what());
        result.success = false;
    } catch (const std::exception& e) {
        spdlog::error("ABF参数化异常: {}", e.what());
        result.success = false;
    }

    return result;
}

} // namespace nbcam
