#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace nbcam {

class DataBuffer;

class TcpSocket {
public:
    struct Config {
        std::string host = "192.168.1.10";
        uint16_t port = 7;
        int connect_retry_ms = 100;
        int max_connect_attempts = 5;
        int io_timeout_ms = 200;
    };

    static void socketThread(DataBuffer* buffer,
                             const Config& config,
                             std::atomic<bool>* running,
                             std::atomic<bool>* connected);

    static void updateLastFeedback(const std::string& feedback);
    static std::string getLastFeedback();

private:
    static bool tryConnect(int& sockfd, const Config& config, const std::atomic<bool>* running);

    static std::mutex feedback_mutex_;
    static std::string last_feedback_;
};

} // namespace nbcam
