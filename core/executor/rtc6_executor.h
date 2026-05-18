#pragma once

#include "executor_interface.h"
#include <atomic>
#include <cstdint>
#include <string>

namespace nbcam {

// RTC-6板卡执行器（Windows + RTC6DLL）
class Rtc6Executor : public IJobExecutor {
public:
    struct AxisConfig {
        double offset_mm = 0.0;
        double limit_min_mm = -120.0;
        double limit_max_mm = 120.0;
    };

    struct DelayConfig {
        int laser_on_delay_us = 200;
        int laser_off_delay_us = 25;
        int mark_delay_us = 0;
        int jump_delay_us = 300;
        int polygon_delay_us = 0;
    };

    struct MachineConfig {
        uint32_t card_no = 1;
        bool dry_run_only = true;
        bool apply_job_transform = true;
        bool live_laser_enabled = false;
        bool mask_laser_output = false;
        double units_per_mm = 1000.0;
        double default_mark_speed_mm_s = 300.0;
        double default_jump_speed_mm_s = 500.0;
        AxisConfig x_axis;
        AxisConfig y_axis;
        AxisConfig z_axis;
        DelayConfig delay;
        std::string program_file;
    };

    struct PreflightReport {
        size_t total_points = 0;
        size_t mark_points = 0;
        size_t jump_points = 0;
        bool used_job_transform = false;
        std::string summary;
    };

    Rtc6Executor();
    ~Rtc6Executor() override;

    bool initialize() override;
    bool execute(const LaserJob& job) override;
    void stop() override;
    std::string getStatus() const override;
    bool isConnected() const override;

    void setMachineConfig(const MachineConfig& config);
    const MachineConfig& getMachineConfig() const { return machine_config_; }
    bool validateJob(const LaserJob& job,
                     std::string* out_error = nullptr,
                     PreflightReport* out_report = nullptr) const;

private:
    struct RtcPoint {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
    };

    MachineConfig machine_config_;
    std::atomic<bool> is_running_{false};
    bool rtc6_initialized_ = false;

    bool encodePointForRtc(const LaserJob& job,
                           const PathPoint& point,
                           size_t segment_index,
                           size_t point_index,
                           RtcPoint* out_point,
                           std::string* out_error,
                           bool* out_used_job_transform = nullptr) const;
    bool transformCoordinates(double& x, double& y, double& z, const std::vector<double>& transform) const;
    static bool checkAxisRange(double mm,
                               const AxisConfig& axis_config,
                               const char* axis_name,
                               std::string* out_error);
};

} // namespace nbcam
