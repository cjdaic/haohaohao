#pragma once

#include "parameterizer_interface.h"

namespace nbcam {

// 保面积参数化算法实现（Discrete Authalic）
class AuthalicParameterizer : public IParameterizer {
public:
    AuthalicParameterizer() = default;
    ~AuthalicParameterizer() override = default;

    ParameterizationResult parameterize(const TriangleMesh& mesh) override;
    std::string getName() const override { return "AUTHALIC"; }
};

} // namespace nbcam

