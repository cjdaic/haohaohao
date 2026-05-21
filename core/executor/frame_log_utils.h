#pragma once

#include "../job/laser_job.h"
#include <array>
#include <string>

namespace nbcam {

// 将LaserJob转换为控制卡帧并记录操作-帧映射日志。
// 约束：忽略频率/功率/速度变化，A/B固定为0，仅输出Begin/Mark/Jump/End帧。
bool exportJobFramesForDemo(const LaserJob& job,
                            const std::string& log_dir,
                            std::string* out_log_path,
                            std::array<uint16_t, 8>* out_first_data_frame,
                            bool swap_yz_axes = false,
                            bool laser_output_enabled = false);

// gRPC + TCP 单帧发送示例（gRPC层采用请求/响应日志模拟，数据面走TCP发16字节帧）。
// 无论发送成功与否，都会写入时间戳日志。
bool sendFrameGrpcTcpDemo(const std::array<uint16_t, 8>& frame,
                          const std::string& log_dir,
                          std::string* out_log_path,
                          const std::string& grpc_endpoint = "localhost:50051",
                          const std::string& tcp_host = "192.168.1.10",
                          uint16_t tcp_port = 7);

// 并发连接 gRPC/TCP 目标并分别发送同一帧（16字节小端原始帧）。
// 说明：gRPC端点按 host:port 建立 TCP 连接并发送原始帧，用于联调连通性与帧链路验证。
bool sendFrameGrpcTcpConcurrent(const std::array<uint16_t, 8>& frame,
                                const std::string& log_dir,
                                std::string* out_log_path,
                                const std::string& grpc_endpoint = "localhost:50051",
                                const std::string& tcp_host = "192.168.1.10",
                                uint16_t tcp_port = 7,
                                int timeout_ms = 800,
                                bool* out_grpc_ok = nullptr,
                                std::string* out_grpc_feedback = nullptr,
                                bool* out_tcp_ok = nullptr,
                                std::string* out_tcp_feedback = nullptr);

// 导出连接阶段使用的完整控制卡数据帧日志（仅头部联通信息 + 纯数据帧）。
bool exportConnectionFrameLog(const LaserJob& job,
                              const std::string& log_dir,
                              bool grpc_ok,
                              const std::string& grpc_feedback,
                              bool tcp_ok,
                              const std::string& tcp_feedback,
                              const std::string& grpc_endpoint,
                              const std::string& tcp_target,
                              bool serial_enabled,
                              bool serial_ok,
                              const std::string& serial_feedback,
                              std::string* out_log_path,
                              bool swap_yz_axes = false,
                              bool laser_output_enabled = false);

}  // namespace nbcam
