#include "patch_clusterer.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <spdlog/spdlog.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 辅助函数：限制值在[-1, 1]范围内
static inline double clamp01(double x) { 
    return std::max(-1.0, std::min(1.0, x)); 
}

namespace nbcam {

double PatchClusterer::calculateTriangleArea(const TriangleMesh& mesh, size_t tri_idx) const
{
    if (tri_idx >= mesh.triangles.size()) {
        return 0.0;
    }
    
    const auto& tri = mesh.triangles[tri_idx];
    if (tri.v0 >= mesh.vertices.size() || tri.v1 >= mesh.vertices.size() || tri.v2 >= mesh.vertices.size()) {
        return 0.0;
    }
    
    const auto& v0 = mesh.vertices[tri.v0];
    const auto& v1 = mesh.vertices[tri.v1];
    const auto& v2 = mesh.vertices[tri.v2];
    
    // 计算两个边向量
    double e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
    double e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
    
    // 叉积
    double cross_x = e1y * e2z - e1z * e2y;
    double cross_y = e1z * e2x - e1x * e2z;
    double cross_z = e1x * e2y - e1y * e2x;
    
    // 面积 = 0.5 * |叉积|
    return 0.5 * std::sqrt(cross_x * cross_x + cross_y * cross_y + cross_z * cross_z);
}

void PatchClusterer::calculateTriangleNormal(const TriangleMesh& mesh, size_t tri_idx,
                                             double& nx, double& ny, double& nz) const
{
    if (tri_idx >= mesh.triangles.size()) {
        nx = ny = nz = 0.0;
        return;
    }
    
    const auto& tri = mesh.triangles[tri_idx];
    if (tri.v0 >= mesh.vertices.size() || tri.v1 >= mesh.vertices.size() || tri.v2 >= mesh.vertices.size()) {
        nx = ny = nz = 0.0;
        return;
    }
    
    const auto& v0 = mesh.vertices[tri.v0];
    const auto& v1 = mesh.vertices[tri.v1];
    const auto& v2 = mesh.vertices[tri.v2];
    
    // 计算两个边向量
    double e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
    double e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
    
    // 叉积得到法向量
    nx = e1y * e2z - e1z * e2y;
    ny = e1z * e2x - e1x * e2z;
    nz = e1x * e2y - e1y * e2x;
    
    // 归一化
    double len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 1e-10) {
        nx /= len;
        ny /= len;
        nz /= len;
    } else {
        nx = ny = 0.0;
        nz = 1.0;
    }
}

// 辅助函数：比较两个顶点是否相同（考虑浮点数误差）
static bool verticesEqual(const nbcam::Vertex& v1, const nbcam::Vertex& v2, double epsilon = 1e-6) {
    return std::abs(v1.x - v2.x) < epsilon &&
           std::abs(v1.y - v2.y) < epsilon &&
           std::abs(v1.z - v2.z) < epsilon;
}

std::vector<size_t> PatchClusterer::findAdjacentTriangles(const TriangleMesh& mesh, size_t tri_idx) const
{
    std::vector<size_t> adjacent;
    
    if (tri_idx >= mesh.triangles.size()) {
        return adjacent;
    }
    
    const auto& tri = mesh.triangles[tri_idx];
    
    // 获取当前三角形的三个顶点
    if (tri.v0 >= mesh.vertices.size() || tri.v1 >= mesh.vertices.size() || tri.v2 >= mesh.vertices.size()) {
        return adjacent;
    }
    
    const auto& v0 = mesh.vertices[tri.v0];
    const auto& v1 = mesh.vertices[tri.v1];
    const auto& v2 = mesh.vertices[tri.v2];
    
    // 查找共享边的三角形（通过比较顶点坐标，而不是索引）
    // 这样可以处理STL文件中顶点未去重的情况
    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        if (i == tri_idx) continue;
        
        const auto& other_tri = mesh.triangles[i];
        
        if (other_tri.v0 >= mesh.vertices.size() || 
            other_tri.v1 >= mesh.vertices.size() || 
            other_tri.v2 >= mesh.vertices.size()) {
            continue;
        }
        
        const auto& ov0 = mesh.vertices[other_tri.v0];
        const auto& ov1 = mesh.vertices[other_tri.v1];
        const auto& ov2 = mesh.vertices[other_tri.v2];
        
        // 检查是否共享边（共享两个顶点，通过坐标比较）
        int shared_vertices = 0;
        
        // 检查当前三角形的每个顶点是否与其他三角形的顶点相同
        if (verticesEqual(v0, ov0) || verticesEqual(v0, ov1) || verticesEqual(v0, ov2)) shared_vertices++;
        if (verticesEqual(v1, ov0) || verticesEqual(v1, ov1) || verticesEqual(v1, ov2)) shared_vertices++;
        if (verticesEqual(v2, ov0) || verticesEqual(v2, ov1) || verticesEqual(v2, ov2)) shared_vertices++;
        
        if (shared_vertices >= 2) {
            adjacent.push_back(i);
        }
    }
    
    return adjacent;
}

double PatchClusterer::calculateDihedralAngle(const TriangleMesh& mesh,
                                             size_t tri1_idx, size_t tri2_idx) const
{
    // 计算两个三角形的法向量
    double n1x, n1y, n1z, n2x, n2y, n2z;
    calculateTriangleNormal(mesh, tri1_idx, n1x, n1y, n1z);
    calculateTriangleNormal(mesh, tri2_idx, n2x, n2y, n2z);
    
    // 计算法向量点积
    double dot = n1x * n2x + n1y * n2y + n1z * n2z;
    
    // 归一化点积（虽然法向量已经归一化，但为了数值稳定性）
    double len1 = std::sqrt(n1x * n1x + n1y * n1y + n1z * n1z);
    double len2 = std::sqrt(n2x * n2x + n2y * n2y + n2z * n2z);
    double c = clamp01(dot / (len1 * len2 + 1e-30));
    
    // 如果绕序不一致，n2可能反向；"共面"仍然应视为同一面片，所以取abs
    c = std::abs(c);
    
    // 二面角 = arccos(|c|)
    // 如果二面角很小（接近0），说明两个三角形几乎共面
    return std::acos(c);
}

// 使用BFS进行patch聚类（按照patch_logic.cpp的思路）
void PatchClusterer::bfsClustering(const TriangleMesh& mesh, double dihedral_threshold)
{
    size_t num_triangles = mesh.triangles.size();
    triangle_to_patch_.clear();
    triangle_to_patch_.resize(num_triangles, -1);
    
    std::unordered_set<size_t> visited;
    visited.reserve(num_triangles);
    
    int next_patch_id = 0;
    
    // 对每个未访问的三角形，从它开始BFS形成一个patch
    for (size_t start_idx = 0; start_idx < num_triangles; ++start_idx) {
        if (visited.find(start_idx) != visited.end()) {
            continue;  // 已经访问过
        }
        
        // 开始新的patch，BFS遍历
        std::queue<size_t> q;
        q.push(start_idx);
        visited.insert(start_idx);
        triangle_to_patch_[start_idx] = next_patch_id;
        
        while (!q.empty()) {
            size_t cur_idx = q.front();
            q.pop();
            
            // 查找当前三角形的所有相邻三角形
            auto adjacent = findAdjacentTriangles(mesh, cur_idx);
            
            for (size_t adj_idx : adjacent) {
                // 如果相邻三角形未访问，检查二面角
                if (visited.find(adj_idx) == visited.end()) {
                    double dihedral = calculateDihedralAngle(mesh, cur_idx, adj_idx);
                    
                    // 如果二面角小于阈值，加入当前patch
                    if (dihedral < dihedral_threshold) {
                        visited.insert(adj_idx);
                        triangle_to_patch_[adj_idx] = next_patch_id;
                        q.push(adj_idx);  // 继续BFS
                    }
                }
            }
        }
        
        next_patch_id++;
    }
    
    spdlog::info("BFS聚类完成，产生 {} 个patch", next_patch_id);
}

// 保留并查集方法作为备选（已废弃，使用BFS）
int PatchClusterer::findParent(int x)
{
    if (parent_[x] != x) {
        parent_[x] = findParent(parent_[x]);  // 路径压缩
    }
    return parent_[x];
}

void PatchClusterer::unionFindClustering(const TriangleMesh& mesh, double dihedral_threshold)
{
    // 现在使用BFS方法，这个方法保留作为备选
    bfsClustering(mesh, dihedral_threshold);
}

void PatchClusterer::computePatchProperties(const TriangleMesh& mesh)
{
    // 统计每个patch的三角形
    std::unordered_map<int, std::vector<size_t>> patch_triangles;
    for (size_t i = 0; i < triangle_to_patch_.size(); ++i) {
        int patch_id = triangle_to_patch_[i];
        patch_triangles[patch_id].push_back(i);
    }
    
    // 创建patch并计算属性
    patches_.clear();
    patches_.reserve(patch_triangles.size());
    
    for (const auto& [patch_id, tri_indices] : patch_triangles) {
        Patch patch;
        patch.id = patch_id;
        patch.triangle_indices = tri_indices;
        
        // 计算总面积和面积加权平均法向
        double total_area = 0.0;
        double weighted_nx = 0.0, weighted_ny = 0.0, weighted_nz = 0.0;
        
        for (size_t tri_idx : tri_indices) {
            double area = calculateTriangleArea(mesh, tri_idx);
            total_area += area;
            
            double nx, ny, nz;
            calculateTriangleNormal(mesh, tri_idx, nx, ny, nz);
            
            weighted_nx += area * nx;
            weighted_ny += area * ny;
            weighted_nz += area * nz;
        }
        
        patch.total_area = total_area;
        
        // 归一化法向量
        double len = std::sqrt(weighted_nx * weighted_nx + weighted_ny * weighted_ny + weighted_nz * weighted_nz);
        if (len > 1e-10) {
            patch.normal_x = weighted_nx / len;
            patch.normal_y = weighted_ny / len;
            patch.normal_z = weighted_nz / len;
        } else {
            patch.normal_x = patch.normal_y = 0.0;
            patch.normal_z = 1.0;
        }
        
        patches_.push_back(patch);
    }
    
    // 按ID排序
    std::sort(patches_.begin(), patches_.end(),
              [](const Patch& a, const Patch& b) { return a.id < b.id; });
}

void PatchClusterer::extractPatchBoundaries(const TriangleMesh& mesh)
{
    // 提取每个patch的边界边
    for (auto& patch : patches_) {
        patch.boundary_edges.clear();
        
        // 统计每条边出现的次数
        // 使用简单的hash函数
        struct EdgeHash {
            std::size_t operator()(const std::pair<size_t, size_t>& p) const {
                return std::hash<size_t>()(p.first) ^ (std::hash<size_t>()(p.second) << 1);
            }
        };
        std::unordered_map<std::pair<size_t, size_t>, int, EdgeHash> edge_count;
        
        for (size_t tri_idx : patch.triangle_indices) {
            const auto& tri = mesh.triangles[tri_idx];
            
            // 添加三条边（确保顺序一致）
            std::vector<std::pair<size_t, size_t>> edges = {
                {std::min(tri.v0, tri.v1), std::max(tri.v0, tri.v1)},
                {std::min(tri.v1, tri.v2), std::max(tri.v1, tri.v2)},
                {std::min(tri.v2, tri.v0), std::max(tri.v2, tri.v0)}
            };
            
            for (const auto& edge : edges) {
                edge_count[edge]++;
            }
        }
        
        // 边界边是只出现一次的边
        for (const auto& [edge, count] : edge_count) {
            if (count == 1) {
                patch.boundary_edges.push_back(edge.first);
                patch.boundary_edges.push_back(edge.second);
            }
        }
    }
}

std::vector<Patch> PatchClusterer::clusterPatches(const TriangleMesh& mesh, double dihedral_threshold)
{
    clear();
    
    if (mesh.triangles.empty()) {
        spdlog::warn("网格为空，无法进行patch聚类");
        return patches_;
    }
    
    double threshold_deg = dihedral_threshold * 180.0 / M_PI;
    spdlog::info("开始patch聚类（BFS方法），三角形数量: {}, 二面角阈值: {:.3f} 弧度 ({:.1f}度)", 
                 mesh.triangles.size(), dihedral_threshold, threshold_deg);
    
    // 使用BFS进行聚类（按照patch_logic.cpp的思路）
    bfsClustering(mesh, dihedral_threshold);
    
    // 计算patch属性
    computePatchProperties(mesh);
    
    // 提取边界
    extractPatchBoundaries(mesh);
    
    spdlog::info("patch聚类完成，共 {} 个patch", patches_.size());
    
    return patches_;
}

int PatchClusterer::getTrianglePatchId(size_t triangle_index) const
{
    if (triangle_index < triangle_to_patch_.size()) {
        return triangle_to_patch_[triangle_index];
    }
    return -1;
}

void PatchClusterer::clear()
{
    patches_.clear();
    triangle_to_patch_.clear();
    parent_.clear();
}

} // namespace nbcam
