#include "frame_log_utils.h"
#include "data_generator.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace nbcam {

namespace {

constexpr double kAxisScale = 1000.0;      // mm -> count
constexpr double kAxisCenter = 32768.0;    // 中心偏置
constexpr uint16_t kOpcodeMark = 0x00FF;
constexpr uint16_t kOpcodeJump = 0x0000;
constexpr uint16_t kOpcodeBegin = 0xFF00;
constexpr uint16_t kOpcodeEnd = 0x1100;
constexpr int kDelayTickUs = 10;

std::string nowTimestamp()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto tt = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm{};
    localtime_s(&tm, &tt);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << "." << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

std::string nowFileStamp()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto tt = system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &tt);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

void writeLogLine(std::ofstream& ofs, const std::string& message)
{
    ofs << "[" << nowTimestamp() << "] " << message << "\n";
}

bool ensureLogDir(const std::string& log_dir)
{
    if (log_dir.empty()) {
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    return !ec;
}

uint16_t clampAxisToU16(double mm_value)
{
    double raw = kAxisCenter;
    if (std::isfinite(mm_value)) {
        raw = mm_value * kAxisScale + kAxisCenter;
    }
    raw = std::clamp(raw, 0.0, 65535.0);
    return static_cast<uint16_t>(std::llround(raw));
}

std::array<uint16_t, 8> makeDataFrame(uint16_t x, uint16_t y, uint16_t z, bool is_jump)
{
    // arg1..arg8 = B, A, Z, Y, X, OPCODE, arg7, arg8
    return {0, 0, z, y, x, is_jump ? kOpcodeJump : kOpcodeMark, 0, 0};
}

std::array<uint16_t, 8> makeOpcodeFrame(uint16_t opcode)
{
    return {0, 0, 0, 0, 0, opcode, 0, 0};
}

std::array<uint16_t, 8> makeRawFrame(uint16_t arg1,
                                     uint16_t arg2,
                                     uint16_t arg3,
                                     uint16_t arg4,
                                     uint16_t arg5,
                                     uint16_t arg6,
                                     uint16_t arg7 = 0,
                                     uint16_t arg8 = 0)
{
    return {arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8};
}

std::string formatFrame(const std::array<uint16_t, 8>& frame)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < frame.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << frame[i];
    }
    oss << "]";
    return oss.str();
}

std::array<uint8_t, 16> encodeFrameBytes(const std::array<uint16_t, 8>& frame)
{
    std::array<uint8_t, 16> bytes{};
    for (size_t i = 0; i < frame.size(); ++i) {
        bytes[i * 2] = static_cast<uint8_t>(frame[i] & 0xFF);
        bytes[i * 2 + 1] = static_cast<uint8_t>((frame[i] >> 8) & 0xFF);
    }
    return bytes;
}

std::string hexDump(const char* data, int length)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < length; ++i) {
        if (i > 0) {
            oss << " ";
        }
        oss << std::setw(2) << (static_cast<unsigned int>(static_cast<unsigned char>(data[i])));
    }
    return oss.str();
}

struct EndpointParseResult
{
    bool ok = false;
    std::string host;
    uint16_t port = 0;
    std::string feedback;
};

struct FrameSendResult
{
    bool ok = false;
    std::string feedback;
};

std::string trimString(const std::string& text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

EndpointParseResult parseEndpoint(const std::string& endpoint)
{
    EndpointParseResult result;
    std::string normalized = trimString(endpoint);
    const auto schema_pos = normalized.find("://");
    if (schema_pos != std::string::npos) {
        normalized = normalized.substr(schema_pos + 3);
    }
    const auto slash_pos = normalized.find('/');
    if (slash_pos != std::string::npos) {
        normalized = normalized.substr(0, slash_pos);
    }
    const auto colon_pos = normalized.rfind(':');
    if (colon_pos == std::string::npos || colon_pos == 0 || colon_pos + 1 >= normalized.size()) {
        result.feedback = "invalid endpoint format, require host:port";
        return result;
    }

    const std::string host = trimString(normalized.substr(0, colon_pos));
    const std::string port_text = trimString(normalized.substr(colon_pos + 1));
    if (host.empty() || port_text.empty()) {
        result.feedback = "empty host or port";
        return result;
    }

    char* end_ptr = nullptr;
    const long parsed_port = std::strtol(port_text.c_str(), &end_ptr, 10);
    if (!end_ptr || *end_ptr != '\0' || parsed_port <= 0 || parsed_port > 65535) {
        result.feedback = "invalid port: " + port_text;
        return result;
    }

    result.ok = true;
    result.host = host;
    result.port = static_cast<uint16_t>(parsed_port);
    result.feedback = "ok";
    return result;
}

std::string winSockErrorString(int code)
{
    char* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD len = FormatMessageA(flags, nullptr, static_cast<DWORD>(code), 0, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
    std::ostringstream oss;
    oss << "code=" << code;
    if (len > 0 && buffer) {
        std::string msg(buffer);
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n')) {
            msg.pop_back();
        }
        LocalFree(buffer);
        oss << ", msg=" << msg;
    }
    return oss.str();
}

bool socketConnectWithTimeout(SOCKET sock, const sockaddr* addr, int addr_len, int timeout_ms, std::string* out_feedback)
{
    u_long non_blocking = 1;
    if (ioctlsocket(sock, FIONBIO, &non_blocking) != 0) {
        if (out_feedback) {
            *out_feedback = "set non-blocking failed: " + winSockErrorString(WSAGetLastError());
        }
        return false;
    }

    int rc = connect(sock, addr, addr_len);
    if (rc != 0) {
        const int connect_error = WSAGetLastError();
        if (connect_error != WSAEWOULDBLOCK && connect_error != WSAEINPROGRESS && connect_error != WSAEINVAL) {
            if (out_feedback) {
                *out_feedback = "connect failed: " + winSockErrorString(connect_error);
            }
            return false;
        }
    }

    fd_set write_set;
    fd_set err_set;
    FD_ZERO(&write_set);
    FD_ZERO(&err_set);
    FD_SET(sock, &write_set);
    FD_SET(sock, &err_set);
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    const int select_rc = select(0, nullptr, &write_set, &err_set, &tv);
    if (select_rc <= 0) {
        if (out_feedback) {
            if (select_rc == 0) {
                *out_feedback = "connect timeout";
            } else {
                *out_feedback = "select failed: " + winSockErrorString(WSAGetLastError());
            }
        }
        return false;
    }

    int so_error = 0;
    int so_len = sizeof(so_error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &so_len) != 0 || so_error != 0) {
        if (out_feedback) {
            const int code = (so_error != 0) ? so_error : WSAGetLastError();
            *out_feedback = "socket error: " + winSockErrorString(code);
        }
        return false;
    }

    non_blocking = 0;
    ioctlsocket(sock, FIONBIO, &non_blocking);
    return true;
}

FrameSendResult sendFrameToEndpoint(const std::string& host,
                                    uint16_t port,
                                    const std::array<uint8_t, 16>& payload,
                                    int timeout_ms)
{
    FrameSendResult result;

    WSADATA wsa_data{};
    const int startup_rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (startup_rc != 0) {
        result.feedback = "WSAStartup failed: " + std::to_string(startup_rc);
        return result;
    }

    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addr_list = nullptr;
    const std::string port_text = std::to_string(port);
    const int gai_rc = getaddrinfo(host.c_str(), port_text.c_str(), &hints, &addr_list);
    if (gai_rc != 0 || !addr_list) {
        result.feedback = "getaddrinfo failed: " + std::string(gai_strerrorA(gai_rc));
        WSACleanup();
        return result;
    }

    std::string feedback = "unreachable";
    for (addrinfo* it = addr_list; it != nullptr; it = it->ai_next) {
        SOCKET sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock == INVALID_SOCKET) {
            feedback = "socket create failed: " + winSockErrorString(WSAGetLastError());
            continue;
        }

        std::string connect_feedback;
        if (!socketConnectWithTimeout(sock, it->ai_addr, static_cast<int>(it->ai_addrlen), timeout_ms, &connect_feedback)) {
            feedback = connect_feedback;
            closesocket(sock);
            continue;
        }

        DWORD timeout_value = static_cast<DWORD>(timeout_ms);
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_value), sizeof(timeout_value));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_value), sizeof(timeout_value));

        int sent_total = 0;
        while (sent_total < static_cast<int>(payload.size())) {
            const int sent = send(sock,
                                  reinterpret_cast<const char*>(payload.data()) + sent_total,
                                  static_cast<int>(payload.size()) - sent_total,
                                  0);
            if (sent <= 0) {
                feedback = "send failed: " + winSockErrorString(WSAGetLastError());
                break;
            }
            sent_total += sent;
        }

        if (sent_total == static_cast<int>(payload.size())) {
            char recv_buf[128] = {0};
            const int recv_size = recv(sock, recv_buf, static_cast<int>(sizeof(recv_buf)), 0);
            std::ostringstream oss;
            oss << "connect+send ok, bytes=" << sent_total;
            if (recv_size > 0) {
                oss << ", recv_hex=" << hexDump(recv_buf, recv_size);
            } else if (recv_size == 0) {
                oss << ", recv=peer_closed";
            } else {
                oss << ", recv=none(" << winSockErrorString(WSAGetLastError()) << ")";
            }
            feedback = oss.str();
            result.ok = true;
        }

        closesocket(sock);
        if (result.ok) {
            break;
        }
    }

    freeaddrinfo(addr_list);
    WSACleanup();
    result.feedback = feedback;
    return result;
}

uint16_t mmToFrameValue(double mm)
{
    if (!std::isfinite(mm)) {
        return 0;
    }
    const double raw = std::clamp(std::round(mm * 1000.0 + 32768.0), 0.0, 65535.0);
    return static_cast<uint16_t>(raw);
}

double powerToValueForFrame(double power)
{
    return 5.5366 + 2.67805 * power - 0.107836 * std::pow(power, 2) +
           0.00241519 * std::pow(power, 3) - 0.0000248153 * std::pow(power, 4) +
           0.0000000964112 * std::pow(power, 5);
}

int resolveDelayUsFromParams(const ProcessParams* params, bool laser_on, int fallback_us)
{
    if (params) {
        const int64_t override_us = laser_on ? params->laser_on_delay_us : params->laser_off_delay_us;
        if (override_us > 0) {
            return static_cast<int>((std::min)(override_us,
                                               static_cast<int64_t>((std::numeric_limits<int>::max)())));
        }
    }
    return (std::max)(0, fallback_us);
}

double resolveSpeedMmS(const ProcessParams* params, double fallback_mm_s)
{
    if (params && std::isfinite(params->speed_mm_s) && params->speed_mm_s > 1e-6) {
        return params->speed_mm_s;
    }
    if (std::isfinite(fallback_mm_s) && fallback_mm_s > 1e-6) {
        return fallback_mm_s;
    }
    return 300.0;
}

int computeInterpolationSamples(uint16_t x0, uint16_t y0, uint16_t z0,
                                uint16_t x1, uint16_t y1, uint16_t z1,
                                double speed_mm_s)
{
    const double safe_speed_mm_s = (std::max)(1e-6, speed_mm_s);
    const double speed_mm_per_s = safe_speed_mm_s;
    constexpr double kTickSec = 0.00001;

    const double dx = static_cast<double>(static_cast<int>(x1) - static_cast<int>(x0));
    const double dy = static_cast<double>(static_cast<int>(y1) - static_cast<int>(y0));
    const double dz = static_cast<double>(static_cast<int>(z1) - static_cast<int>(z0));
    double length_xy = 0.001 * std::sqrt(dx * dx + dy * dy);
    const double length_z = std::abs(0.001 * dz);

    int n_max = 1;
    if (length_xy > 1e-12) {
        n_max = static_cast<int>((length_xy / (speed_mm_per_s * kTickSec)) + 1.0);
    } else if (length_z > 1e-12) {
        n_max = static_cast<int>((length_z / (speed_mm_per_s * kTickSec)) + 1.0);
    }
    return (std::max)(1, n_max);
}

uint16_t interpolateAxis(uint16_t start, uint16_t end, int idx, int count)
{
    if (count <= 0) {
        return end;
    }
    const double ratio = static_cast<double>(idx) / static_cast<double>(count);
    const double raw = std::round(static_cast<double>(start) +
                                  (static_cast<double>(end) - static_cast<double>(start)) * ratio);
    return static_cast<uint16_t>(std::clamp(raw, 0.0, 65535.0));
}

void appendInterpolatedFrames(uint16_t x0, uint16_t y0, uint16_t z0,
                              uint16_t x1, uint16_t y1, uint16_t z1,
                              double speed_mm_s,
                              bool mark_segment,
                              int laser_on_delay_us,
                              int* io_mark_elapsed_us,
                              std::vector<std::array<uint16_t, 8>>& frames)
{
    const int samples = computeInterpolationSamples(x0, y0, z0, x1, y1, z1, speed_mm_s);
    int mark_elapsed_us = io_mark_elapsed_us ? *io_mark_elapsed_us : 0;

    for (int i = 1; i <= samples; ++i) {
        const uint16_t x = interpolateAxis(x0, x1, i, samples);
        const uint16_t y = interpolateAxis(y0, y1, i, samples);
        const uint16_t z = interpolateAxis(z0, z1, i, samples);
        if (mark_segment) {
            const bool is_jump = (mark_elapsed_us < laser_on_delay_us);
            frames.push_back(makeDataFrame(x, y, z, is_jump));
            mark_elapsed_us += kDelayTickUs;
        } else {
            frames.push_back(makeDataFrame(x, y, z, true));
        }
    }

    if (io_mark_elapsed_us) {
        *io_mark_elapsed_us = mark_elapsed_us;
    }
}

void appendDelayFrames(uint16_t x, uint16_t y, uint16_t z,
                       int delay_us,
                       int delay_on_us,
                       std::vector<std::array<uint16_t, 8>>& frames)
{
    const int safe_delay_us = (std::max)(0, delay_us);
    const int safe_delay_on_us = (std::max)(0, delay_on_us);
    const int total_ticks = safe_delay_us / kDelayTickUs;
    const int mark_ticks = (std::min)(total_ticks, safe_delay_on_us / kDelayTickUs);
    for (int i = 0; i < total_ticks; ++i) {
        const bool is_jump = (i >= mark_ticks);
        frames.push_back(makeDataFrame(x, y, z, is_jump));
    }
}

void appendSetFreqFrames(int freq, std::vector<std::array<uint16_t, 8>>& frames)
{
    const int safe_freq = (std::max)(1, freq);
    const int cnt = 50000 / safe_freq;
    frames.push_back(makeRawFrame(0x00AA, 0, 0, 0, 0, 0xAA00, 0, 0));
    frames.push_back(makeRawFrame(static_cast<uint16_t>(cnt & 0xFFFF),
                                  static_cast<uint16_t>((cnt >> 16) & 0xFFFF),
                                  0, 0, 0, 0, 0, 0));
    frames.push_back(makeRawFrame(0x00AA, 0, 0, 0, 0, 0x5500, 0, 0));
}

void appendSetPowerFrames(double power, std::vector<std::array<uint16_t, 8>>& frames)
{
    if (power > 100.0) {
        power = 100.0;
    }

    const double p_value = powerToValueForFrame(power);
    const uint16_t p = static_cast<uint16_t>(p_value * 65535.0 / 100.0);
    for (int i = 0; i < 10; ++i) {
        frames.push_back(makeRawFrame(p, 0, 0, 0, 0, 0xBB00, 0, 11451));
    }
}

void appendHandleBeginFrames(std::vector<std::array<uint16_t, 8>>& frames)
{
    for (int i = 0; i < 2; ++i) {
        frames.push_back(makeOpcodeFrame(kOpcodeBegin));
        frames.push_back(makeRawFrame(0x8000, 0x8000, 0x8000, 0x8000, 0x8000, kOpcodeJump, 0, 0));
        frames.push_back(makeOpcodeFrame(kOpcodeEnd));
    }
}

void appendProcessBeginFrames(std::vector<std::array<uint16_t, 8>>& frames)
{
    appendHandleBeginFrames(frames);
    frames.push_back(makeOpcodeFrame(kOpcodeBegin));
}

void appendProcessEndFrame(std::vector<std::array<uint16_t, 8>>& frames)
{
    frames.push_back(makeOpcodeFrame(kOpcodeEnd));
}

void appendJobFramesWithParams(const LaserJob& job, std::vector<std::array<uint16_t, 8>>& frames)
{
    int active_freq = static_cast<int>(std::llround(job.process_defaults.freq_hz));
    double active_power = job.process_defaults.power_w;

    appendSetFreqFrames(active_freq, frames);
    appendSetPowerFrames(active_power, frames);
    appendProcessBeginFrames(frames);

    for (const auto& segment : job.segments) {
        const ProcessParams* segment_params = segment.params_override ? segment.params_override.get()
                                                                      : &job.process_defaults;
        if (segment.points.empty()) {
            continue;
        }

        uint16_t prev_x = 0;
        uint16_t prev_y = 0;
        uint16_t prev_z = 0;
        bool has_prev = false;
        int mark_elapsed_us = 0;

        for (size_t point_index = 0; point_index < segment.points.size(); ++point_index) {
            const auto& point = segment.points[point_index];
            const ProcessParams* point_params = point.params_override ? point.params_override.get() : segment_params;
            if (point_params) {
                if (std::abs(point_params->freq_hz - static_cast<double>(active_freq)) > 1.0) {
                    active_freq = static_cast<int>(std::llround(point_params->freq_hz));
                    appendSetFreqFrames(active_freq, frames);
                }
                if (std::abs(point_params->power_w - active_power) > 0.1) {
                    active_power = point_params->power_w;
                    appendSetPowerFrames(active_power, frames);
                }
            }

            const uint16_t x = mmToFrameValue(point.x);
            const uint16_t y = mmToFrameValue(point.y);
            const uint16_t z = mmToFrameValue(point.z);
            const bool point_is_jump = (segment.type == SegmentType::JUMP) || (point.laser == 0);

            if (!has_prev) {
                frames.push_back(makeDataFrame(x, y, z, true));
                prev_x = x;
                prev_y = y;
                prev_z = z;
                has_prev = true;
                mark_elapsed_us = 0;
                continue;
            }

            if (point_is_jump) {
                appendInterpolatedFrames(prev_x,
                                         prev_y,
                                         prev_z,
                                         x,
                                         y,
                                         z,
                                         static_cast<double>((std::max)(1, DataGenerator::JUMP_SPEED)),
                                         false,
                                         0,
                                         nullptr,
                                         frames);
            } else {
                const int laser_on_delay_us = resolveDelayUsFromParams(point_params, true, DataGenerator::LASER_ON_DELAY);
                const double mark_speed = resolveSpeedMmS(point_params, segment_params ? segment_params->speed_mm_s : job.process_defaults.speed_mm_s);
                appendInterpolatedFrames(prev_x,
                                         prev_y,
                                         prev_z,
                                         x,
                                         y,
                                         z,
                                         mark_speed,
                                         true,
                                         laser_on_delay_us,
                                         &mark_elapsed_us,
                                         frames);
            }

            prev_x = x;
            prev_y = y;
            prev_z = z;
        }

        if (has_prev) {
            if (segment.type == SegmentType::JUMP) {
                appendDelayFrames(prev_x, prev_y, prev_z, DataGenerator::JUMP_DELAY, 0, frames);
            } else {
                const ProcessParams* tail_params =
                    segment.points.back().params_override ? segment.points.back().params_override.get() : segment_params;
                const int laser_off_delay_us = resolveDelayUsFromParams(tail_params, false, DataGenerator::LASER_OFF_DELAY);
                appendDelayFrames(prev_x, prev_y, prev_z, DataGenerator::MARK_DELAY, laser_off_delay_us, frames);
            }
        }
    }

    appendProcessEndFrame(frames);
}

void writeFrameOnlyLine(std::ofstream& ofs, const std::array<uint16_t, 8>& frame)
{
    ofs << frame[0] << ","
        << frame[1] << ","
        << frame[2] << ","
        << frame[3] << ","
        << frame[4] << ","
        << frame[5] << ","
        << frame[6] << ","
        << frame[7] << "\n";
}

}  // namespace

bool exportJobFramesForDemo(const LaserJob& job,
                            const std::string& log_dir,
                            std::string* out_log_path,
                            std::array<uint16_t, 8>* out_first_data_frame)
{
    if (!job.isValid() || !ensureLogDir(log_dir)) {
        return false;
    }

    const std::filesystem::path log_path =
        std::filesystem::path(log_dir) / ("example1_frame_map_" + nowFileStamp() + ".log");
    std::ofstream ofs(log_path.string(), std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    if (out_log_path) {
        *out_log_path = log_path.string();
    }

    writeLogLine(ofs, "Example1 path -> frame mapping start");
    writeLogLine(ofs, "Rule: ignore freq/power/speed changes; A/B fixed to 0; only Begin/Mark/Jump/End.");
    writeLogLine(ofs, "Axis encode: encoded = clamp(round(mm * 1000 + 32768), 0, 65535).");

    size_t mark_count = 0;
    size_t jump_count = 0;
    size_t skipped_duplicate_count = 0;
    size_t skipped_invalid_count = 0;
    bool first_data_frame_set = false;

    const auto begin_frame = makeOpcodeFrame(kOpcodeBegin);
    writeLogLine(ofs, "OP=BEGIN frame=" + formatFrame(begin_frame));

    for (const auto& segment : job.segments) {
        std::array<uint16_t, 8> prev_frame{};
        bool has_prev_frame = false;

        for (size_t point_index = 0; point_index < segment.points.size(); ++point_index) {
            const auto& point = segment.points[point_index];
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z) ||
                !std::isfinite(point.u) || !std::isfinite(point.v)) {
                ++skipped_invalid_count;
                continue;
            }

            const bool is_jump = (segment.type == SegmentType::JUMP) || (point.laser == 0);
            const uint16_t x = clampAxisToU16(point.x);
            const uint16_t y = clampAxisToU16(point.y);
            const uint16_t z = clampAxisToU16(point.z);
            const auto frame = makeDataFrame(x, y, z, is_jump);

            if (has_prev_frame && frame == prev_frame) {
                ++skipped_duplicate_count;
                continue;
            }

            if (!first_data_frame_set && out_first_data_frame) {
                *out_first_data_frame = frame;
                first_data_frame_set = true;
            }

            std::ostringstream line;
            line << "OP=" << (is_jump ? "JUMP" : "MARK")
                 << " seg_id=" << segment.id
                 << " point_idx=" << point_index
                 << " xyz=(" << std::fixed << std::setprecision(4)
                 << point.x << ", " << point.y << ", " << point.z << ")"
                 << " uv=(" << point.u << ", " << point.v << ")"
                 << " frame=" << formatFrame(frame);
            writeLogLine(ofs, line.str());

            if (is_jump) {
                ++jump_count;
            } else {
                ++mark_count;
            }
            prev_frame = frame;
            has_prev_frame = true;
        }
    }

    const auto end_frame = makeOpcodeFrame(kOpcodeEnd);
    writeLogLine(ofs, "OP=END frame=" + formatFrame(end_frame));

    const size_t total_frames = 2 + mark_count + jump_count;
    const double estimated_ms = static_cast<double>(total_frames) * 0.01;  // 10us/frame
    std::ostringstream summary;
    summary << "Summary: total_frames=" << total_frames
            << ", mark=" << mark_count
            << ", jump=" << jump_count
            << ", skipped_duplicate=" << skipped_duplicate_count
            << ", skipped_invalid=" << skipped_invalid_count
            << ", estimated_time_ms=" << std::fixed << std::setprecision(3) << estimated_ms;
    writeLogLine(ofs, summary.str());

    if (!first_data_frame_set && out_first_data_frame) {
        *out_first_data_frame = begin_frame;
    }

    writeLogLine(ofs, "Example1 path -> frame mapping end");
    return true;
}

bool sendFrameGrpcTcpDemo(const std::array<uint16_t, 8>& frame,
                          const std::string& log_dir,
                          std::string* out_log_path,
                          const std::string& grpc_endpoint,
                          const std::string& tcp_host,
                          uint16_t tcp_port)
{
    if (!ensureLogDir(log_dir)) {
        return false;
    }

    const std::filesystem::path log_path =
        std::filesystem::path(log_dir) / ("grpc_tcp_demo_" + nowFileStamp() + ".log");
    std::ofstream ofs(log_path.string(), std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    if (out_log_path) {
        *out_log_path = log_path.string();
    }

    writeLogLine(ofs, "gRPC/TCP single-frame demo start");
    writeLogLine(ofs, "GRPC_REQUEST endpoint=" + grpc_endpoint + " method=SendSingleFrame frame=" + formatFrame(frame));
    writeLogLine(ofs, "TCP_TARGET " + tcp_host + ":" + std::to_string(tcp_port));

    int grpc_code = -1;
    std::string grpc_message;

    WSADATA wsa_data{};
    const int startup_ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (startup_ret != 0) {
        grpc_message = "WSAStartup failed, code=" + std::to_string(startup_ret);
        writeLogLine(ofs, "TCP_ERROR " + grpc_message);
        writeLogLine(ofs, "GRPC_REPLY code=-1 message=" + grpc_message);
        writeLogLine(ofs, "gRPC/TCP single-frame demo end");
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        const int err = WSAGetLastError();
        grpc_message = "socket create failed, code=" + std::to_string(err);
        writeLogLine(ofs, "TCP_ERROR " + grpc_message);
        writeLogLine(ofs, "GRPC_REPLY code=-2 message=" + grpc_message);
        writeLogLine(ofs, "gRPC/TCP single-frame demo end");
        WSACleanup();
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(tcp_port);
    if (inet_pton(AF_INET, tcp_host.c_str(), &server_addr.sin_addr) <= 0) {
        const int err = WSAGetLastError();
        grpc_message = "invalid tcp host, code=" + std::to_string(err);
        writeLogLine(ofs, "TCP_ERROR " + grpc_message);
        writeLogLine(ofs, "GRPC_REPLY code=-3 message=" + grpc_message);
        writeLogLine(ofs, "gRPC/TCP single-frame demo end");
        closesocket(sock);
        WSACleanup();
        return false;
    }

    writeLogLine(ofs, "TCP_CONNECT begin");
    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
        const int err = WSAGetLastError();
        grpc_message = "connect failed, code=" + std::to_string(err);
        writeLogLine(ofs, "TCP_ERROR " + grpc_message);
        writeLogLine(ofs, "GRPC_REPLY code=-4 message=" + grpc_message);
        writeLogLine(ofs, "gRPC/TCP single-frame demo end");
        closesocket(sock);
        WSACleanup();
        return false;
    }
    writeLogLine(ofs, "TCP_CONNECT success");

    BOOL keep_alive = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&keep_alive), sizeof(keep_alive));
    DWORD timeout_ms = 500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout_ms), sizeof(timeout_ms));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout_ms), sizeof(timeout_ms));

    char recv_buf[128] = {0};
    const int recv_pre = recv(sock, recv_buf, static_cast<int>(sizeof(recv_buf)), 0);
    if (recv_pre > 0) {
        writeLogLine(ofs, "TCP_FEEDBACK_PRE bytes=" + std::to_string(recv_pre) + " hex=" + hexDump(recv_buf, recv_pre));
    } else if (recv_pre == 0) {
        writeLogLine(ofs, "TCP_FEEDBACK_PRE peer_closed");
    } else {
        const int err = WSAGetLastError();
        writeLogLine(ofs, "TCP_FEEDBACK_PRE none, code=" + std::to_string(err));
    }

    const auto bytes = encodeFrameBytes(frame);
    int sent_total = 0;
    while (sent_total < static_cast<int>(bytes.size())) {
        const int sent = send(sock,
                              reinterpret_cast<const char*>(bytes.data()) + sent_total,
                              static_cast<int>(bytes.size()) - sent_total,
                              0);
        if (sent <= 0) {
            const int err = WSAGetLastError();
            grpc_message = "send failed, code=" + std::to_string(err) + ", sent_total=" + std::to_string(sent_total);
            writeLogLine(ofs, "TCP_ERROR " + grpc_message);
            writeLogLine(ofs, "GRPC_REPLY code=-5 message=" + grpc_message);
            writeLogLine(ofs, "gRPC/TCP single-frame demo end");
            closesocket(sock);
            WSACleanup();
            return false;
        }
        sent_total += sent;
    }
    writeLogLine(ofs, "TCP_SEND bytes=" + std::to_string(sent_total) + " hex=" +
                          hexDump(reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size())));

    const int recv_post = recv(sock, recv_buf, static_cast<int>(sizeof(recv_buf)), 0);
    if (recv_post > 0) {
        writeLogLine(ofs, "TCP_FEEDBACK_POST bytes=" + std::to_string(recv_post) + " hex=" + hexDump(recv_buf, recv_post));
    } else if (recv_post == 0) {
        writeLogLine(ofs, "TCP_FEEDBACK_POST peer_closed");
    } else {
        const int err = WSAGetLastError();
        writeLogLine(ofs, "TCP_FEEDBACK_POST none, code=" + std::to_string(err));
    }

    closesocket(sock);
    WSACleanup();

    grpc_code = 0;
    grpc_message = "single frame forwarded to TCP";
    writeLogLine(ofs, "GRPC_REPLY code=" + std::to_string(grpc_code) + " message=" + grpc_message);
    writeLogLine(ofs, "gRPC/TCP single-frame demo end");
    return true;
}

bool sendFrameGrpcTcpConcurrent(const std::array<uint16_t, 8>& frame,
                                const std::string& log_dir,
                                std::string* out_log_path,
                                const std::string& grpc_endpoint,
                                const std::string& tcp_host,
                                uint16_t tcp_port,
                                int timeout_ms,
                                bool* out_grpc_ok,
                                std::string* out_grpc_feedback,
                                bool* out_tcp_ok,
                                std::string* out_tcp_feedback)
{
    if (!ensureLogDir(log_dir)) {
        return false;
    }

    const std::filesystem::path log_path =
        std::filesystem::path(log_dir) / ("grpc_tcp_concurrent_" + nowFileStamp() + ".log");
    std::ofstream ofs(log_path.string(), std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    if (out_log_path) {
        *out_log_path = log_path.string();
    }

    if (out_grpc_ok) {
        *out_grpc_ok = false;
    }
    if (out_tcp_ok) {
        *out_tcp_ok = false;
    }
    if (out_grpc_feedback) {
        *out_grpc_feedback = "not_started";
    }
    if (out_tcp_feedback) {
        *out_tcp_feedback = "not_started";
    }

    const EndpointParseResult grpc_target = parseEndpoint(grpc_endpoint);
    const auto bytes = encodeFrameBytes(frame);

    writeLogLine(ofs, "gRPC/TCP concurrent send start");
    writeLogLine(ofs, "FRAME " + formatFrame(frame));
    writeLogLine(ofs, "FRAME_HEX " + hexDump(reinterpret_cast<const char*>(bytes.data()),
                                             static_cast<int>(bytes.size())));
    writeLogLine(ofs, "GRPC_ENDPOINT " + grpc_endpoint);
    writeLogLine(ofs, "TCP_ENDPOINT " + tcp_host + ":" + std::to_string(tcp_port));
    writeLogLine(ofs, "TIMEOUT_MS " + std::to_string(timeout_ms));

    FrameSendResult grpc_result;
    FrameSendResult tcp_result;

    if (!grpc_target.ok) {
        grpc_result.ok = false;
        grpc_result.feedback = "endpoint parse failed: " + grpc_target.feedback;
        tcp_result = sendFrameToEndpoint(tcp_host, tcp_port, bytes, timeout_ms);
    } else {
        auto grpc_future = std::async(std::launch::async, [&]() {
            return sendFrameToEndpoint(grpc_target.host, grpc_target.port, bytes, timeout_ms);
        });
        auto tcp_future = std::async(std::launch::async, [&]() {
            return sendFrameToEndpoint(tcp_host, tcp_port, bytes, timeout_ms);
        });

        grpc_result = grpc_future.get();
        tcp_result = tcp_future.get();
    }

    if (out_grpc_ok) {
        *out_grpc_ok = grpc_result.ok;
    }
    if (out_tcp_ok) {
        *out_tcp_ok = tcp_result.ok;
    }
    if (out_grpc_feedback) {
        *out_grpc_feedback = grpc_result.feedback;
    }
    if (out_tcp_feedback) {
        *out_tcp_feedback = tcp_result.feedback;
    }

    writeLogLine(ofs, "GRPC_RESULT ok=" + std::string(grpc_result.ok ? "1" : "0") +
                           " feedback=" + grpc_result.feedback);
    writeLogLine(ofs, "TCP_RESULT ok=" + std::string(tcp_result.ok ? "1" : "0") +
                           " feedback=" + tcp_result.feedback);
    writeLogLine(ofs, "gRPC/TCP concurrent send end");

    return grpc_result.ok && tcp_result.ok;
}

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
                              std::string* out_log_path)
{
    if (!job.isValid() || !ensureLogDir(log_dir)) {
        return false;
    }

    const std::filesystem::path log_path =
        std::filesystem::path(log_dir) / ("connect_frame_dump_" + nowFileStamp() + ".log");
    std::ofstream ofs(log_path.string(), std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    if (out_log_path) {
        *out_log_path = log_path.string();
    }

    ofs << "# grpc endpoint=" << grpc_endpoint
        << " ok=" << (grpc_ok ? "1" : "0")
        << " feedback=" << grpc_feedback << "\n";
    ofs << "# tcp target=" << tcp_target
        << " ok=" << (tcp_ok ? "1" : "0")
        << " feedback=" << tcp_feedback << "\n";
    ofs << "# serial enabled=" << (serial_enabled ? "1" : "0")
        << " ok=" << (serial_ok ? "1" : "0")
        << " feedback=" << serial_feedback << "\n";

    std::vector<std::array<uint16_t, 8>> frames;
    frames.reserve(job.getTotalPointCount() + 64);
    appendJobFramesWithParams(job, frames);
    for (const auto& frame : frames) {
        writeFrameOnlyLine(ofs, frame);
    }

    return true;
}

}  // namespace nbcam
