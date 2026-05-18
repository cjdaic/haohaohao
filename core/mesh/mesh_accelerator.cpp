#include "mesh_accelerator.h"
#include <spdlog/spdlog.h>
#include <CGAL/AABB_tree.h>
#include <iterator>
#include <limits>

namespace nbcam {

void MeshAccelerator::build(const TriangleMesh& mesh)
{
    clear();
    
    if (mesh.triangles.empty() || mesh.vertices.empty()) {
        spdlog::warn("网格为空，无法构建AABB树");
        return;
    }
    
    mesh_ = &mesh;
    
    // 转换三角形到CGAL格式
    triangles_.clear();
    triangles_.reserve(mesh.triangles.size());
    
    for (const auto& tri : mesh.triangles) {
        if (tri.v0 < mesh.vertices.size() && 
            tri.v1 < mesh.vertices.size() && 
            tri.v2 < mesh.vertices.size()) {
            const auto& v0 = mesh.vertices[tri.v0];
            const auto& v1 = mesh.vertices[tri.v1];
            const auto& v2 = mesh.vertices[tri.v2];
            
            Point_3 p0(v0.x, v0.y, v0.z);
            Point_3 p1(v1.x, v1.y, v1.z);
            Point_3 p2(v2.x, v2.y, v2.z);
            
            triangles_.emplace_back(p0, p1, p2);
        }
    }
    
    if (triangles_.empty()) {
        spdlog::warn("没有有效的三角形，无法构建AABB树");
        return;
    }
    
    // 构建AABB树
    tree_ = std::make_unique<Tree>(triangles_.begin(), triangles_.end());
    tree_->accelerate_distance_queries();
    
    spdlog::info("AABB树构建完成: {} 个三角形", triangles_.size());
}

void MeshAccelerator::clear()
{
    tree_.reset();
    triangles_.clear();
    mesh_ = nullptr;
}

int MeshAccelerator::rayQuery(const Point_3& origin, const Point_3& direction) const
{
    if (!isBuilt()) {
        return -1;
    }
    
    // 创建射线
    Ray_3 ray(origin, direction);
    
    // 执行射线查询
    // first_intersection 返回 std::optional<std::pair<Intersection, Iterator>>
    auto intersection = tree_->first_intersection(ray);
    
    if (intersection) {
        // intersection->second 在const函数中返回const_iterator
        // 使用const_iterator确保类型匹配
        std::vector<Triangle_3>::const_iterator begin_it = triangles_.cbegin();
        std::vector<Triangle_3>::const_iterator target_it = intersection->second;
        size_t idx = std::distance(begin_it, target_it);
        if (idx < triangles_.size() && mesh_) {
            return static_cast<int>(idx);
        }
    }
    
    return -1;
}

int MeshAccelerator::pointQuery(const Point_3& point) const
{
    if (!isBuilt()) {
        return -1;
    }
    
    // 使用AABB树的closest_point_and_primitive查找最近的primitive
    auto closest = tree_->closest_point_and_primitive(point);
    
    // 在const函数中使用const_iterator
    std::vector<Triangle_3>::const_iterator end_it = triangles_.cend();
    if (closest.second != end_it) {
        std::vector<Triangle_3>::const_iterator begin_it = triangles_.cbegin();
        std::vector<Triangle_3>::const_iterator target_it = closest.second;
        size_t idx = std::distance(begin_it, target_it);
        if (idx < triangles_.size()) {
            return static_cast<int>(idx);
        }
    }
    
    return -1;
}

} // namespace nbcam
