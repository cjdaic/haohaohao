#pragma once

#include "process_model_interface.h"

namespace nbcam {

// 基于曲率的工艺参数模型
class CurvatureModel : public IProcessModel {
public:
    CurvatureModel() = default;
    ~CurvatureModel() override = default;
    
    void assignProcessParams(
        const TriangleMesh& mesh,
        std::vector<PathPoint>& points
    ) override;
    
    std::string getName() const override { return "curvature"; }
    
    // 设置参数范围
    void setPowerRange(double min_power, double max_power) {
        min_power_ = min_power;
        max_power_ = max_power;
    }
    
    void setSpeedRange(double min_speed, double max_speed) {
        min_speed_ = min_speed;
        max_speed_ = max_speed;
    }

private:
    double min_power_ = 10.0;
    double max_power_ = 30.0;
    double min_speed_ = 200.0;
    double max_speed_ = 500.0;
    
    // 计算曲率
    double computeCurvature(const TriangleMesh& mesh, size_t vertex_index) const;
};

} // namespace nbcam
