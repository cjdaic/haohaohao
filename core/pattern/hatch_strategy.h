#pragma once

#include "fill_strategy_interface.h"

namespace nbcam {

// 栅格填充策略
class HatchStrategy : public IFillStrategy {
public:
    HatchStrategy() = default;
    ~HatchStrategy() override = default;
    
    std::vector<UVPathPoint> generatePath(
        const std::vector<UVCoord>& boundary,
        double spacing,
        double angle = 0.0
    ) override;
    
    std::string getName() const override { return "hatch"; }
};

} // namespace nbcam
