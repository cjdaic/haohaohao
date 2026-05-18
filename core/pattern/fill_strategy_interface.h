#pragma once

#include <vector>
#include "../param/parameterizer_interface.h"

namespace nbcam {

// UV路径点
struct UVPathPoint {
    double u, v;
    bool is_jump_before = false;   // true表示此点前有跳转（不连线）
    bool is_arrow_tip = false;     // true表示在此点绘制箭头指示方向（从上一点到此点）
};

// 填充策略接口
class IFillStrategy {
public:
    virtual ~IFillStrategy() = default;
    
    // 在UV平面上生成路径
    virtual std::vector<UVPathPoint> generatePath(
        const std::vector<UVCoord>& boundary,  // 边界点
        double spacing,                        // 填充间距
        double angle = 0.0                     // 填充角度
    ) = 0;
    
    // 获取策略名称
    virtual std::string getName() const = 0;
};

} // namespace nbcam
