#pragma once

#include "../mesh/mesh_io.h"
#include "../job/laser_job.h"
#include <vector>

namespace nbcam {

// 工艺参数场生成器接口
class IProcessModel {
public:
    virtual ~IProcessModel() = default;
    
    // 根据几何特征生成工艺参数
    virtual void assignProcessParams(
        const TriangleMesh& mesh,
        std::vector<PathPoint>& points
    ) = 0;
    
    // 获取模型名称
    virtual std::string getName() const = 0;
};

} // namespace nbcam
