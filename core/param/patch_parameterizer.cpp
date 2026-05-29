#include "patch_parameterizer.h"
#include "abf_parameterizer.h"
#include "lscm_parameterizer.h"
#include "arap_parameterizer.h"
#include "authalic_parameterizer.h"
#include "cgal_mesh_utils.h"
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <algorithm>
#include <limits>

namespace nbcam {

PatchParameterizer::SubMeshResult PatchParameterizer::extractPatchSubmesh(
    const TriangleMesh& mesh, const Patch& patch)
{
    SubMeshResult result;
    result.submesh = std::make_unique<TriangleMesh>();
    
    // 收集patch使用的所有顶点
    std::unordered_map<size_t, size_t> vertex_map;  // 原始顶点索引 -> 子网格顶点索引
    std::vector<size_t> submesh_to_original_vec;
    
    for (size_t tri_idx : patch.triangle_indices) {
        if (tri_idx >= mesh.triangles.size()) continue;
        
        const auto& tri = mesh.triangles[tri_idx];
        
        // 添加顶点到映射
        for (size_t v_idx : {tri.v0, tri.v1, tri.v2}) {
            if (v_idx < mesh.vertices.size() && vertex_map.find(v_idx) == vertex_map.end()) {
                size_t new_idx = result.submesh->vertices.size();
                vertex_map[v_idx] = new_idx;
                submesh_to_original_vec.push_back(v_idx);
                
                // 添加顶点到子网格
                result.submesh->vertices.push_back(mesh.vertices[v_idx]);
            }
        }
    }
    
    // 创建原始到子网格的映射数组
    result.original_to_submesh.resize(mesh.vertices.size(), static_cast<size_t>(-1));
    for (const auto& [orig_idx, sub_idx] : vertex_map) {
        result.original_to_submesh[orig_idx] = sub_idx;
    }
    
    result.submesh_to_original = submesh_to_original_vec;
    
    // 添加三角形（使用子网格的顶点索引）
    for (size_t tri_idx : patch.triangle_indices) {
        if (tri_idx >= mesh.triangles.size()) continue;
        
        const auto& orig_tri = mesh.triangles[tri_idx];
        
        size_t v0_sub = result.original_to_submesh[orig_tri.v0];
        size_t v1_sub = result.original_to_submesh[orig_tri.v1];
        size_t v2_sub = result.original_to_submesh[orig_tri.v2];
        
        if (v0_sub != static_cast<size_t>(-1) && 
            v1_sub != static_cast<size_t>(-1) && 
            v2_sub != static_cast<size_t>(-1)) {
            Triangle sub_tri;
            sub_tri.v0 = v0_sub;
            sub_tri.v1 = v1_sub;
            sub_tri.v2 = v2_sub;
            result.submesh->triangles.push_back(sub_tri);
        }
    }
    
    spdlog::info("提取patch子网格: {} 顶点, {} 三角形", 
                 result.submesh->vertices.size(), result.submesh->triangles.size());
    
    return result;
}

std::vector<UVCoord> PatchParameterizer::mapUVToOriginalMesh(
    const TriangleMesh& original_mesh,
    const Patch& patch,
    const std::vector<UVCoord>& submesh_uv,
    const SubMeshResult& submesh_result)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<UVCoord> original_uv(original_mesh.vertices.size(), {nan, nan});
    
    // 将子网格的UV坐标映射回原始网格
    for (size_t i = 0; i < submesh_result.submesh_to_original.size(); ++i) {
        size_t orig_idx = submesh_result.submesh_to_original[i];
        if (orig_idx < original_uv.size() && i < submesh_uv.size()) {
            original_uv[orig_idx] = submesh_uv[i];
        }
    }
    
    return original_uv;
}

ParameterizationResult PatchParameterizer::parameterizePatch(
    const TriangleMesh& mesh, const Patch& patch, const std::string& algorithm)
{
    ParameterizationResult result;
    
    if (patch.triangle_indices.empty()) {
        spdlog::error("Patch为空，无法进行参数化");
        result.success = false;
        return result;
    }
    
    spdlog::info("开始对patch {} 进行参数化（算法: {}），包含 {} 个三角形", 
                 patch.id, algorithm, patch.triangle_indices.size());
    
    // 提取patch子网格
    SubMeshResult submesh_result = extractPatchSubmesh(mesh, patch);
    
    if (!submesh_result.submesh || submesh_result.submesh->triangles.empty()) {
        spdlog::error("无法提取patch子网格");
        result.success = false;
        return result;
    }
    
    // 根据算法选择参数化器
    ParameterizationResult submesh_result_param;
    if (algorithm == "ABF") {
        ABFParameterizer parameterizer;
        submesh_result_param = parameterizer.parameterize(*submesh_result.submesh);
    } else if (algorithm == "ARAP") {
        ARAPParameterizer parameterizer;
        submesh_result_param = parameterizer.parameterize(*submesh_result.submesh);
    } else if (algorithm == "AUTHALIC") {
        AuthalicParameterizer parameterizer;
        submesh_result_param = parameterizer.parameterize(*submesh_result.submesh);
    } else {
        // 默认使用LSCM
        LSCMParameterizer parameterizer;
        submesh_result_param = parameterizer.parameterize(*submesh_result.submesh);
    }
    
    if (!submesh_result_param.success) {
        spdlog::error("Patch子网格参数化失败（算法: {}）", algorithm);
        result.success = false;
        return result;
    }
    
    // 将UV坐标映射回原始网格
    result.uv_coords = mapUVToOriginalMesh(mesh, patch, 
                                           submesh_result_param.uv_coords, 
                                           submesh_result);
    result.island_count = submesh_result_param.island_count;
    result.success = true;
    
    spdlog::info("Patch参数化完成（算法: {}）", algorithm);
    
    return result;
}

} // namespace nbcam
