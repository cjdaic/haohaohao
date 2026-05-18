#include "grpc_service.h"
#include <spdlog/spdlog.h>

namespace nbcam {

FiveAxisServiceImpl::FiveAxisServiceImpl(std::shared_ptr<DataBuffer> buffer)
    : data_buffer_(buffer)
{
}

FiveAxisServiceImpl::~FiveAxisServiceImpl() {
    stop();
}

void FiveAxisServiceImpl::start(const std::string& server_address) {
    if (is_running_) {
        spdlog::warn("gRPC服务已在运行");
        return;
    }
    
    // gRPC服务器启动逻辑
    // 完整实现需要：
    // 1. 使用protoc生成C++代码
    // 2. 实现FiveAxis::Service接口
    // 3. 创建并启动服务器
    
    spdlog::warn("gRPC服务实现待完善，需要生成protobuf代码");
    is_running_ = false;
}

void FiveAxisServiceImpl::stop() {
    if (is_running_) {
        if (server_) {
            server_->Shutdown();
        }
        is_running_ = false;
        spdlog::info("gRPC服务已停止");
    }
}

} // namespace nbcam
