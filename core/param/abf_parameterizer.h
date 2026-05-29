#pragma once

#include "parameterizer_interface.h"

namespace nbcam {

// ABF参数化算法实现（Angle Based Flattening）
class ABFParameterizer : public IParameterizer {
public:
    ABFParameterizer() = default;
    ~ABFParameterizer() override = default;

    ParameterizationResult parameterize(const TriangleMesh& mesh) override;
    std::string getName() const override { return "ABF"; }
};

} // namespace nbcam
