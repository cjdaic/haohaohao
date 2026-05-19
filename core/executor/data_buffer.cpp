#include "data_buffer.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <stdexcept>

namespace nbcam {

DataBuffer::DataBuffer() {
    databuf_.resize(DATA_BUF_NUM);
    buffer_lengths_.resize(DATA_BUF_NUM, 0);
    for (int i = 0; i < DATA_BUF_NUM; ++i) {
        databuf_[i].resize(DATA_BUF_SIZE, 0);
        wr_queue_.push(i);
    }
    
    if (!wr_queue_.empty()) {
        wr_ptr_ = wr_queue_.front();
        wr_queue_.pop();
    }
}

DataBuffer::~DataBuffer() {
    stopTcpThread();
}

void DataBuffer::addData(uint16_t arg1, uint16_t arg2, uint16_t arg3,
                        uint16_t arg4, uint16_t arg5, uint16_t arg6,
                        uint16_t arg7, uint16_t arg8) {
    uint8_t args[16] = {
        static_cast<uint8_t>(arg1 & 0xFF), static_cast<uint8_t>(arg1 >> 8),
        static_cast<uint8_t>(arg2 & 0xFF), static_cast<uint8_t>(arg2 >> 8),
        static_cast<uint8_t>(arg3 & 0xFF), static_cast<uint8_t>(arg3 >> 8),
        static_cast<uint8_t>(arg4 & 0xFF), static_cast<uint8_t>(arg4 >> 8),
        static_cast<uint8_t>(arg5 & 0xFF), static_cast<uint8_t>(arg5 >> 8),
        static_cast<uint8_t>(arg6 & 0xFF), static_cast<uint8_t>(arg6 >> 8),
        static_cast<uint8_t>(arg7 & 0xFF), static_cast<uint8_t>(arg7 >> 8),
        static_cast<uint8_t>(arg8 & 0xFF), static_cast<uint8_t>(arg8 >> 8)
    };
    
    for (int i = 0; i < 16; ++i) {
        if (ptr_ >= DATA_BUF_SIZE) {
            handleBufferFilled();
        }
        databuf_[wr_ptr_][ptr_] = args[i];
        ptr_++;
    }
    
    handleBufferFilled();
}

void DataBuffer::addProcessData(uint16_t X, uint16_t Y, uint16_t Z, uint16_t A, uint16_t B) {
    addData(B, A, Z, Y, X, 0x00FF);
}

void DataBuffer::addProcessJumpData(uint16_t X, uint16_t Y, uint16_t Z, uint16_t A, uint16_t B) {
    addData(B, A, Z, Y, X, 0);
}

void DataBuffer::addProcessBegin() {
    handleBegin();
    addData(0, 0, 0, 0, 0, 0xFF00, 0, 0);
}

void DataBuffer::addProcessEnd() {
    addData(0, 0, 0, 0, 0, 0x1100, 0, 0);
}

void DataBuffer::setFreqData(int freq) {
    handleBegin();
    int cnt = 50000 / freq;
    addData(0xAA, 0, 0, 0, 0, 0xAA00, 0, 0);
    addData(static_cast<uint16_t>(cnt & 0xFFFF), static_cast<uint16_t>(cnt >> 16));
    addData(0xAA, 0, 0, 0, 0, 0x5500, 0, 0);
}

void DataBuffer::setPowerData(double power) {
    if (power > 100.0) {
        power = 100.0;
    }
    
    double p_value = powerToValue(power);
    uint16_t p = static_cast<uint16_t>(p_value * 65535.0 / 100.0);
    
    for (int i = 0; i < 10; ++i) {
        addData(p, 0, 0, 0, 0, 0xbb00, 0, 11451);
    }
    forceFill();
}

void DataBuffer::forceFill() {
    if (ptr_ <= 0) {
        return;
    }
    const size_t payload_length = static_cast<size_t>(ptr_);
    buffer_lengths_[wr_ptr_] = payload_length;
    while (ptr_ < DATA_BUF_SIZE) {
        databuf_[wr_ptr_][ptr_] = 0;
        ptr_++;
    }
    buffer_lengths_[wr_ptr_] = payload_length;
    handleBufferFilled();
}

void DataBuffer::appendBytes(const void* data, size_t size)
{
    if (!data || size == 0) {
        return;
    }

    const auto* src = static_cast<const uint8_t*>(data);
    size_t remaining = size;
    while (remaining > 0) {
        if (ptr_ >= DATA_BUF_SIZE) {
            handleBufferFilled();
        }

        const size_t writable = std::min<size_t>(remaining, static_cast<size_t>(DATA_BUF_SIZE - ptr_));
        std::copy(src, src + writable, databuf_[wr_ptr_].begin() + ptr_);
        ptr_ += static_cast<int>(writable);
        src += writable;
        remaining -= writable;
        buffer_lengths_[wr_ptr_] = static_cast<size_t>(ptr_);

        if (ptr_ >= DATA_BUF_SIZE) {
            handleBufferFilled();
        }
    }
}

bool DataBuffer::getWriteBuf(int* out_index) {
    std::unique_lock<std::mutex> lock(wr_mutex_);
    wr_cv_.wait(lock, [this] { return !wr_queue_.empty() || !tcp_thread_running_.load(); });
    if (wr_queue_.empty()) {
        return false;
    }
    int p = wr_queue_.front();
    wr_queue_.pop();
    if (out_index) {
        *out_index = p;
    }
    return true;
}

bool DataBuffer::getReadBuf(int* out_index) {
    std::unique_lock<std::mutex> lock(rd_mutex_);
    rd_cv_.wait(lock, [this] { return !rd_queue_.empty() || !tcp_thread_running_.load(); });
    if (rd_queue_.empty()) {
        return false;
    }
    int p = rd_queue_.front();
    rd_queue_.pop();
    if (out_index) {
        *out_index = p;
    }
    return true;
}

bool DataBuffer::tryGetReadBuf(int* out_index)
{
    std::lock_guard<std::mutex> lock(rd_mutex_);
    if (rd_queue_.empty()) {
        return false;
    }
    int p = rd_queue_.front();
    rd_queue_.pop();
    if (out_index) {
        *out_index = p;
    }
    return true;
}

bool DataBuffer::hasPendingReadBuf()
{
    std::lock_guard<std::mutex> lock(rd_mutex_);
    return !rd_queue_.empty();
}

void DataBuffer::readEnd(int buf_index) {
    std::lock_guard<std::mutex> lock(wr_mutex_);
    if (wr_queue_.size() >= DATA_BUF_NUM) {
        spdlog::warn("写队列异常");
    }
    wr_queue_.push(buf_index);
    wr_cv_.notify_one();
}

void DataBuffer::writeEnd(int buf_index) {
    std::lock_guard<std::mutex> lock(rd_mutex_);
    if (rd_queue_.size() >= DATA_BUF_NUM) {
        spdlog::warn("读队列异常");
    }
    rd_queue_.push(buf_index);
    rd_cv_.notify_one();
}

void DataBuffer::startTcpThread() {
    if (tcp_thread_running_.load()) {
        return;
    }
    if (tcp_thread_.joinable()) {
        tcp_thread_.join();
    }
    if (!tcp_thread_running_.exchange(true)) {
        tcp_connected_.store(false);
        tcp_thread_ = std::thread([this]() {
            TcpSocket::socketThread(this, transport_config_, &tcp_thread_running_, &tcp_connected_);
            onTcpThreadExited();
        });
    }
}

void DataBuffer::stopTcpThread() {
    const bool was_running = tcp_thread_running_.exchange(false);
    if (was_running) {
        wr_cv_.notify_all();
        rd_cv_.notify_all();
    }
    if (tcp_thread_.joinable()) {
        tcp_thread_.join();
    }
    tcp_connected_.store(false);
}

void DataBuffer::setTransportConfig(const TcpSocket::Config& config) {
    transport_config_ = config;
}

bool DataBuffer::waitUntilIdle(std::chrono::milliseconds timeout)
{
    if (!tcp_thread_running_.load()) {
        return true;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (tcp_thread_running_.load()) {
        bool read_queue_empty = false;
        size_t write_queue_size = 0;
        {
            std::lock_guard<std::mutex> rd_lock(rd_mutex_);
            read_queue_empty = rd_queue_.empty();
        }
        {
            std::lock_guard<std::mutex> wr_lock(wr_mutex_);
            write_queue_size = wr_queue_.size();
        }

        if (read_queue_empty && write_queue_size >= static_cast<size_t>(DATA_BUF_NUM - 1)) {
            return true;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return false;
}

std::string DataBuffer::describeState()
{
    bool running = tcp_thread_running_.load();
    bool connected = tcp_connected_.load();
    size_t read_queue_size = 0;
    size_t write_queue_size = 0;
    {
        std::lock_guard<std::mutex> rd_lock(rd_mutex_);
        read_queue_size = rd_queue_.size();
    }
    {
        std::lock_guard<std::mutex> wr_lock(wr_mutex_);
        write_queue_size = wr_queue_.size();
    }

    return "running=" + std::string(running ? "true" : "false") +
           ", connected=" + std::string(connected ? "true" : "false") +
           ", read_queue=" + std::to_string(read_queue_size) +
           ", write_queue=" + std::to_string(write_queue_size) +
           ", ptr=" + std::to_string(ptr_);
}

std::string DataBuffer::getLastTransportFeedback() const
{
    return TcpSocket::getLastFeedback();
}

void DataBuffer::onTcpThreadExited()
{
    tcp_thread_running_.store(false);
    tcp_connected_.store(false);
    wr_cv_.notify_all();
    rd_cv_.notify_all();
}

void DataBuffer::handleBufferFilled() {
    if (ptr_ >= DATA_BUF_SIZE) {
        if (buffer_lengths_[wr_ptr_] == 0) {
            buffer_lengths_[wr_ptr_] = static_cast<size_t>(ptr_);
        }
        if (!tcp_thread_running_.load()) {
            spdlog::info("启动TCP线程");
            startTcpThread();
        }
        spdlog::info("写入成功，缓冲区: {}, 有效字节: {}", wr_ptr_, buffer_lengths_[wr_ptr_]);
        writeEnd(wr_ptr_);
        int next_wr_ptr = 0;
        if (!getWriteBuf(&next_wr_ptr)) {
            throw std::runtime_error("TCP线程已停止，无法继续获取写缓冲区");
        }
        wr_ptr_ = next_wr_ptr;
        ptr_ = 0;
        buffer_lengths_[wr_ptr_] = 0;
    }
}

void DataBuffer::handleBegin() {
    for (int i = 0; i < 2; ++i) {
        addData(0, 0, 0, 0, 0, 0xFF00, 0, 0);
        addProcessJumpData(0x8000, 0x8000, 0x8000, 0x8000, 0x8000);
        addData(0, 0, 0, 0, 0, 0x1100, 0, 0);
        forceFill();
    }
}

double DataBuffer::powerToValue(double power) const {
    // 从C#代码移植的功率转换公式
    return 5.5366 + 2.67805 * power - 0.107836 * std::pow(power, 2) +
           0.00241519 * std::pow(power, 3) - 0.0000248153 * std::pow(power, 4) +
           0.0000000964112 * std::pow(power, 5);
}

} // namespace nbcam
