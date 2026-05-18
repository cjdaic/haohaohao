#pragma once

#include "parameterizer_interface.h"

namespace nbcam {

// ARAP参数化算法实现（As-Rigid-As-Possible）
// 作为BFF的替代方案
class ARAPParameterizer : public IParameterizer {
public:
    ARAPParameterizer() = default;
    ~ARAPParameterizer() override = default;
    
    ParameterizationResult parameterize(const TriangleMesh& mesh) override;
    std::string getName() const override { return "ARAP"; }
};

} // namespace nbcam
