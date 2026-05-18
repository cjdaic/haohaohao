#pragma once

#include "parameterizer_interface.h"

namespace nbcam {

// LSCM参数化算法实现（Least Squares Conformal Maps）
// 作为ABF的替代方案
class LSCMParameterizer : public IParameterizer {
public:
    LSCMParameterizer() = default;
    ~LSCMParameterizer() override = default;
    
    ParameterizationResult parameterize(const TriangleMesh& mesh) override;
    std::string getName() const override { return "LSCM"; }
};

} // namespace nbcam
