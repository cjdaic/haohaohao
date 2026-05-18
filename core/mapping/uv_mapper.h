#pragma once

#include "../mesh/mesh_io.h"
#include "../param/parameterizer_interface.h"
#include "../pattern/fill_strategy_interface.h"
#include "../job/laser_job.h"
#include <vector>

namespace nbcam {

// UV 空间网格，用于加速三角形查找（避免大模型下 O(N*M) 的全量遍历）
struct UVGrid {
    double u_min = 0, u_max = 0, v_min = 0, v_max = 0;
    int grid_nu = 64, grid_nv = 64;  // 网格分辨率
    std::vector<std::vector<size_t>> cells;  // cells[cell_idx] = triangle indices
};

// UV到XYZ映射器
class UVMapper {
public:
    UVMapper() = default;
    ~UVMapper() = default;
    
    // 将UV路径点映射回三维XYZ坐标
    // triangle_indices: 可选的三角形索引列表，如果提供则只搜索这些三角形（用于patch映射）
    std::vector<PathPoint> mapUVToXYZ(
        const std::vector<UVPathPoint>& uv_path,
        const TriangleMesh& mesh,
        const std::vector<UVCoord>& uv_coords,
        const std::vector<size_t>* triangle_indices = nullptr
    );
    
    // 查找UV点所在的三角面
    size_t findTriangle(const UVPathPoint& uv_point,
                       const std::vector<UVCoord>& uv_coords,
                       const TriangleMesh& mesh,
                       const std::vector<size_t>* triangle_indices = nullptr) const;
    
    // 在三角面内插值得到XYZ坐标
    void interpolateXYZ(const UVPathPoint& uv_point,
                       size_t triangle_index,
                       const std::vector<UVCoord>& uv_coords,
                       const TriangleMesh& mesh,
                       double& x, double& y, double& z) const;

private:
    // 构建 UV 空间网格加速查找
    static UVGrid buildUVGrid(const TriangleMesh& mesh,
                              const std::vector<UVCoord>& uv_coords,
                              const std::vector<size_t>& triangle_indices);

    // 使用网格加速的三角形查找
    static size_t findTriangleWithGrid(const UVPathPoint& uv_point,
                                      const UVGrid& grid,
                                      const std::vector<UVCoord>& uv_coords,
                                      const TriangleMesh& mesh);
};

} // namespace nbcam
