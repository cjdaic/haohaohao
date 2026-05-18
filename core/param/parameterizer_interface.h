#pragma once

#include "../mesh/mesh_io.h"
#include <vector>

namespace nbcam {

// UV坐标
struct UVCoord {
    double u, v;
};

// 参数化结果
struct ParameterizationResult {
    std::vector<UVCoord> uv_coords;  // 每个顶点对应的UV坐标
    int island_count = 1;            // UV岛数量
    bool success = false;
};

// 参数化器接口
class IParameterizer {
public:
    virtual ~IParameterizer() = default;
    
    // 对网格进行UV参数化
    virtual ParameterizationResult parameterize(const TriangleMesh& mesh) = 0;
    
    // 获取算法名称
    virtual std::string getName() const = 0;
};

} // namespace nbcam
