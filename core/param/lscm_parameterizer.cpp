#include "lscm_parameterizer.h"
#include "cgal_mesh_utils.h"
#include <spdlog/spdlog.h>
#include <CGAL/Surface_mesh_parameterization/LSCM_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Mean_value_coordinates_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/parameterize.h>
#include <CGAL/boost/graph/properties.h>
#include <iostream>
#include <vector>
#include <limits>
#include <cmath>
#include <algorithm>

namespace nbcam {

// 使用LSCM进行参数化
bool parameterizeWithLSCM(Surface_mesh& mesh, UV_pmap uv_map, ParameterizationResult& result,
                          const std::map<vertex_descriptor, size_t>& cgal_to_original_index) {
    try {
        // 查找边界 - 遍历halfedge找到边界
        halfedge_descriptor bhd = boost::graph_traits<Surface_mesh>::null_halfedge();
        for (auto h : halfedges(mesh)) {
            if (CGAL::is_border(h, mesh)) {
                bhd = h;
                break;
            }
        }
        
        if (bhd == boost::graph_traits<Surface_mesh>::null_halfedge()) {
            spdlog::error("无法找到网格边界");
            return false;
        }
        
        // 使用LSCM参数化
        CGAL::Surface_mesh_parameterization::LSCM_parameterizer_3<Surface_mesh> parameterizer;
        CGAL::Surface_mesh_parameterization::Error_code err = 
            CGAL::Surface_mesh_parameterization::parameterize(mesh, parameterizer, bhd, uv_map);
        
        if (err != CGAL::Surface_mesh_parameterization::OK) {
            spdlog::error("LSCM参数化失败，错误代码: {}", static_cast<int>(err));
            return false;
        }
        
        // 提取UV坐标 - 使用CGAL顶点到原始索引的映射确保顺序一致
        result.uv_coords.clear();
        result.uv_coords.reserve(mesh.number_of_vertices());
        
        // 按索引顺序提取UV坐标
        result.uv_coords.resize(mesh.number_of_vertices(), {0.0, 0.0});
        double u_min = std::numeric_limits<double>::max();
        double u_max = std::numeric_limits<double>::lowest();
        double v_min = std::numeric_limits<double>::max();
        double v_max = std::numeric_limits<double>::lowest();
        
        // 第一遍：提取并找到有效范围
        std::vector<bool> valid_uv(mesh.number_of_vertices(), false);
        
        // 提取UV坐标，使用正确的索引映射
        for (auto v : vertices(mesh)) {
            auto it = cgal_to_original_index.find(v);
            if (it == cgal_to_original_index.end()) {
                spdlog::warn("LSCM参数化: 无法找到CGAL顶点的原始索引，跳过该顶点");
                continue;
            }
            
            size_t idx = it->second;
            if (idx >= result.uv_coords.size()) {
                spdlog::warn("LSCM参数化: 顶点索引 {} 超出范围 (总数: {})", idx, result.uv_coords.size());
                continue;
            }
            
            const Point_2& uv = uv_map[v];
            double u = uv.x();
            double v_val = uv.y();
            
            // 检查是否为有效值（非NaN、非Inf）
            if (std::isfinite(u) && std::isfinite(v_val)) {
                result.uv_coords[idx].u = u;
                result.uv_coords[idx].v = v_val;
                valid_uv[idx] = true;
                u_min = std::min(u_min, u);
                u_max = std::max(u_max, u);
                v_min = std::min(v_min, v_val);
                v_max = std::max(v_max, v_val);
            } else {
                // 无效值，标记为待修复
                result.uv_coords[idx].u = 0.0;
                result.uv_coords[idx].v = 0.0;
                valid_uv[idx] = false;
                spdlog::warn("LSCM参数化: 顶点 {} 产生无效UV坐标 ({}, {})", idx, u, v_val);
            }
        }
        
        // 验证UV坐标的有效性（只检查NaN和Inf，不进行离群值检测）
        // 如果参数化过程正确，不应该有异常点
        size_t invalid_count = 0;
        for (size_t i = 0; i < valid_uv.size(); ++i) {
            if (!valid_uv[i]) {
                invalid_count++;
                spdlog::warn("LSCM参数化: 顶点 {} 的UV坐标无效（NaN或Inf），这可能是参数化过程中的错误", i);
            }
        }
        
        if (invalid_count > 0) {
            spdlog::error("LSCM参数化: 发现 {} 个无效UV坐标，这通常表示参数化过程存在问题", invalid_count);
            // 对于无效点，使用UV范围的中心作为临时值（但应该调查为什么会出现无效值）
            if (u_max > u_min && v_max > v_min) {
                for (size_t i = 0; i < valid_uv.size(); ++i) {
                    if (!valid_uv[i]) {
                        result.uv_coords[i].u = (u_min + u_max) * 0.5;
                        result.uv_coords[i].v = (v_min + v_max) * 0.5;
                        spdlog::warn("LSCM参数化: 临时修复顶点 {} 的UV坐标为范围中心 ({:.6f}, {:.6f})", 
                                    i, result.uv_coords[i].u, result.uv_coords[i].v);
                    }
                }
            }
        }
        
        result.island_count = 1;  // LSCM通常产生单个UV岛
        result.success = true;
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("LSCM参数化异常: {}", e.what());
        return false;
    }
}

ParameterizationResult LSCMParameterizer::parameterize(const TriangleMesh& mesh) {
    ParameterizationResult result;
    
    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        spdlog::error("网格为空，无法进行参数化");
        result.success = false;
        return result;
    }
    
    spdlog::info("开始LSCM参数化: {} 顶点, {} 三角面", 
                 mesh.vertices.size(), mesh.triangles.size());
    
    try {
        // 转换为CGAL Surface_mesh，获取顶点索引映射
        Surface_mesh cgal_mesh;
        std::map<vertex_descriptor, size_t> cgal_to_original_index;
        if (!convertToCGALMesh(mesh, cgal_mesh, cgal_to_original_index)) {
            result.success = false;
            return result;
        }
        
        // 检查网格有效性
        if (cgal_mesh.number_of_vertices() == 0 || cgal_mesh.number_of_faces() == 0) {
            spdlog::error("CGAL网格转换后为空");
            result.success = false;
            return result;
        }
        
        // 创建UV属性映射
        UV_pmap uv_map = cgal_mesh.add_property_map<vertex_descriptor, Point_2>("h:uv").first;
        
        // 使用LSCM进行参数化
        if (!parameterizeWithLSCM(cgal_mesh, uv_map, result, cgal_to_original_index)) {
            // 如果LSCM失败，尝试使用Mean Value Coordinates
            spdlog::warn("LSCM参数化失败，尝试使用Mean Value Coordinates");
            
            // 查找边界
            halfedge_descriptor bhd = boost::graph_traits<Surface_mesh>::null_halfedge();
            for (auto h : halfedges(cgal_mesh)) {
                if (CGAL::is_border(h, cgal_mesh)) {
                    bhd = h;
                    break;
                }
            }
            
            if (bhd != boost::graph_traits<Surface_mesh>::null_halfedge()) {
                CGAL::Surface_mesh_parameterization::Mean_value_coordinates_parameterizer_3<Surface_mesh> parameterizer;
                CGAL::Surface_mesh_parameterization::Error_code err = 
                    CGAL::Surface_mesh_parameterization::parameterize(cgal_mesh, parameterizer, bhd, uv_map);
                
                if (err == CGAL::Surface_mesh_parameterization::OK) {
                    // 提取UV坐标并验证（使用正确的索引映射）
                    result.uv_coords.resize(cgal_mesh.number_of_vertices(), {0.0, 0.0});
                    double u_min = std::numeric_limits<double>::max();
                    double u_max = std::numeric_limits<double>::lowest();
                    double v_min = std::numeric_limits<double>::max();
                    double v_max = std::numeric_limits<double>::lowest();
                    
                    std::vector<bool> valid_uv(cgal_mesh.number_of_vertices(), false);
                    for (auto v : vertices(cgal_mesh)) {
                        auto it = cgal_to_original_index.find(v);
                        if (it == cgal_to_original_index.end()) {
                            spdlog::warn("LSCM参数化(Mean Value): 无法找到CGAL顶点的原始索引，跳过该顶点");
                            continue;
                        }
                        
                        size_t idx = it->second;
                        if (idx >= result.uv_coords.size()) {
                            spdlog::warn("LSCM参数化(Mean Value): 顶点索引 {} 超出范围", idx);
                            continue;
                        }
                        
                        const Point_2& uv = uv_map[v];
                        double u = uv.x();
                        double v_val = uv.y();
                        
                        if (std::isfinite(u) && std::isfinite(v_val)) {
                            result.uv_coords[idx].u = u;
                            result.uv_coords[idx].v = v_val;
                            valid_uv[idx] = true;
                            u_min = std::min(u_min, u);
                            u_max = std::max(u_max, u);
                            v_min = std::min(v_min, v_val);
                            v_max = std::max(v_max, v_val);
                        } else {
                            result.uv_coords[idx].u = 0.0;
                            result.uv_coords[idx].v = 0.0;
                            valid_uv[idx] = false;
                        }
                    }
                    
                    // 验证UV坐标的有效性（只检查NaN和Inf）
                    size_t invalid_count = 0;
                    for (size_t i = 0; i < valid_uv.size(); ++i) {
                        if (!valid_uv[i]) {
                            invalid_count++;
                        }
                    }
                    
                    if (invalid_count > 0) {
                        spdlog::error("LSCM参数化(Mean Value): 发现 {} 个无效UV坐标", invalid_count);
                        if (u_max > u_min && v_max > v_min) {
                            for (size_t i = 0; i < valid_uv.size(); ++i) {
                                if (!valid_uv[i]) {
                                    result.uv_coords[i].u = (u_min + u_max) * 0.5;
                                    result.uv_coords[i].v = (v_min + v_max) * 0.5;
                                }
                            }
                        }
                    }
                    
                    result.island_count = 1;
                    result.success = true;
                } else {
                    spdlog::error("Mean Value Coordinates参数化也失败");
                    result.success = false;
                }
            } else {
                spdlog::error("无法找到网格边界");
                result.success = false;
            }
        }
        
        if (result.success) {
            spdlog::info("参数化完成: {} 顶点, {} 个UV岛", 
                         result.uv_coords.size(), result.island_count);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("LSCM参数化异常: {}", e.what());
        result.success = false;
    }
    
    return result;
}

} // namespace nbcam
