#pragma once

#include "mesh_io.h"

namespace nbcam {

// 网格预处理接口
class MeshPreprocessor {
public:
    MeshPreprocessor() = default;
    ~MeshPreprocessor() = default;
    
    // 修复网格（填充孔洞、移除重复顶点等）
    void repairMesh(TriangleMesh& mesh);
    
    // 统一法向量方向
    void unifyNormals(TriangleMesh& mesh);
    
    // 去噪
    void denoise(TriangleMesh& mesh);
    
    // 缩放
    void scale(TriangleMesh& mesh, double scale_factor);
    void scaleNonUniform(TriangleMesh& mesh, double sx, double sy, double sz);
    
    // 坐标系对齐（与振镜工作坐标一致）
    void alignCoordinateSystem(TriangleMesh& mesh);
    
    // 分区（如果需要）
    std::vector<TriangleMesh> partition(const TriangleMesh& mesh);
};

} // namespace nbcam
