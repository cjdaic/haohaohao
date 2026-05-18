#pragma once

#include "mesh_io.h"
#include <CGAL/Simple_cartesian.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits_3.h>
#include <CGAL/AABB_triangle_primitive_3.h>
#include <memory>
#include <vector>

namespace nbcam {

// CGAL类型定义
typedef CGAL::Simple_cartesian<double> Kernel;
typedef Kernel::Point_3 Point_3;
typedef Kernel::Ray_3 Ray_3;
typedef Kernel::Triangle_3 Triangle_3;
typedef std::vector<Triangle_3>::iterator Iterator;
typedef CGAL::AABB_triangle_primitive_3<Kernel, Iterator> Primitive;
typedef CGAL::AABB_traits_3<Kernel, Primitive> AABB_traits;
typedef CGAL::AABB_tree<AABB_traits> Tree;

// 网格加速结构：使用CGAL AABB树进行快速三角形查询
class MeshAccelerator {
public:
    MeshAccelerator() = default;
    ~MeshAccelerator() = default;
    
    // 构建AABB树
    void build(const TriangleMesh& mesh);
    
    // 清除数据
    void clear();
    
    // 射线查询：返回第一个相交的三角形索引，如果没有相交返回-1
    // origin: 射线起点
    // direction: 射线方向（归一化）
    int rayQuery(const Point_3& origin, const Point_3& direction) const;
    
    // 点查询：返回最近的三角形索引
    int pointQuery(const Point_3& point) const;
    
    // 检查是否已构建
    bool isBuilt() const { return tree_ != nullptr && !triangles_.empty(); }

private:
    std::unique_ptr<Tree> tree_;
    std::vector<Triangle_3> triangles_;
    const TriangleMesh* mesh_ = nullptr;  // 非拥有指针，用于索引映射
};

} // namespace nbcam
