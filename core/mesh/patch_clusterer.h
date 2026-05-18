#pragma once

#include "mesh_io.h"
#include <CGAL/Simple_cartesian.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits_3.h>
#include <CGAL/AABB_triangle_primitive_3.h>
#include <vector>
#include <unordered_map>
#include <memory>

namespace nbcam {

// Patch（面片）信息
struct Patch {
    int id;  // Patch ID
    std::vector<size_t> triangle_indices;  // 属于此patch的三角形索引
    double total_area;  // 总面积
    double normal_x, normal_y, normal_z;  // 面积加权平均法向（归一化）
    
    // 边界信息（用于高亮显示）
    std::vector<size_t> boundary_edges;  // 边界边（边的索引对）
};

// Patch聚类器：使用二面角对三角形进行聚类
class PatchClusterer {
public:
    PatchClusterer() = default;
    ~PatchClusterer() = default;
    
    // 对网格进行patch聚类
    // mesh: 输入网格
    // dihedral_threshold: 二面角阈值（弧度），小于此值的相邻三角形归为同一patch
    // 返回patch列表
    std::vector<Patch> clusterPatches(const TriangleMesh& mesh, double dihedral_threshold = 0.1);
    
    // 获取指定三角形的patch ID
    int getTrianglePatchId(size_t triangle_index) const;
    
    // 获取所有patch
    const std::vector<Patch>& getPatches() const { return patches_; }
    
    // 清除数据
    void clear();

private:
    // 计算三角形面积
    double calculateTriangleArea(const TriangleMesh& mesh, size_t tri_idx) const;
    
    // 计算三角形单位法向量
    void calculateTriangleNormal(const TriangleMesh& mesh, size_t tri_idx,
                                 double& nx, double& ny, double& nz) const;
    
    // 计算两个相邻三角形的二面角
    double calculateDihedralAngle(const TriangleMesh& mesh,
                                  size_t tri1_idx, size_t tri2_idx) const;
    
    // 查找相邻三角形
    std::vector<size_t> findAdjacentTriangles(const TriangleMesh& mesh, size_t tri_idx) const;
    
    // 使用BFS进行聚类（按照patch_logic.cpp的思路）
    void bfsClustering(const TriangleMesh& mesh, double dihedral_threshold);
    
    // 使用并查集进行聚类（保留作为备选）
    void unionFindClustering(const TriangleMesh& mesh, double dihedral_threshold);
    
    // 并查集查找父节点（需要修改parent_数组，所以不是const）
    int findParent(int x);
    
    // 计算patch的平均法向和总面积
    void computePatchProperties(const TriangleMesh& mesh);
    
    // 提取patch边界
    void extractPatchBoundaries(const TriangleMesh& mesh);
    
    std::vector<Patch> patches_;
    std::vector<int> triangle_to_patch_;  // 三角形索引到patch ID的映射
    std::vector<int> parent_;  // 并查集父节点
};

} // namespace nbcam
