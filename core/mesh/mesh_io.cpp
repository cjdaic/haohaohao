#include "mesh_io.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <iomanip>
#include <cstdint>
#include <cmath>

namespace nbcam {

void TriangleMesh::clear() {
    vertices.clear();
    triangles.clear();
}

bool TriangleMesh::isValid() const {
    if (vertices.empty() || triangles.empty()) {
        return false;
    }
    
    // 检查索引有效性
    size_t max_index = vertices.size() - 1;
    for (const auto& tri : triangles) {
        if (tri.v0 > max_index || tri.v1 > max_index || tri.v2 > max_index) {
            return false;
        }
    }
    
    return true;
}

std::unique_ptr<TriangleMesh> MeshIO::loadFromFile(const std::string& filepath) {
    std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "obj") {
        return loadOBJ(filepath);
    } else if (ext == "stl") {
        return loadSTL(filepath);
    } else {
        spdlog::error("不支持的文件格式: {}", ext);
        return nullptr;
    }
}

bool MeshIO::saveToFile(const TriangleMesh& mesh, const std::string& filepath) {
    std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "obj") {
        return saveOBJ(mesh, filepath);
    } else if (ext == "stl") {
        return saveSTL(mesh, filepath);
    } else {
        spdlog::error("不支持的文件格式: {}", ext);
        return false;
    }
}

std::unique_ptr<TriangleMesh> MeshIO::loadOBJ(const std::string& filepath) {
    auto mesh = std::make_unique<TriangleMesh>();
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::error("无法打开文件: {}", filepath);
        return nullptr;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        
        if (type == "v") {
            Vertex v{};
            iss >> v.x >> v.y >> v.z;
            v.nx = v.ny = v.nz = 0.0;
            mesh->vertices.push_back(v);
        } else if (type == "vn") {
            if (!mesh->vertices.empty()) {
                auto& v = mesh->vertices.back();
                iss >> v.nx >> v.ny >> v.nz;
            }
        } else if (type == "f") {
            Triangle tri{};
            std::string v0, v1, v2;
            iss >> v0 >> v1 >> v2;
            
            // 解析顶点索引（OBJ格式从1开始）
            tri.v0 = std::stoul(v0.substr(0, v0.find('/'))) - 1;
            tri.v1 = std::stoul(v1.substr(0, v1.find('/'))) - 1;
            tri.v2 = std::stoul(v2.substr(0, v2.find('/'))) - 1;
            
            mesh->triangles.push_back(tri);
        }
    }
    
    file.close();
    spdlog::info("加载OBJ文件成功: {} 顶点, {} 三角面", 
                 mesh->vertices.size(), mesh->triangles.size());
    return mesh;
}

std::unique_ptr<TriangleMesh> MeshIO::loadSTL(const std::string& filepath) {
    auto mesh = std::make_unique<TriangleMesh>();
    
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("无法打开STL文件: {}", filepath);
        return nullptr;
    }
    
    // 读取文件头（80字节）
    char header[80];
    file.read(header, 80);
    
    // 读取三角形数量（4字节）
    uint32_t num_triangles = 0;
    file.read(reinterpret_cast<char*>(&num_triangles), sizeof(uint32_t));
    
    // 检查是否为ASCII格式（如果前5个字符是"solid"，可能是ASCII格式）
    file.seekg(0);
    std::string first_line;
    std::getline(file, first_line);
    file.seekg(0);
    
    bool is_ascii = false;
    if (first_line.find("solid") == 0 || first_line.find("SOLID") == 0) {
        // 尝试ASCII格式
        is_ascii = true;
        file.close();
        file.open(filepath, std::ios::in);
        
        std::string line;
        std::string token;
        std::map<std::string, size_t> vertex_map;  // 用于去重顶点
        size_t vertex_index = 0;
        
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            iss >> token;
            
            if (token == "vertex") {
                double x, y, z;
                iss >> x >> y >> z;
                
                // 创建顶点键用于去重
                std::ostringstream vertex_key;
                vertex_key << std::fixed << std::setprecision(6) << x << "," << y << "," << z;
                std::string key = vertex_key.str();
                
                // 检查顶点是否已存在
                auto it = vertex_map.find(key);
                size_t idx;
                if (it == vertex_map.end()) {
                    Vertex v{};
                    v.x = x;
                    v.y = y;
                    v.z = z;
                    v.nx = v.ny = v.nz = 0.0;
                    mesh->vertices.push_back(v);
                    vertex_map[key] = vertex_index;
                    idx = vertex_index;
                    vertex_index++;
                } else {
                    idx = it->second;
                }
                
                // 如果当前三角形未完成，添加顶点索引
                const size_t INVALID_INDEX = static_cast<size_t>(-1);
                if (mesh->triangles.empty() || 
                    mesh->triangles.back().v2 != INVALID_INDEX) {
                    // 开始新三角形
                    Triangle tri{};
                    tri.v0 = idx;
                    tri.v1 = INVALID_INDEX;
                    tri.v2 = INVALID_INDEX;
                    mesh->triangles.push_back(tri);
                } else if (mesh->triangles.back().v1 == INVALID_INDEX) {
                    mesh->triangles.back().v1 = idx;
                } else if (mesh->triangles.back().v2 == INVALID_INDEX) {
                    mesh->triangles.back().v2 = idx;
                }
            } else if (token == "facet" && !mesh->triangles.empty()) {
                // 读取法向量
                std::getline(file, line);
                std::istringstream iss2(line);
                std::string normal_token;
                iss2 >> normal_token;
                if (normal_token == "normal") {
                    double nx, ny, nz;
                    iss2 >> nx >> ny >> nz;
                    // 将法向量应用到当前三角形的顶点
                    if (!mesh->triangles.empty()) {
                        auto& tri = mesh->triangles.back();
                        if (tri.v0 != SIZE_MAX && tri.v0 < mesh->vertices.size()) {
                            mesh->vertices[tri.v0].nx = nx;
                            mesh->vertices[tri.v0].ny = ny;
                            mesh->vertices[tri.v0].nz = nz;
                        }
                    }
                }
            }
        }
        
        // 清理无效三角形
        const size_t INVALID_INDEX = static_cast<size_t>(-1);
        mesh->triangles.erase(
            std::remove_if(mesh->triangles.begin(), mesh->triangles.end(),
                [INVALID_INDEX](const Triangle& t) {
                    return t.v0 == INVALID_INDEX || t.v1 == INVALID_INDEX || t.v2 == INVALID_INDEX;
                }),
            mesh->triangles.end()
        );
        
    } else {
        // 二进制格式
        file.seekg(80);  // 跳过文件头
        file.read(reinterpret_cast<char*>(&num_triangles), sizeof(uint32_t));
        
        // 用于顶点去重的映射（坐标 -> 索引）
        std::map<std::string, size_t> vertex_map;
        size_t vertex_index = 0;
        const double EPSILON = 1e-6;
        
        // 读取每个三角形（50字节：12字节法向量 + 3*12字节顶点 + 2字节属性）
        for (uint32_t i = 0; i < num_triangles; ++i) {
            // 读取法向量（12字节，3个float）
            float nx, ny, nz;
            file.read(reinterpret_cast<char*>(&nx), sizeof(float));
            file.read(reinterpret_cast<char*>(&ny), sizeof(float));
            file.read(reinterpret_cast<char*>(&nz), sizeof(float));
            
            // 读取三个顶点（每个12字节，3个float）
            std::vector<size_t> vertex_indices;
            for (int j = 0; j < 3; ++j) {
                float x, y, z;
                file.read(reinterpret_cast<char*>(&x), sizeof(float));
                file.read(reinterpret_cast<char*>(&y), sizeof(float));
                file.read(reinterpret_cast<char*>(&z), sizeof(float));
                
                // 创建顶点键用于去重（使用与ASCII格式相同的精度）
                std::ostringstream vertex_key;
                vertex_key << std::fixed << std::setprecision(6) << x << "," << y << "," << z;
                std::string key = vertex_key.str();
                
                // 检查顶点是否已存在
                auto it = vertex_map.find(key);
                size_t idx;
                if (it == vertex_map.end()) {
                    // 新顶点
                    Vertex v{};
                    v.x = x;
                    v.y = y;
                    v.z = z;
                    v.nx = nx;
                    v.ny = ny;
                    v.nz = nz;
                    mesh->vertices.push_back(v);
                    vertex_map[key] = vertex_index;
                    idx = vertex_index;
                    vertex_index++;
                } else {
                    // 使用已存在的顶点索引
                    idx = it->second;
                    // 更新法向量（取平均值或使用当前值，这里简单使用当前值）
                    // 注意：STL格式中每个三角形有自己的法向量，这里简化处理
                }
                
                vertex_indices.push_back(idx);
            }
            
            // 添加三角形
            Triangle tri{};
            tri.v0 = vertex_indices[0];
            tri.v1 = vertex_indices[1];
            tri.v2 = vertex_indices[2];
            mesh->triangles.push_back(tri);
            
            // 跳过属性字节（2字节）
            uint16_t attribute;
            file.read(reinterpret_cast<char*>(&attribute), sizeof(uint16_t));
        }
    }
    
    file.close();
    
    if (mesh->vertices.empty() || mesh->triangles.empty()) {
        spdlog::error("STL文件为空或格式错误: {}", filepath);
        return nullptr;
    }
    
    spdlog::info("加载STL文件成功: {} 顶点, {} 三角面", 
                 mesh->vertices.size(), mesh->triangles.size());
    return mesh;
}

bool MeshIO::saveOBJ(const TriangleMesh& mesh, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        spdlog::error("无法创建文件: {}", filepath);
        return false;
    }
    
    // 写入顶点
    for (const auto& v : mesh.vertices) {
        file << "v " << v.x << " " << v.y << " " << v.z << "\n";
        if (v.nx != 0 || v.ny != 0 || v.nz != 0) {
            file << "vn " << v.nx << " " << v.ny << " " << v.nz << "\n";
        }
    }
    
    // 写入面
    for (const auto& tri : mesh.triangles) {
        file << "f " << (tri.v0 + 1) << " " << (tri.v1 + 1) << " " << (tri.v2 + 1) << "\n";
    }
    
    file.close();
    return true;
}

bool MeshIO::saveSTL(const TriangleMesh& mesh, const std::string& filepath) {
    if (!mesh.isValid()) {
        spdlog::error("保存STL失败：网格无效");
        return false;
    }

    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        spdlog::error("无法创建STL文件: {}", filepath);
        return false;
    }

    char header[80] = {0};
    const std::string header_text = "NBCAM binary STL export";
    const size_t copy_len = std::min(header_text.size(), sizeof(header));
    std::copy(header_text.begin(), header_text.begin() + copy_len, header);
    file.write(header, sizeof(header));

    const uint32_t triangle_count = static_cast<uint32_t>(mesh.triangles.size());
    file.write(reinterpret_cast<const char*>(&triangle_count), sizeof(triangle_count));

    for (const auto& tri : mesh.triangles) {
        if (tri.v0 >= mesh.vertices.size() || tri.v1 >= mesh.vertices.size() || tri.v2 >= mesh.vertices.size()) {
            continue;
        }

        const auto& v0 = mesh.vertices[tri.v0];
        const auto& v1 = mesh.vertices[tri.v1];
        const auto& v2 = mesh.vertices[tri.v2];

        const float e1x = static_cast<float>(v1.x - v0.x);
        const float e1y = static_cast<float>(v1.y - v0.y);
        const float e1z = static_cast<float>(v1.z - v0.z);
        const float e2x = static_cast<float>(v2.x - v0.x);
        const float e2y = static_cast<float>(v2.y - v0.y);
        const float e2z = static_cast<float>(v2.z - v0.z);

        float nx = e1y * e2z - e1z * e2y;
        float ny = e1z * e2x - e1x * e2z;
        float nz = e1x * e2y - e1y * e2x;
        const float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (nlen > 1e-12f) {
            nx /= nlen;
            ny /= nlen;
            nz /= nlen;
        } else {
            nx = 0.0f;
            ny = 0.0f;
            nz = 0.0f;
        }

        const float normal[3] = {nx, ny, nz};
        file.write(reinterpret_cast<const char*>(normal), sizeof(normal));

        const float p0[3] = {static_cast<float>(v0.x), static_cast<float>(v0.y), static_cast<float>(v0.z)};
        const float p1[3] = {static_cast<float>(v1.x), static_cast<float>(v1.y), static_cast<float>(v1.z)};
        const float p2[3] = {static_cast<float>(v2.x), static_cast<float>(v2.y), static_cast<float>(v2.z)};
        file.write(reinterpret_cast<const char*>(p0), sizeof(p0));
        file.write(reinterpret_cast<const char*>(p1), sizeof(p1));
        file.write(reinterpret_cast<const char*>(p2), sizeof(p2));

        const uint16_t attr = 0;
        file.write(reinterpret_cast<const char*>(&attr), sizeof(attr));
    }

    file.close();
    const bool ok = file.good();
    if (!ok) {
        spdlog::error("保存STL失败（写入异常）: {}", filepath);
        return false;
    }

    spdlog::info("保存STL成功: {} (triangles={})", filepath, mesh.triangles.size());
    return true;
}

} // namespace nbcam
