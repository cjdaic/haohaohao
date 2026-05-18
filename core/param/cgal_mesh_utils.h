#pragma once

#include "../mesh/mesh_io.h"
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/boost/graph/properties.h>
#include <spdlog/spdlog.h>
#include <map>

namespace nbcam {

// CGAL类型定义
typedef CGAL::Simple_cartesian<double> Kernel;
typedef Kernel::Point_3 Point_3;
typedef Kernel::Point_2 Point_2;
typedef CGAL::Surface_mesh<Point_3> Surface_mesh;
typedef boost::graph_traits<Surface_mesh>::vertex_descriptor vertex_descriptor;
typedef boost::graph_traits<Surface_mesh>::halfedge_descriptor halfedge_descriptor;
typedef boost::graph_traits<Surface_mesh>::face_descriptor face_descriptor;

// UV属性映射
typedef Surface_mesh::Property_map<vertex_descriptor, Point_2> UV_pmap;

// 将TriangleMesh转换为CGAL Surface_mesh
// 返回：CGAL顶点描述符到原始索引的映射
inline bool convertToCGALMesh(const TriangleMesh& mesh, Surface_mesh& cgal_mesh, 
                              std::map<vertex_descriptor, size_t>& cgal_to_original_index) {
    try {
        cgal_to_original_index.clear();
        
        // 添加顶点，并建立CGAL顶点到原始索引的映射
        std::vector<vertex_descriptor> vertex_map(mesh.vertices.size());
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const auto& v = mesh.vertices[i];
            vertex_descriptor vd = cgal_mesh.add_vertex(Point_3(v.x, v.y, v.z));
            vertex_map[i] = vd;
            cgal_to_original_index[vd] = i;  // 建立CGAL顶点到原始索引的映射
        }
        
        // 添加面
        for (const auto& tri : mesh.triangles) {
            if (tri.v0 < mesh.vertices.size() && 
                tri.v1 < mesh.vertices.size() && 
                tri.v2 < mesh.vertices.size()) {
                cgal_mesh.add_face(
                    vertex_map[tri.v0],
                    vertex_map[tri.v1],
                    vertex_map[tri.v2]
                );
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("转换到CGAL网格失败: {}", e.what());
        return false;
    }
}

// 重载版本：保持向后兼容
inline bool convertToCGALMesh(const TriangleMesh& mesh, Surface_mesh& cgal_mesh) {
    std::map<vertex_descriptor, size_t> dummy_map;
    return convertToCGALMesh(mesh, cgal_mesh, dummy_map);
}

} // namespace nbcam
