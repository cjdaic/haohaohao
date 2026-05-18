#pragma once

#include "fill_strategy_interface.h"

namespace nbcam {

// 沿X轴直线往返填充策略（带箭头指示方向）
// 生成平行于X轴的扫描线，奇数行从左到右、偶数行从右到左，形成锯齿形路径
class LineHatchStrategy : public IFillStrategy {
public:
    LineHatchStrategy() = default;
    ~LineHatchStrategy() override = default;

    std::vector<UVPathPoint> generatePath(
        const std::vector<UVCoord>& boundary,
        double spacing,
        double angle = 0.0
    ) override;

    std::string getName() const override { return "line_hatch"; }
};

}  // namespace nbcam
