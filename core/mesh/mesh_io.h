#pragma once

#include <string>
#include <vector>
#include <memory>

namespace nbcam {

// 三角网格顶点
struct Vertex {
    double x, y, z;
    double nx, ny, nz;  // 法向量
};

// 三角面片
struct Triangle {
    size_t v0, v1, v2;  // 顶点索引
};

// 三角网格
class TriangleMesh {
public:
    TriangleMesh() = default;
    ~TriangleMesh() = default;
    
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    
    // 工具方法
    void clear();
    size_t getVertexCount() const { return vertices.size(); }
    size_t getTriangleCount() const { return triangles.size(); }
    bool isValid() const;
};

// 网格IO接口
class MeshIO {
public:
    MeshIO() = default;
    ~MeshIO() = default;
    
    // 从文件加载网格（OBJ/STL/SLDPRT）
    std::unique_ptr<TriangleMesh> loadFromFile(const std::string& filepath);
    
    // 保存网格到文件
    bool saveToFile(const TriangleMesh& mesh, const std::string& filepath);
    
    // 从OBJ格式加载
    std::unique_ptr<TriangleMesh> loadOBJ(const std::string& filepath);
    
    // 从STL格式加载
    std::unique_ptr<TriangleMesh> loadSTL(const std::string& filepath);

    // 从SolidWorks零件格式加载（Windows上通过本机SolidWorks导出临时STL）
    std::unique_ptr<TriangleMesh> loadSLDPRT(const std::string& filepath);
    
    // 保存为OBJ格式
    bool saveOBJ(const TriangleMesh& mesh, const std::string& filepath);
    
    // 保存为STL格式
    bool saveSTL(const TriangleMesh& mesh, const std::string& filepath);
};

} // namespace nbcam
