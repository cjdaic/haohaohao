#include "rtc6_executor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>
#include <sstream>
#include <thread>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

extern "C" {
unsigned long __stdcall init_rtc6_dll();
void __stdcall free_rtc6_dll();
void __stdcall set_rtc6_mode();
unsigned long __stdcall acquire_rtc(unsigned long card_no);
unsigned long __stdcall release_rtc(unsigned long card_no);
unsigned long __stdcall n_load_program_file(unsigned long card_no, char* path);
void __stdcall n_config_list(unsigned long card_no, unsigned long mem1, unsigned long mem2);
void __stdcall n_set_start_list_1(unsigned long card_no);
void __stdcall n_set_end_of_list(unsigned long card_no);
void __stdcall n_execute_list_1(unsigned long card_no);
void __stdcall n_stop_execution(unsigned long card_no);
void __stdcall n_get_status(unsigned long card_no, unsigned long* status, unsigned long* pos);
unsigned long __stdcall n_get_last_error(unsigned long card_no);
void __stdcall n_set_jump_speed_ctrl(unsigned long card_no, double speed);
void __stdcall n_set_mark_speed_ctrl(unsigned long card_no, double speed);
void __stdcall n_set_laser_delays(unsigned long card_no, long laser_on_delay, unsigned long laser_off_delay);
void __stdcall n_set_scanner_delays(unsigned long card_no, unsigned long jump, unsigned long mark, unsigned long polygon);
void __stdcall n_jump_abs_3d(unsigned long card_no, long x, long y, long z);
void __stdcall n_mark_abs_3d(unsigned long card_no, long x, long y, long z);
}
#endif

namespace nbcam {

namespace {

std::string buildPointContext(size_t segment_index, size_t point_index, const char* axis_name)
{
    std::ostringstream oss;
    oss << "segment=" << segment_index << ", point=" << point_index << ", axis=" << axis_name;
    return oss.str();
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

int clampNonNegative(int value)
{
    return (std::max)(0, value);
}

int resolveLaserDelayUs(const ProcessParams* params, bool laser_on, int fallback_us)
{
    if (params) {
        const int64_t override_us = laser_on ? params->laser_on_delay_us : params->laser_off_delay_us;
        if (override_us > 0) {
            return static_cast<int>((std::min)(override_us,
                                               static_cast<int64_t>((std::numeric_limits<int>::max)())));
        }
    }
    return clampNonNegative(fallback_us);
}

}  // namespace

Rtc6Executor::Rtc6Executor() = default;

Rtc6Executor::~Rtc6Executor()
{
    stop();
}

void Rtc6Executor::setMachineConfig(const MachineConfig& config)
{
    machine_config_ = config;
    machine_config_.delay.laser_on_delay_us = clampNonNegative(machine_config_.delay.laser_on_delay_us);
    machine_config_.delay.laser_off_delay_us = clampNonNegative(machine_config_.delay.laser_off_delay_us);
    machine_config_.delay.mark_delay_us = clampNonNegative(machine_config_.delay.mark_delay_us);
    machine_config_.delay.jump_delay_us = clampNonNegative(machine_config_.delay.jump_delay_us);
    machine_config_.delay.polygon_delay_us = clampNonNegative(machine_config_.delay.polygon_delay_us);
    if (!(std::isfinite(machine_config_.default_mark_speed_mm_s) && machine_config_.default_mark_speed_mm_s > 1e-6)) {
        machine_config_.default_mark_speed_mm_s = 300.0;
    }
    if (!(std::isfinite(machine_config_.default_jump_speed_mm_s) && machine_config_.default_jump_speed_mm_s > 1e-6)) {
        machine_config_.default_jump_speed_mm_s = 500.0;
    }
    if (!(std::isfinite(machine_config_.units_per_mm) && machine_config_.units_per_mm > 1e-6)) {
        machine_config_.units_per_mm = 1000.0;
    }
}

bool Rtc6Executor::initialize()
{
    try {
        if (machine_config_.dry_run_only) {
            rtc6_initialized_ = true;
            is_running_ = true;
            spdlog::info("Rtc6Executor初始化成功: mode=dry_run, card={}", machine_config_.card_no);
            return true;
        }

#ifdef _WIN32
        const unsigned long init_ret = init_rtc6_dll();
        if (init_ret != 0) {
            spdlog::error("Rtc6Executor初始化失败: init_rtc6_dll ret={}", init_ret);
            rtc6_initialized_ = false;
            return false;
        }

        set_rtc6_mode();
        const unsigned long acq_ret = acquire_rtc(machine_config_.card_no);
        if (acq_ret != 0) {
            spdlog::error("Rtc6Executor初始化失败: acquire_rtc(card={}) ret={}", machine_config_.card_no, acq_ret);
            free_rtc6_dll();
            rtc6_initialized_ = false;
            return false;
        }

        std::string program_file = machine_config_.program_file;
        if (program_file.empty()) {
            const std::filesystem::path default_program = std::filesystem::path("RTC6") / "RTC6OUT.out";
            if (std::filesystem::exists(default_program)) {
                program_file = default_program.string();
            }
        }
        if (!program_file.empty() && std::filesystem::exists(program_file)) {
            std::vector<char> mutable_path(program_file.begin(), program_file.end());
            mutable_path.push_back('\0');
            const unsigned long load_ret = n_load_program_file(machine_config_.card_no, mutable_path.data());
            if (load_ret != 0) {
                spdlog::warn("Rtc6Executor: n_load_program_file('{}') ret={}", program_file, load_ret);
            } else {
                spdlog::info("Rtc6Executor: 程序文件已加载 {}", program_file);
            }
        } else {
            spdlog::warn("Rtc6Executor: 未配置有效程序文件，跳过n_load_program_file");
        }

        n_config_list(machine_config_.card_no, 400000, 400000);
        n_set_jump_speed_ctrl(machine_config_.card_no, machine_config_.default_jump_speed_mm_s);
        n_set_mark_speed_ctrl(machine_config_.card_no, machine_config_.default_mark_speed_mm_s);
        n_set_laser_delays(machine_config_.card_no,
                           machine_config_.delay.laser_on_delay_us,
                           static_cast<unsigned long>(machine_config_.delay.laser_off_delay_us));
        n_set_scanner_delays(machine_config_.card_no,
                             static_cast<unsigned long>(machine_config_.delay.jump_delay_us),
                             static_cast<unsigned long>(machine_config_.delay.mark_delay_us),
                             static_cast<unsigned long>(machine_config_.delay.polygon_delay_us));

        rtc6_initialized_ = true;
        is_running_ = true;
        spdlog::info("Rtc6Executor初始化成功: mode=live, laser_output={}, card={}, units_per_mm={}, delays(on/off/mark/jump/polygon)={}/{}/{}/{}/{}us",
                     machine_config_.mask_laser_output ? "masked" : "enabled",
                     machine_config_.card_no,
                     machine_config_.units_per_mm,
                     machine_config_.delay.laser_on_delay_us,
                     machine_config_.delay.laser_off_delay_us,
                     machine_config_.delay.mark_delay_us,
                     machine_config_.delay.jump_delay_us,
                     machine_config_.delay.polygon_delay_us);
        return true;
#else
        spdlog::error("Rtc6Executor初始化失败: 当前平台不支持RTC6");
        return false;
#endif
    } catch (const std::exception& e) {
        spdlog::error("Rtc6Executor初始化失败: {}", e.what());
        rtc6_initialized_ = false;
        is_running_ = false;
        return false;
    }
}

bool Rtc6Executor::execute(const LaserJob& job)
{
    if (!is_running_) {
        spdlog::error("RTC6执行器未初始化");
        return false;
    }
    if (!job.isValid()) {
        spdlog::error("RTC6执行失败: 任务无效");
        return false;
    }

    std::string validation_error;
    PreflightReport report;
    if (!validateJob(job, &validation_error, &report)) {
        spdlog::error("RTC6执行前预检失败: {}", validation_error);
        return false;
    }

    if (machine_config_.dry_run_only) {
        spdlog::info("RTC6 Dry Run通过: {}", report.summary);
        return true;
    }

    if (!rtc6_initialized_) {
        spdlog::error("RTC6执行失败: 设备未初始化");
        return false;
    }

#ifdef _WIN32
    try {
        n_set_jump_speed_ctrl(machine_config_.card_no, machine_config_.default_jump_speed_mm_s);
        n_set_mark_speed_ctrl(machine_config_.card_no, machine_config_.default_mark_speed_mm_s);
        n_set_laser_delays(machine_config_.card_no,
                           machine_config_.delay.laser_on_delay_us,
                           static_cast<unsigned long>(machine_config_.delay.laser_off_delay_us));
        n_set_scanner_delays(machine_config_.card_no,
                             static_cast<unsigned long>(machine_config_.delay.jump_delay_us),
                             static_cast<unsigned long>(machine_config_.delay.mark_delay_us),
                             static_cast<unsigned long>(machine_config_.delay.polygon_delay_us));

        n_set_start_list_1(machine_config_.card_no);

        double active_mark_speed = machine_config_.default_mark_speed_mm_s;
        double active_jump_speed = machine_config_.default_jump_speed_mm_s;
        int active_laser_on_delay = machine_config_.delay.laser_on_delay_us;
        int active_laser_off_delay = machine_config_.delay.laser_off_delay_us;

        const auto applyProcessState = [&](const ProcessParams* params, bool for_jump) {
            const int target_laser_on_delay =
                resolveLaserDelayUs(params, true, machine_config_.delay.laser_on_delay_us);
            const int target_laser_off_delay =
                resolveLaserDelayUs(params, false, machine_config_.delay.laser_off_delay_us);
            if (target_laser_on_delay != active_laser_on_delay || target_laser_off_delay != active_laser_off_delay) {
                n_set_laser_delays(machine_config_.card_no,
                                   target_laser_on_delay,
                                   static_cast<unsigned long>(target_laser_off_delay));
                active_laser_on_delay = target_laser_on_delay;
                active_laser_off_delay = target_laser_off_delay;
            }

            if (for_jump) {
                const double jump_speed = resolveSpeedMmS(params, machine_config_.default_jump_speed_mm_s);
                if (std::abs(jump_speed - active_jump_speed) > 1e-6) {
                    n_set_jump_speed_ctrl(machine_config_.card_no, jump_speed);
                    active_jump_speed = jump_speed;
                }
                return;
            }

            const double mark_speed = resolveSpeedMmS(params, machine_config_.default_mark_speed_mm_s);
            if (std::abs(mark_speed - active_mark_speed) > 1e-6) {
                n_set_mark_speed_ctrl(machine_config_.card_no, mark_speed);
                active_mark_speed = mark_speed;
            }
        };

        for (size_t segment_index = 0; segment_index < job.segments.size(); ++segment_index) {
            const auto& segment = job.segments[segment_index];
            if (segment.points.empty()) {
                continue;
            }

            const ProcessParams* segment_params = segment.params_override ? segment.params_override.get()
                                                                          : &job.process_defaults;

            // 每个段首点先Jump，避免从上一段终点到本段首点发生误打标。
            {
                const auto& first_point = segment.points.front();
                const ProcessParams* first_point_params =
                    first_point.params_override ? first_point.params_override.get() : segment_params;
                RtcPoint rtc_point{};
                std::string encode_error;
                if (!encodePointForRtc(job, first_point, segment_index, 0, &rtc_point, &encode_error, nullptr)) {
                    throw std::runtime_error(encode_error);
                }
                applyProcessState(first_point_params, true);
                n_jump_abs_3d(machine_config_.card_no, rtc_point.x, rtc_point.y, rtc_point.z);
            }

            for (size_t point_index = 1; point_index < segment.points.size(); ++point_index) {
                const auto& point = segment.points[point_index];
                const ProcessParams* point_params = point.params_override ? point.params_override.get() : segment_params;

                RtcPoint rtc_point{};
                std::string encode_error;
                if (!encodePointForRtc(job, point, segment_index, point_index, &rtc_point, &encode_error, nullptr)) {
                    throw std::runtime_error(encode_error);
                }

                const bool is_jump = (segment.type == SegmentType::JUMP) || (point.laser == 0);
                if (is_jump) {
                    applyProcessState(point_params, true);
                    n_jump_abs_3d(machine_config_.card_no, rtc_point.x, rtc_point.y, rtc_point.z);
                } else {
                    applyProcessState(point_params, false);
                    if (machine_config_.mask_laser_output) {
                        n_jump_abs_3d(machine_config_.card_no, rtc_point.x, rtc_point.y, rtc_point.z);
                    } else {
                        n_mark_abs_3d(machine_config_.card_no, rtc_point.x, rtc_point.y, rtc_point.z);
                    }
                }
            }
        }

        n_set_end_of_list(machine_config_.card_no);
        n_execute_list_1(machine_config_.card_no);

        unsigned long status = 0;
        unsigned long pos = 0;
        n_get_status(machine_config_.card_no, &status, &pos);
        const unsigned long last_error = n_get_last_error(machine_config_.card_no);
        if (last_error != 0) {
            spdlog::error("RTC6执行失败: last_error={}, status={}, pos={}", last_error, status, pos);
            return false;
        }

        spdlog::info("RTC6任务下发完成: {}, status={}, pos={}", report.summary, status, pos);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("RTC6执行异常: {}", e.what());
        return false;
    }
#else
    spdlog::error("RTC6执行失败: 当前平台不支持");
    return false;
#endif
}

void Rtc6Executor::stop()
{
#ifdef _WIN32
    if (rtc6_initialized_) {
        n_stop_execution(machine_config_.card_no);
        release_rtc(machine_config_.card_no);
        free_rtc6_dll();
        rtc6_initialized_ = false;
    }
#endif
    is_running_ = false;
}

std::string Rtc6Executor::getStatus() const
{
    if (!is_running_) {
        return "未初始化";
    }
    if (machine_config_.dry_run_only) {
        return "Dry Run";
    }
    return rtc6_initialized_ ? "已连接" : "初始化失败";
}

bool Rtc6Executor::isConnected() const
{
    if (machine_config_.dry_run_only) {
        return is_running_.load();
    }
    return rtc6_initialized_;
}

bool Rtc6Executor::validateJob(const LaserJob& job,
                               std::string* out_error,
                               PreflightReport* out_report) const
{
    if (!job.isValid()) {
        if (out_error) {
            *out_error = "LaserJob无效或没有路径点";
        }
        return false;
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
            RtcPoint rtc_point{};
            std::string encode_error;
            bool used_job_transform = false;
            if (!encodePointForRtc(job,
                                   point,
                                   segment_index,
                                   point_index,
                                   &rtc_point,
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
            << ", mode=" << (machine_config_.dry_run_only ? "dry_run" : "live")
            << ", card=" << machine_config_.card_no;
    report.summary = summary.str();

    if (out_report) {
        *out_report = report;
    }
    if (out_error) {
        out_error->clear();
    }
    return true;
}

void Rtc6Executor::applyAxisSwapIfNeeded(double& y, double& z) const
{
    if (machine_config_.swap_yz_axes) {
        std::swap(y, z);
    }
}

bool Rtc6Executor::encodePointForRtc(const LaserJob& job,
                                     const PathPoint& point,
                                     size_t segment_index,
                                     size_t point_index,
                                     RtcPoint* out_point,
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

    applyAxisSwapIfNeeded(y, z);
    x += machine_config_.x_axis.offset_mm;
    y += machine_config_.y_axis.offset_mm;
    z += machine_config_.z_axis.offset_mm;

    std::string axis_error;
    const char* logical_y_name = machine_config_.swap_yz_axes ? "Z" : "Y";
    const char* logical_z_name = machine_config_.swap_yz_axes ? "Y" : "Z";
    if (!checkAxisRange(x, machine_config_.x_axis, "X", &axis_error)) {
        if (out_error) {
            *out_error = buildPointContext(segment_index, point_index, "X") + ": " + axis_error;
        }
        return false;
    }
    if (!checkAxisRange(y, machine_config_.y_axis, logical_y_name, &axis_error)) {
        if (out_error) {
            *out_error = buildPointContext(segment_index, point_index, logical_y_name) + ": " + axis_error;
        }
        return false;
    }
    if (!checkAxisRange(z, machine_config_.z_axis, logical_z_name, &axis_error)) {
        if (out_error) {
            *out_error = buildPointContext(segment_index, point_index, logical_z_name) + ": " + axis_error;
        }
        return false;
    }

    const double scale = machine_config_.units_per_mm;
    const double x_raw = std::llround(x * scale);
    const double y_raw = std::llround(y * scale);
    const double z_raw = std::llround(z * scale);

    if (x_raw < static_cast<double>((std::numeric_limits<int32_t>::min)()) ||
        x_raw > static_cast<double>((std::numeric_limits<int32_t>::max)()) ||
        y_raw < static_cast<double>((std::numeric_limits<int32_t>::min)()) ||
        y_raw > static_cast<double>((std::numeric_limits<int32_t>::max)()) ||
        z_raw < static_cast<double>((std::numeric_limits<int32_t>::min)()) ||
        z_raw > static_cast<double>((std::numeric_limits<int32_t>::max)())) {
        if (out_error) {
            *out_error = "RTC6坐标编码溢出";
        }
        return false;
    }

    if (out_point) {
        out_point->x = static_cast<int32_t>(x_raw);
        out_point->y = static_cast<int32_t>(y_raw);
        out_point->z = static_cast<int32_t>(z_raw);
    }
    if (out_used_job_transform) {
        *out_used_job_transform = used_job_transform;
    }
    return true;
}

bool Rtc6Executor::transformCoordinates(double& x, double& y, double& z, const std::vector<double>& transform) const
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

bool Rtc6Executor::checkAxisRange(double mm,
                                  const AxisConfig& axis_config,
                                  const char* axis_name,
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
    return true;
}

} // namespace nbcam
