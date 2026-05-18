#pragma once

#include "../mesh/mesh_io.h"
#include "../mesh/patch_clusterer.h"
#include "parameterizer_interface.h"
#include <memory>

namespace nbcam {

// Patch参数化器：对选中的patch进行UV参数化
class PatchParameterizer {
public:
    PatchParameterizer() = default;
    ~PatchParameterizer() = default;
    
    // 对指定的patch进行参数化
    // mesh: 原始网格
    // patch: 要参数化的patch
    // algorithm: 参数化算法 ("LSCM" / "ARAP" / "AUTHALIC")
    // 返回参数化结果，UV坐标索引对应patch子网格的顶点索引
    ParameterizationResult parameterizePatch(const TriangleMesh& mesh, const Patch& patch, const std::string& algorithm = "LSCM");
    
    // 提取patch子网格（包含patch的所有三角形及其顶点）
    // 返回子网格和从原始网格顶点索引到子网格顶点索引的映射
    struct SubMeshResult {
        std::unique_ptr<TriangleMesh> submesh;
        std::vector<size_t> original_to_submesh;  // 原始顶点索引 -> 子网格顶点索引
        std::vector<size_t> submesh_to_original;  // 子网格顶点索引 -> 原始顶点索引
    };
    SubMeshResult extractPatchSubmesh(const TriangleMesh& mesh, const Patch& patch);
    
    // 将子网格的UV坐标映射回原始网格
    // original_mesh: 原始网格
    // patch: patch信息
    // submesh_uv: 子网格的UV坐标（索引对应子网格顶点）
    // submesh_result: 子网格提取结果
    // 返回原始网格的UV坐标（只包含patch的顶点，其他顶点UV为(0,0)）
    std::vector<UVCoord> mapUVToOriginalMesh(
        const TriangleMesh& original_mesh,
        const Patch& patch,
        const std::vector<UVCoord>& submesh_uv,
        const SubMeshResult& submesh_result);
};

} // namespace nbcam
