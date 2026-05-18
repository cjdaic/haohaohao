#pragma once

#include "fill_strategy_interface.h"

namespace nbcam {

// 回形矩形填充：从内到外逐层填充，每层为轴对齐矩形，间隔由 spacing 控制
class RingFillStrategy : public IFillStrategy {
public:
    RingFillStrategy() = default;
    ~RingFillStrategy() override = default;

    std::vector<UVPathPoint> generatePath(
        const std::vector<UVCoord>& boundary,
        double spacing,
        double angle = 0.0
    ) override;

    std::string getName() const override { return "ring"; }
};

}  // namespace nbcam
