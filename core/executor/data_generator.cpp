#include "data_generator.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace nbcam {

// 静态成员初始化
int DataGenerator::JUMP_SPEED = 500;
int DataGenerator::LASER_ON_DELAY = 200;
int DataGenerator::LASER_OFF_DELAY = 25;
int DataGenerator::MARK_DELAY = 0;
int DataGenerator::JUMP_DELAY = 300;
int DataGenerator::POLYGON_DELAY = 0;

void DataGenerator::correction(double& X, double& Y, double& Z, double& A, double& B) {
    // 坐标校正逻辑（根据实际硬件需求实现）
    // 这里先留空，后续根据实际需求补充
}

void DataGenerator::correctionly(double& X, double& Y, double& Z) {
    // 坐标校正逻辑（根据实际硬件需求实现）
    // 这里先留空，后续根据实际需求补充
}

void DataGenerator::genDelayData(double X, double Y, double Z, double A, double B,
                                 int delay, int delayOn, DataBuffer& buffer) {
    const int t = 10;  // 10微秒
    int n = delay / t;
    int i = 0;
    
    correctionly(X, Y, Z);
    
    // 激光开启延时
    while (i < delayOn / t) {
        buffer.addProcessData(static_cast<uint16_t>(X),
                             static_cast<uint16_t>(Y),
                             static_cast<uint16_t>(Z),
                             static_cast<uint16_t>(A),
                             static_cast<uint16_t>(B));
        i++;
    }
    
    // 跳转延时
    while (i < n) {
        buffer.addProcessJumpData(static_cast<uint16_t>(X),
                                  static_cast<uint16_t>(Y),
                                  static_cast<uint16_t>(Z),
                                  static_cast<uint16_t>(A),
                                  static_cast<uint16_t>(B));
        i++;
    }
}

void DataGenerator::genLineData(double speed, int times,
                                double X1, double Y1, double Z1, double A1, double B1,
                                double X2, double Y2, double Z2, double A2, double B2,
                                DataBuffer& buffer) {
    speed *= 0.001;  // 转换为mm/us
    
    double length_xy = 0.001 * std::sqrt(std::pow(X2 - X1, 2) + std::pow(Y2 - Y1, 2));
    double length_z = std::abs(0.001 * (Z2 - Z1));
    double length_ab = 0.001 * std::sqrt(std::pow(A2 - A1, 2) + std::pow(B2 - B1, 2));
    const double t = 0.00001;  // 10微秒
    
    int n_max = 0;
    if (length_xy != 0) {
        n_max = static_cast<int>((length_xy / (speed * t)) + 1);
    } else if (length_z != 0) {
        n_max = static_cast<int>((length_z / (speed * t)) + 1);
        length_xy = length_z;
    } else {
        n_max = static_cast<int>((length_ab / (speed * t)) + 1);
        length_xy = length_ab;
    }
    
    if (X1 == X2 && Y1 == Y2 && Z1 == Z2 && A1 == A2 && B1 == B2) {
        correction(X1, Y1, Z1, A1, B1);
        buffer.addProcessJumpData(static_cast<uint16_t>(X1),
                                  static_cast<uint16_t>(Y1),
                                  static_cast<uint16_t>(Z1),
                                  static_cast<uint16_t>(A1),
                                  static_cast<uint16_t>(B1));
        return;
    }
    
    for (int repeat = 0; repeat < times; ++repeat) {
        for (int i = 1; i <= n_max; ++i) {
            double X = X1 + i * (X2 - X1) * speed * t / length_xy;
            double Y = Y1 + i * (Y2 - Y1) * speed * t / length_xy;
            double Z = Z1 + i * (Z2 - Z1) * speed * t / length_xy;
            double A = A1 + i * (A2 - A1) * speed * t / length_xy;
            double B = B1 + i * (B2 - B1) * speed * t / length_xy;
            
            correction(X, Y, Z, A, B);
            
            if (i * 10 < LASER_ON_DELAY) {
                buffer.addProcessJumpData(static_cast<uint16_t>(X),
                                          static_cast<uint16_t>(Y),
                                          static_cast<uint16_t>(Z),
                                          static_cast<uint16_t>(A),
                                          static_cast<uint16_t>(B));
            } else {
                buffer.addProcessData(static_cast<uint16_t>(X),
                                     static_cast<uint16_t>(Y),
                                     static_cast<uint16_t>(Z),
                                     static_cast<uint16_t>(A),
                                     static_cast<uint16_t>(B));
            }
        }
    }
}

void DataGenerator::genCircleData(double speed, int times,
                                  double X1, double Y1, double X2, double Y2,
                                  int M, double angle, double taper, bool filled,
                                  double r_min, double r_interval,
                                  double z_start, double z_end, double z_interval,
                                  int circle_num_repair, int times_repair,
                                  DataBuffer& buffer) {
    // 圆形数据生成逻辑（简化实现，完整实现需要参考C#代码）
    // 这里提供基础框架
    spdlog::warn("genCircleData: 完整实现待补充");
}

void DataGenerator::genFilledRectangleData(double X0, double Y0, double X1, double Y1,
                                          double taperAMax, double taperBMax,
                                          double feedSpacingX, double feedSpacingY,
                                          double speed, int times,
                                          double z_start, double z_end, double z_interval,
                                          double X2, double Y2,
                                          int circle_num_repair, int times_repair,
                                          DataBuffer& buffer) {
    // 矩形填充数据生成逻辑（简化实现）
    spdlog::warn("genFilledRectangleData: 完整实现待补充");
}

void DataGenerator::genFilledRectangleData3D(double X0, double Y0, double Z0,
                                            double X1, double Y1, double Z1,
                                            double speed, double interval, int times,
                                            DataBuffer& buffer) {
    buffer.addProcessBegin();
    
    // 3D矩形填充逻辑（简化实现）
    // 这里需要根据C#代码完整实现
    
    buffer.addProcessEnd();
}

void DataGenerator::genFilledEllipseData(double speed, int times,
                                         double X0, double Y0,
                                         double aMax, double bMax, double aMin, double bMin,
                                         double feedSpacingX, double feedSpacingY,
                                         double taperAMax, double taperBMax,
                                         double z_start, double z_end, double z_interval,
                                         int circle_num_repair, int times_repair,
                                         DataBuffer& buffer) {
    // 椭圆填充数据生成逻辑（简化实现）
    spdlog::warn("genFilledEllipseData: 完整实现待补充");
}

void DataGenerator::setFreq(int freq, DataBuffer& buffer) {
    buffer.setFreqData(freq);
}

} // namespace nbcam
