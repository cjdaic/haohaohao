#pragma once

#include "laser_job.h"
#include <string>
#include <memory>

namespace nbcam {

// 序列化器接口
class ISerializer {
public:
    virtual ~ISerializer() = default;
    
    // 序列化任务到字符串
    virtual std::string serialize(const LaserJob& job) = 0;
    
    // 从字符串反序列化任务
    virtual std::unique_ptr<LaserJob> deserialize(const std::string& data) = 0;
    
    // 序列化到文件
    virtual bool saveToFile(const LaserJob& job, const std::string& filepath) = 0;
    
    // 从文件加载
    virtual std::unique_ptr<LaserJob> loadFromFile(const std::string& filepath) = 0;
};

} // namespace nbcam
