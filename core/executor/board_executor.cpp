#include "board_executor.h"
#include "data_generator.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <spdlog/spdlog.h>

namespace nbcam {

namespace {

constexpr double kAxisEncodeBias = 32768.0;
constexpr double kAxisEncodeScale = 1000.0;
constexpr int kConnectionWaitMs = 2000;
constexpr int kConnectionPollMs = 50;
constexpr int kDelayTickUs = 10;
constexpr auto kTransportDrainTimeout = std::chrono::seconds(60);

std::string buildPointContext(size_t segment_index, size_t point_index, const char* axis_name)
{
    std::ostringstream oss;
    oss << "segment=" << segment_index << ", point=" << point_index << ", axis=" << axis_name;
    return oss.str();
}

int clampNonNegative(int value)
{
    return (std::max)(0, value);
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

}  // namespace

BoardExecutor::BoardExecutor()
{
    data_buffer_ = std::make_unique<DataBuffer>();
}

BoardExecutor::~BoardExecutor()
{
    stop();
}

void BoardExecutor::setMachineConfig(const MachineConfig& config)
{
    machine_config_ = config;
    machine_config_.delay.jump_speed_mm_s = clampNonNegative(machine_config_.delay.jump_speed_mm_s);
    machine_config_.delay.laser_on_delay_us = clampNonNegative(machine_config_.delay.laser_on_delay_us);
    machine_config_.delay.laser_off_delay_us = clampNonNegative(machine_config_.delay.laser_off_delay_us);
    machine_config_.delay.mark_delay_us = clampNonNegative(machine_config_.delay.mark_delay_us);
    machine_config_.delay.jump_delay_us = clampNonNegative(machine_config_.delay.jump_delay_us);
    machine_config_.delay.polygon_delay_us = clampNonNegative(machine_config_.delay.polygon_delay_us);

    DataGenerator::JUMP_SPEED = machine_config_.delay.jump_speed_mm_s;
    DataGenerator::LASER_ON_DELAY = machine_config_.delay.laser_on_delay_us;
    DataGenerator::LASER_OFF_DELAY = machine_config_.delay.laser_off_delay_us;
    DataGenerator::MARK_DELAY = machine_config_.delay.mark_delay_us;
    DataGenerator::JUMP_DELAY = machine_config_.delay.jump_delay_us;
    DataGenerator::POLYGON_DELAY = machine_config_.delay.polygon_delay_us;

    if (data_buffer_) {
        TcpSocket::Config transport_config;
        transport_config.host = machine_config_.transport.host;
        transport_config.port = machine_config_.transport.port;
        transport_config.connect_retry_ms = machine_config_.transport.connect_retry_ms;
        transport_config.max_connect_attempts = machine_config_.transport.max_connect_attempts;
        transport_config.io_timeout_ms = machine_config_.transport.io_timeout_ms;
        data_buffer_->setTransportConfig(transport_config);
    }
}

bool BoardExecutor::initialize()
{
    try {
        setMachineConfig(machine_config_);
        if (!machine_config_.dry_run_only) {
            data_buffer_->startTcpThread();
        }
        is_running_ = true;
        spdlog::info("BoardExecutor初始化成功: mode={}, laser_output={}, tcp={}:{}, delay(on/off/mark/jump)={}/{}/{}/{}us",
                     machine_config_.dry_run_only ? "dry_run" : "live",
                     machine_config_.mask_laser_output ? "masked" : "enabled",
                     machine_config_.transport.host,
                     machine_config_.transport.port,
                     machine_config_.delay.laser_on_delay_us,
                     machine_config_.delay.laser_off_delay_us,
                     machine_config_.delay.mark_delay_us,
                     machine_config_.delay.jump_delay_us);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("BoardExecutor初始化失败: {}", e.what());
        return false;
    }
}

bool BoardExecutor::execute(const LaserJob& job)
{
    if (!is_running_) {
        spdlog::error("执行器未初始化");
        return false;
    }

    if (!job.isValid()) {
        spdlog::error("任务无效");
        return false;
    }

    std::string validation_error;
    PreflightReport report;
    if (!validateJob(job, &validation_error, &report)) {
        spdlog::error("任务预检失败: {}", validation_error);
        return false;
    }

    try {
        if (machine_config_.dry_run_only) {
            simulateDryRunExecution(job);
            spdlog::info("Dry Run通过: {}", report.summary);
            return true;
        }

        const auto wait_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kConnectionWaitMs);
        while (std::chrono::steady_clock::now() < wait_deadline) {
            if (data_buffer_ && data_buffer_->isTcpConnected()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(kConnectionPollMs));
        }
        if (!data_buffer_ || !data_buffer_->isTcpConnected()) {
            spdlog::error("任务执行失败: TCP未连接到{}:{}",
                          machine_config_.transport.host,
                          machine_config_.transport.port);
            return false;
        }

        convertJobToBuffer(job);
        if (!data_buffer_->waitUntilIdle(kTransportDrainTimeout)) {
            spdlog::error("任务执行失败: TCP缓冲区在超时时间内未排空");
            return false;
        }
        spdlog::info("任务执行完成: {}", report.summary);
        return true;
    } catch (const std::exception& e) {
        if (progress_callback_) {
            progress_callback_(-1);
        }
        spdlog::error("任务执行失败: {}", e.what());
        return false;
    }
}

void BoardExecutor::stop()
{
    is_running_ = false;
    if (data_buffer_) {
        data_buffer_->stopTcpThread();
    }
}

std::string BoardExecutor::getStatus() const
{
    if (!is_running_) {
        return "未初始化";
    }
    if (machine_config_.dry_run_only) {
        return "Dry Run";
    }
    if (isConnected()) {
        return "已连接";
    }
    return "连接中";
}

bool BoardExecutor::isConnected() const
{
    if (machine_config_.dry_run_only) {
        return is_running_.load();
    }
    return data_buffer_ && data_buffer_->isTcpConnected();
}

bool BoardExecutor::validateJob(const LaserJob& job,
                                std::string* out_error,
                                PreflightReport* out_report) const
{
    if (!job.isValid()) {
        if (out_error) {
            *out_error = "LaserJob无效或没有路径点";
        }
        return false;
    }

    if (!machine_config_.dry_run_only) {
        if (machine_config_.transport.host.empty() || machine_config_.transport.port == 0) {
            if (out_error) {
                *out_error = "TCP目标地址未配置";
            }
            return false;
        }
    }

    if (machine_config_.apply_job_transform &&
        !job.coordinate.transform_model_to_machine.empty() &&
        job.coordinate.transform_model_to_machine.size() != 16) {
        if (out_error) {
            *out_error = "任务中的transform_model_to_machine不是16元素4x4矩阵";
        }
        return false;
    }

    PreflightReport report;
    for (size_t segment_index = 0; segment_index < job.segments.size(); ++segment_index) {
        const auto& segment = job.segments[segment_index];
        for (size_t point_index = 0; point_index < segment.points.size(); ++point_index) {
            const auto& point = segment.points[point_index];
            EncodedPoint encoded_point;
            std::string encode_error;
            bool used_job_transform = false;
            if (!encodePointForMachine(job,
                                       point,
                                       segment_index,
                                       point_index,
                                       &encoded_point,
                                       &encode_error,
                                       &used_job_transform)) {
                if (out_error) {
                    *out_error = encode_error;
                }
                return false;
            }
            ++report.total_points;
            report.used_job_transform = report.used_job_transform || used_job_transform;
            const bool is_jump = (segment.type == SegmentType::JUMP) || (point.laser == 0);
            if (is_jump) {
                ++report.jump_points;
            } else {
                ++report.mark_points;
            }
        }
    }

    if (report.mark_points > 0 && !machine_config_.dry_run_only && !machine_config_.live_laser_enabled) {
        if (out_error) {
            *out_error = "当前为真实下发模式，但未允许实时出光/下发";
        }
        return false;
    }

    std::ostringstream summary;
            summary << "points=" << report.total_points
            << ", mark=" << report.mark_points
            << ", jump=" << report.jump_points
            << ", transform=" << (report.used_job_transform ? "on" : "off")
            << ", axis_mode=" << (machine_config_.axis_mode == BoardExecutor::AxisMode::FiveAxis ? "five_axis" : "three_axis")
            << ", mode=" << (machine_config_.dry_run_only ? "dry_run" : "live");
    report.summary = summary.str();

    if (out_report) {
        *out_report = report;
    }
    if (out_error) {
        out_error->clear();
    }
    return true;
}

void BoardExecutor::simulateDryRunExecution(const LaserJob& job) const
{
    size_t simulated_samples = 0;

    for (size_t segment_index = 0; segment_index < job.segments.size(); ++segment_index) {
        const auto& segment = job.segments[segment_index];

        if (progress_callback_) {
            progress_callback_(segment.id);
        }

        for (size_t point_index = 0; point_index < segment.points.size(); ++point_index) {
            const auto& point = segment.points[point_index];
            EncodedPoint current_encoded;
            std::string error;
            if (!encodePointForMachine(job, point, segment_index, point_index, &current_encoded, &error, nullptr)) {
                throw std::runtime_error(error);
            }
            ++simulated_samples;
        }
    }

    if (progress_callback_) {
        progress_callback_(-1);
    }
    spdlog::info("Dry Run模拟遍历完成: segments={}, simulated_samples={}",
                 job.segments.size(),
                 simulated_samples);
}

void BoardExecutor::convertJobToBuffer(const LaserJob& job)
{
    const int base_freq_hz = (std::max)(1, static_cast<int>(std::llround(job.process_defaults.freq_hz)));
    const double base_power_w = std::clamp(job.process_defaults.power_w, 0.0, 100.0);
    int active_freq_hz = base_freq_hz;
    double active_power_w = base_power_w;

    data_buffer_->setFreqData(base_freq_hz);
    data_buffer_->setPowerData(base_power_w);
    data_buffer_->addProcessBegin();

    for (size_t segment_index = 0; segment_index < job.segments.size(); ++segment_index) {
        const auto& segment = job.segments[segment_index];
        const ProcessParams* params = segment.params_override.get();
        if (!params) {
            params = &job.process_defaults;
        }

        if (progress_callback_) {
            progress_callback_(segment.id);
        }

        convertSegmentToBuffer(job,
                               segment,
                               *params,
                               segment_index,
                               &active_freq_hz,
                               &active_power_w);
    }

    data_buffer_->addProcessEnd();
    data_buffer_->forceFill();
    if (progress_callback_) {
        progress_callback_(-1);
    }
}

void BoardExecutor::convertSegmentToBuffer(const LaserJob& job,
                                           const PathSegment& segment,
                                           const ProcessParams& default_params,
                                           size_t segment_index,
                                           int* io_active_freq_hz,
                                           double* io_active_power_w)
{
    if (!io_active_freq_hz || !io_active_power_w || segment.points.empty()) {
        return;
    }

    const auto applyProcessOverrides = [this, io_active_freq_hz, io_active_power_w](const ProcessParams* params) {
        if (!params) {
            return;
        }

        const int target_freq = (std::max)(1, static_cast<int>(std::llround(params->freq_hz)));
        if (std::abs(target_freq - *io_active_freq_hz) >= 1) {
            data_buffer_->setFreqData(target_freq);
            *io_active_freq_hz = target_freq;
        }

        const double target_power = std::clamp(params->power_w, 0.0, 100.0);
        if (std::abs(target_power - *io_active_power_w) > 0.1) {
            data_buffer_->setPowerData(target_power);
            *io_active_power_w = target_power;
        }
    };

    applyProcessOverrides(&default_params);

    for (size_t point_index = 0; point_index < segment.points.size(); ++point_index) {
        const auto& point = segment.points[point_index];
        const ProcessParams* point_params = point.params_override ? point.params_override.get() : &default_params;
        applyProcessOverrides(point_params);

        EncodedPoint current_encoded;
        std::string error;
        if (!encodePointForMachine(job, point, segment_index, point_index, &current_encoded, &error, nullptr)) {
            throw std::runtime_error(error);
        }

        const bool point_is_jump = (segment.type == SegmentType::JUMP) || (point.laser == 0);
        if (point_is_jump || machine_config_.mask_laser_output) {
            data_buffer_->addProcessJumpData(current_encoded.x,
                                             current_encoded.y,
                                             current_encoded.z,
                                             current_encoded.a,
                                             current_encoded.b);
        } else {
            data_buffer_->addProcessData(current_encoded.x,
                                         current_encoded.y,
                                         current_encoded.z,
                                         current_encoded.a,
                                         current_encoded.b);
        }
    }
}

int BoardExecutor::resolveLaserOnDelayUs(const ProcessParams* params, int fallback_us)
{
    if (params && params->laser_on_delay_us > 0) {
        return static_cast<int>((std::min)(params->laser_on_delay_us,
                                           static_cast<int64_t>((std::numeric_limits<int>::max)())));
    }
    return clampNonNegative(fallback_us);
}

int BoardExecutor::resolveLaserOffDelayUs(const ProcessParams* params, int fallback_us)
{
    if (params && params->laser_off_delay_us > 0) {
        return static_cast<int>((std::min)(params->laser_off_delay_us,
                                           static_cast<int64_t>((std::numeric_limits<int>::max)())));
    }
    return clampNonNegative(fallback_us);
}

bool BoardExecutor::encodePointForMachine(const LaserJob& job,
                                          const PathPoint& point,
                                          size_t segment_index,
                                          size_t point_index,
                                          EncodedPoint* out_point,
                                          std::string* out_error,
                                          bool* out_used_job_transform) const
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
        if (out_error) {
            *out_error = "存在非有限坐标点";
        }
        return false;
    }

    double x = point.x;
    double y = point.y;
    double z = point.z;
    bool used_job_transform = false;

    if (machine_config_.apply_job_transform && !job.coordinate.transform_model_to_machine.empty()) {
        if (!transformCoordinates(x, y, z, job.coordinate.transform_model_to_machine)) {
            if (out_error) {
                *out_error = "坐标变换失败，transform_model_to_machine非法";
            }
            return false;
        }
        used_job_transform = true;
    }

    x += machine_config_.x_axis.offset_mm;
    y += machine_config_.y_axis.offset_mm;
    z += machine_config_.z_axis.offset_mm;

    EncodedPoint encoded_point;
    encoded_point.a = 0x8000;
    encoded_point.b = 0x8000;
    std::string axis_error;
    if (!encodeAxis(x, machine_config_.x_axis, "X", &encoded_point.x, &axis_error)) {
        if (out_error) {
            *out_error = buildPointContext(segment_index, point_index, "X") + ": " + axis_error;
        }
        return false;
    }
    if (!encodeAxis(y, machine_config_.y_axis, "Y", &encoded_point.y, &axis_error)) {
        if (out_error) {
            *out_error = buildPointContext(segment_index, point_index, "Y") + ": " + axis_error;
        }
        return false;
    }
    if (!encodeAxis(z, machine_config_.z_axis, "Z", &encoded_point.z, &axis_error)) {
        if (out_error) {
            *out_error = buildPointContext(segment_index, point_index, "Z") + ": " + axis_error;
        }
        return false;
    }
    if (machine_config_.axis_mode == BoardExecutor::AxisMode::FiveAxis) {
        double a = point.a + machine_config_.a_axis.offset_mm;
        double b = point.b + machine_config_.b_axis.offset_mm;
        if (!encodeAxis(a, machine_config_.a_axis, "A", &encoded_point.a, &axis_error)) {
            if (out_error) {
                *out_error = buildPointContext(segment_index, point_index, "A") + ": " + axis_error;
            }
            return false;
        }
        if (!encodeAxis(b, machine_config_.b_axis, "B", &encoded_point.b, &axis_error)) {
            if (out_error) {
                *out_error = buildPointContext(segment_index, point_index, "B") + ": " + axis_error;
            }
            return false;
        }
    }

    if (out_point) {
        *out_point = encoded_point;
    }
    if (out_used_job_transform) {
        *out_used_job_transform = used_job_transform;
    }
    return true;
}

bool BoardExecutor::encodeAxis(double mm,
                               const AxisConfig& axis_config,
                               const char* axis_name,
                               uint16_t* out_value,
                               std::string* out_error)
{
    if (!std::isfinite(mm)) {
        if (out_error) {
            *out_error = std::string(axis_name) + "轴坐标不是有限值";
        }
        return false;
    }
    if (axis_config.limit_min_mm > axis_config.limit_max_mm) {
        if (out_error) {
            *out_error = std::string(axis_name) + "轴软限位设置非法（最小值大于最大值）";
        }
        return false;
    }
    if (mm < axis_config.limit_min_mm || mm > axis_config.limit_max_mm) {
        if (out_error) {
            std::ostringstream oss;
            oss << axis_name << "轴超出软限位: mm=" << mm
                << ", limit=[" << axis_config.limit_min_mm << ", " << axis_config.limit_max_mm << "]";
            *out_error = oss.str();
        }
        return false;
    }

    const double raw = std::llround(mm * kAxisEncodeScale + kAxisEncodeBias);
    if (raw < 0.0 || raw > 65535.0) {
        if (out_error) {
            std::ostringstream oss;
            oss << axis_name << "轴编码溢出: mm=" << mm << ", raw=" << raw;
            *out_error = oss.str();
        }
        return false;
    }

    if (out_value) {
        *out_value = static_cast<uint16_t>(raw);
    }
    return true;
}

bool BoardExecutor::transformCoordinates(double& x, double& y, double& z, const std::vector<double>& transform) const
{
    if (transform.size() != 16) {
        return false;
    }

    const double original_x = x;
    const double original_y = y;
    const double original_z = z;

    const double tx = transform[0] * original_x + transform[1] * original_y + transform[2] * original_z + transform[3];
    const double ty = transform[4] * original_x + transform[5] * original_y + transform[6] * original_z + transform[7];
    const double tz = transform[8] * original_x + transform[9] * original_y + transform[10] * original_z + transform[11];
    const double tw = transform[12] * original_x + transform[13] * original_y + transform[14] * original_z + transform[15];

    if (!std::isfinite(tx) || !std::isfinite(ty) || !std::isfinite(tz) || !std::isfinite(tw)) {
        return false;
    }

    if (std::abs(tw) > 1e-9 && std::abs(tw - 1.0) > 1e-9) {
        x = tx / tw;
        y = ty / tw;
        z = tz / tw;
    } else {
        x = tx;
        y = ty;
        z = tz;
    }

    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

} // namespace nbcam
