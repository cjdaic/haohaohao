#pragma once

#include "data_generator.h"
#include "data_buffer.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <thread>
#include <atomic>

namespace nbcam {

// gRPC服务实现（简化版，完整实现需要生成protobuf代码）
class FiveAxisServiceImpl {
public:
    FiveAxisServiceImpl(std::shared_ptr<DataBuffer> buffer);
    ~FiveAxisServiceImpl();
    
    // 启动gRPC服务器
    void start(const std::string& server_address = "0.0.0.0:50051");
    void stop();
    
    bool isRunning() const { return is_running_; }

private:
    std::shared_ptr<DataBuffer> data_buffer_;
    std::unique_ptr<grpc::Server> server_;
    std::atomic<bool> is_running_{false};
    
    // gRPC服务方法（需要根据生成的protobuf代码实现）
    // grpc::Status ProcessLine(grpc::ServerContext* context, const LineData* request, ServerReply* reply);
    // ...
};

} // namespace nbcam
