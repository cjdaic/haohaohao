#pragma once

#include "../job/laser_job.h"
#include <memory>
#include <string>

namespace nbcam {

// 执行器接口
class IJobExecutor {
public:
    virtual ~IJobExecutor() = default;
    
    // 初始化执行器
    virtual bool initialize() = 0;
    
    // 执行任务
    virtual bool execute(const LaserJob& job) = 0;
    
    // 停止执行
    virtual void stop() = 0;
    
    // 获取状态
    virtual std::string getStatus() const = 0;
    
    // 是否连接
    virtual bool isConnected() const = 0;
};

} // namespace nbcam
