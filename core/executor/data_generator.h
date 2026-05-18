#pragma once

#include "data_buffer.h"
#include <cstdint>

namespace nbcam {

// 数据生成器（从C#移植）
class DataGenerator {
public:
    // 延时参数
    static int JUMP_SPEED;
    static int LASER_ON_DELAY;
    static int LASER_OFF_DELAY;
    static int MARK_DELAY;
    static int JUMP_DELAY;
    static int POLYGON_DELAY;
    
    // 生成线段数据
    static void genLineData(double speed, int times,
                           double X1, double Y1, double Z1, double A1, double B1,
                           double X2, double Y2, double Z2, double A2, double B2,
                           DataBuffer& buffer);
    
    // 生成圆形数据
    static void genCircleData(double speed, int times,
                             double X1, double Y1, double X2, double Y2,
                             int M, double angle, double taper, bool filled,
                             double r_min, double r_interval,
                             double z_start, double z_end, double z_interval,
                             int circle_num_repair, int times_repair,
                             DataBuffer& buffer);
    
    // 生成矩形填充数据
    static void genFilledRectangleData(double X0, double Y0, double X1, double Y1,
                                       double taperAMax, double taperBMax,
                                       double feedSpacingX, double feedSpacingY,
                                       double speed, int times,
                                       double z_start, double z_end, double z_interval,
                                       double X2, double Y2,
                                       int circle_num_repair, int times_repair,
                                       DataBuffer& buffer);
    
    // 生成3D矩形填充数据
    static void genFilledRectangleData3D(double X0, double Y0, double Z0,
                                        double X1, double Y1, double Z1,
                                        double speed, double interval, int times,
                                        DataBuffer& buffer);
    
    // 生成椭圆填充数据
    static void genFilledEllipseData(double speed, int times,
                                     double X0, double Y0,
                                     double aMax, double bMax, double aMin, double bMin,
                                     double feedSpacingX, double feedSpacingY,
                                     double taperAMax, double taperBMax,
                                     double z_start, double z_end, double z_interval,
                                     int circle_num_repair, int times_repair,
                                     DataBuffer& buffer);
    
    // 设置频率
    static void setFreq(int freq, DataBuffer& buffer);
    
private:
    // 坐标校正函数
    static void correction(double& X, double& Y, double& Z, double& A, double& B);
    static void correctionly(double& X, double& Y, double& Z);
    
    // 生成延时数据
    static void genDelayData(double X, double Y, double Z, double A, double B,
                            int delay, int delayOn, DataBuffer& buffer);
};

} // namespace nbcam
