#include "laser_job.h"
#include <algorithm>
#include <cmath>

namespace nbcam {

void LaserJob::clear() {
    meta = JobMeta();
    coordinate = CoordinateInfo();
    parameterization = ParameterizationInfo();
    process_defaults = ProcessParams();
    segments.clear();
}

bool LaserJob::isValid() const {
    if (segments.empty()) {
        return false;
    }
    
    // 检查是否有有效的路径点
    for (const auto& segment : segments) {
        if (segment.points.empty()) {
            return false;
        }
    }
    
    return true;
}

size_t LaserJob::getTotalPointCount() const {
    size_t count = 0;
    for (const auto& segment : segments) {
        count += segment.points.size();
    }
    return count;
}

double LaserJob::estimateTotalTime() const {
    double total_time = 0.0;
    
    for (const auto& segment : segments) {
        if (segment.points.size() < 2) {
            continue;
        }
        
        const ProcessParams* params = segment.params_override.get();
        if (!params) {
            params = &process_defaults;
        }
        
        double speed = params->speed_mm_s;
        if (speed <= 0) {
            speed = 300.0;  // 默认速度
        }
        
        // 计算路径长度
        double length = 0.0;
        for (size_t i = 1; i < segment.points.size(); ++i) {
            const auto& p1 = segment.points[i - 1];
            const auto& p2 = segment.points[i];
            
            double dx = p2.x - p1.x;
            double dy = p2.y - p1.y;
            double dz = p2.z - p1.z;
            length += std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        
        // 时间 = 距离 / 速度
        total_time += length / speed;
        
        // 添加延时
        if (segment.type == SegmentType::MARK) {
            total_time += params->laser_on_delay_us * 1e-6;
            total_time += params->laser_off_delay_us * 1e-6;
        }
    }
    
    return total_time;
}

} // namespace nbcam
