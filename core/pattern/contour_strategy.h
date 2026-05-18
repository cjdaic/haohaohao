#pragma once

#include "fill_strategy_interface.h"

namespace nbcam {

// 轮廓填充策略
class ContourStrategy : public IFillStrategy {
public:
    ContourStrategy() = default;
    ~ContourStrategy() override = default;
    
    std::vector<UVPathPoint> generatePath(
        const std::vector<UVCoord>& boundary,
        double spacing,
        double angle = 0.0
    ) override;
    
    std::string getName() const override { return "contour"; }
};

} // namespace nbcam
