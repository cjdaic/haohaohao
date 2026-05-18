#pragma once

#include "tcp_socket.h"
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <cstdint>
#include <chrono>

namespace nbcam {

// 数据缓冲区（双缓冲机制）
class DataBuffer {
public:
    static constexpr int DATA_BUF_NUM = 2;
    static constexpr int DATA_BUF_SIZE = 1600000;  // 1.6MB per buffer
    
    DataBuffer();
    ~DataBuffer();
    
    // 添加加工数据点（激光开启）
    void addProcessData(uint16_t X, uint16_t Y, uint16_t Z, uint16_t A, uint16_t B);
    
    // 添加跳转数据点（激光关闭）
    void addProcessJumpData(uint16_t X, uint16_t Y, uint16_t Z, uint16_t A, uint16_t B);
    
    // 添加开始标记
    void addProcessBegin();
    
    // 添加结束标记
    void addProcessEnd();
    
    // 设置频率
    void setFreqData(int freq);
    
    // 设置功率
    void setPowerData(double power);
    
    // 强制填充缓冲区
    void forceFill();
    
    // 获取读取缓冲区
    bool getReadBuf(int* out_index);
    bool tryGetReadBuf(int* out_index);
    bool hasPendingReadBuf();
    
    // 获取写入缓冲区
    bool getWriteBuf(int* out_index);
    
    // 标记读取完成
    void readEnd(int buf_index);
    
    // 标记写入完成
    void writeEnd(int buf_index);
    
    // 获取缓冲区数据
    const std::vector<uint8_t>& getBuffer(int index) const {
        return databuf_[index];
    }
    size_t getBufferLength(int index) const {
        return (index >= 0 && index < static_cast<int>(buffer_lengths_.size())) ? buffer_lengths_[index] : 0;
    }
    size_t currentWriteOffset() const {
        return ptr_ > 0 ? static_cast<size_t>(ptr_) : 0;
    }
    size_t availableWriteBufferCount();
    bool hasSpareWriteBuffer();

    // 追加原始字节数据
    void appendBytes(const void* data, size_t size);
    
    // 启动TCP线程
    void startTcpThread();
    
    // 停止TCP线程
    void stopTcpThread();

    void setTransportConfig(const TcpSocket::Config& config);
    bool isTcpConnected() const { return tcp_connected_.load(); }
    bool isTcpThreadRunning() const { return tcp_thread_running_.load(); }
    bool waitUntilIdle(std::chrono::milliseconds timeout);
    std::string describeState();
    std::string getLastTransportFeedback() const;
    void onTcpThreadExited();

private:
    std::vector<std::vector<uint8_t>> databuf_;
    std::vector<size_t> buffer_lengths_;
    std::queue<int> wr_queue_;
    std::queue<int> rd_queue_;
    std::mutex wr_mutex_;
    std::mutex rd_mutex_;
    std::condition_variable wr_cv_;
    std::condition_variable rd_cv_;
    std::atomic<int> in_flight_buffers_{0};
    
    int wr_ptr_ = 0;
    int ptr_ = 0;
    
    std::thread tcp_thread_;
    std::atomic<bool> tcp_thread_running_{false};
    std::atomic<bool> tcp_connected_{false};
    TcpSocket::Config transport_config_;
    
    void addData(uint16_t arg1 = 0, uint16_t arg2 = 0, uint16_t arg3 = 0,
                 uint16_t arg4 = 0, uint16_t arg5 = 0, uint16_t arg6 = 0,
                 uint16_t arg7 = 0, uint16_t arg8 = 0);
    
    void handleBufferFilled();
    void handleBegin();
    void markSendBegin();
    void markSendEnd();

    friend class TcpSocket;
    
    // 功率转换函数（从C#移植）
    double powerToValue(double power) const;
};

} // namespace nbcam
