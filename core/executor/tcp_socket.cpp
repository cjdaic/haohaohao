#include "tcp_socket.h"
#include "data_buffer.h"
#include <algorithm>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <string>
#include <limits>

#pragma comment(lib, "ws2_32.lib")

namespace nbcam {

std::mutex TcpSocket::feedback_mutex_;
std::string TcpSocket::last_feedback_;

namespace {

int recvAvailable(SOCKET sock, char* buffer, int buffer_size)
{
    u_long available = 0;
    if (ioctlsocket(sock, FIONREAD, &available) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    if (available == 0) {
        return 0;
    }
    return recv(sock, buffer, (std::min)(buffer_size, static_cast<int>(available)), 0);
}

} // namespace

void TcpSocket::updateLastFeedback(const std::string& feedback)
{
    std::lock_guard<std::mutex> lock(feedback_mutex_);
    last_feedback_ = feedback;
}

std::string TcpSocket::getLastFeedback()
{
    std::lock_guard<std::mutex> lock(feedback_mutex_);
    return last_feedback_;
}

void TcpSocket::socketThread(DataBuffer* buffer,
                             const Config& config,
                             std::atomic<bool>* running,
                             std::atomic<bool>* connected) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        spdlog::error("WSAStartup失败");
        return;
    }
    
    int sockfd = -1;
    char readData[128];
    bool board_ready_for_buffer = false;
    
    while (running && running->load()) {
        try {
            if (sockfd < 0) {
                if (!tryConnect(sockfd, config, running)) {
                    break;
                }
                if (sockfd < 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(config.connect_retry_ms));
                    continue;
                }

                BOOL keepAlive = TRUE;
                setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepAlive), sizeof(keepAlive));

                const DWORD timeout_value = static_cast<DWORD>((std::max)(1, config.io_timeout_ms));
                setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_value), sizeof(timeout_value));
                setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_value), sizeof(timeout_value));

                if (connected) {
                    connected->store(true);
                }
                board_ready_for_buffer = false;
                updateLastFeedback("connected");
            }

            if (buffer->hasPendingReadBuf()) {
                int bytesRead = 0;
                if (!board_ready_for_buffer) {
                    bytesRead = recv(sockfd, readData, sizeof(readData), 0);
                    if (bytesRead == SOCKET_ERROR) {
                        const int recv_error = WSAGetLastError();
                        if (recv_error == WSAETIMEDOUT || recv_error == WSAEWOULDBLOCK) {
                            continue;
                        }
                        throw std::runtime_error("接收失败, code=" + std::to_string(recv_error));
                    }
                    if (bytesRead == 0) {
                        throw std::runtime_error("板卡关闭连接");
                    }
                    board_ready_for_buffer = true;
                }

                int rd_ptr = 0;
                if (!buffer->tryGetReadBuf(&rd_ptr)) {
                    continue;
                }

                const auto& buf = buffer->getBuffer(rd_ptr);
                const size_t buf_len = buffer->getBufferLength(rd_ptr);
                if (buf_len == 0) {
                    buffer->readEnd(rd_ptr);
                    continue;
                }

                buffer->markSendBegin();
                bool send_completed = false;
                try {
                    int totalSent = 0;
                    while (totalSent < static_cast<int>(buf_len)) {
                        const int bytesSent = send(sockfd,
                                                   reinterpret_cast<const char*>(buf.data()) + totalSent,
                                                   static_cast<int>(buf_len) - totalSent,
                                                   0);
                        if (bytesSent == SOCKET_ERROR) {
                            throw std::runtime_error("发送失败, code=" + std::to_string(WSAGetLastError()));
                        }
                        if (bytesSent == 0) {
                            throw std::runtime_error("发送返回0字节");
                        }
                        totalSent += bytesSent;
                    }

                    updateLastFeedback("board ready, sent " + std::to_string(totalSent) + " bytes");
                    board_ready_for_buffer = false;
                    send_completed = true;
                } catch (...) {
                    buffer->markSendEnd();
                    buffer->readEnd(rd_ptr);
                    throw;
                }
                buffer->markSendEnd();
                buffer->readEnd(rd_ptr);
                if (!send_completed) {
                    continue;
                }
                continue;
            }

            // 无待发送缓冲时只探测一次板卡请求，并保留这个请求给下一块数据使用。
            if (!board_ready_for_buffer) {
                int bytesRead = recvAvailable(sockfd, readData, sizeof(readData));
                if (bytesRead == SOCKET_ERROR) {
                    const int recv_error = WSAGetLastError();
                    if (recv_error == WSAETIMEDOUT || recv_error == WSAEWOULDBLOCK) {
                        continue;
                    }
                    throw std::runtime_error("接收失败, code=" + std::to_string(recv_error));
                }
                if (bytesRead > 0) {
                    board_ready_for_buffer = true;
                    updateLastFeedback("board ready");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            
        } catch (const std::exception& e) {
            spdlog::error("TCP通信错误: {}", e.what());
            updateLastFeedback(std::string("error: ") + e.what());
            if (sockfd >= 0) {
                closesocket(sockfd);
                sockfd = -1;
            }
            board_ready_for_buffer = false;
            if (connected) {
                connected->store(false);
            }
            if (running && running->load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(config.connect_retry_ms));
            }
        }
    }
    
    if (sockfd >= 0) {
        shutdown(sockfd, SD_BOTH);
        closesocket(sockfd);
    }
    if (connected) {
        connected->store(false);
    }
    WSACleanup();
}

bool TcpSocket::tryConnect(int& sockfd, const Config& config, const std::atomic<bool>* running) {
    const int max_attempts = config.max_connect_attempts <= 0
        ? (std::numeric_limits<int>::max)()
        : config.max_connect_attempts;
    int attempt = 0;

    while ((!running || running->load()) && attempt < max_attempts) {
        ++attempt;
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            spdlog::error("创建socket失败");
            updateLastFeedback("socket create failed");
            return false;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(config.port);

        if (inet_pton(AF_INET, config.host.c_str(), &serverAddr.sin_addr) <= 0) {
            spdlog::error("无效的地址: {}", config.host);
            updateLastFeedback("invalid address: " + config.host);
            closesocket(sockfd);
            sockfd = -1;
            return false;
        }

        if (connect(sockfd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == 0) {
            spdlog::info("TCP连接成功: {}:{} (attempt {}/{})",
                         config.host,
                         config.port,
                         attempt,
                         max_attempts == (std::numeric_limits<int>::max)() ? -1 : max_attempts);
            updateLastFeedback("connected to " + config.host + ":" + std::to_string(config.port));
            return true;
        }

        const int error_code = WSAGetLastError();
        const bool has_more_attempts = attempt < max_attempts;
        if (has_more_attempts) {
            spdlog::warn("连接{}:{}失败，第{}/{}次，{}ms后重试，错误码={}",
                         config.host,
                         config.port,
                         attempt,
                         max_attempts,
                         config.connect_retry_ms,
                         error_code);
            updateLastFeedback("connect failed to " + config.host + ":" + std::to_string(config.port) +
                               ", attempt " + std::to_string(attempt) + "/" + std::to_string(max_attempts) +
                               ", code=" + std::to_string(error_code));
        } else {
            spdlog::error("连接{}:{}失败，已达到最大重试次数({})，停止连接，错误码={}",
                          config.host,
                          config.port,
                          max_attempts,
                          error_code);
            updateLastFeedback("connect failed to " + config.host + ":" + std::to_string(config.port) +
                               ", attempt " + std::to_string(attempt) + "/" + std::to_string(max_attempts) +
                               ", code=" + std::to_string(error_code) + ", stopped");
        }
        closesocket(sockfd);
        sockfd = -1;
        if (!running || !running->load()) {
            return false;
        }
        if (has_more_attempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config.connect_retry_ms));
        }
    }

    if (running && !running->load()) {
        updateLastFeedback("connect stopped");
    }
    return false;
}

} // namespace nbcam
