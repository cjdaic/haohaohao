#pragma once

#include "executor_interface.h"
#include "data_buffer.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <atomic>
#include <string>
#include <utility>

namespace nbcam {

// 自研板卡执行器
class BoardExecutor : public IJobExecutor {
public:
    enum class AxisMode {
        ThreeAxis,
        FiveAxis,
    };

    struct TransportConfig {
        std::string host = "192.168.1.10";
        uint16_t port = 7;
        int connect_retry_ms = 100;
        int max_connect_attempts = 5;
        int io_timeout_ms = 200;
    };

    struct AxisConfig {
        double offset_mm = 0.0;
        double limit_min_mm = -32.768;
        double limit_max_mm = 32.767;
    };

    struct DelayConfig {
        int jump_speed_mm_s = 500;
        int laser_on_delay_us = 200;
        int laser_off_delay_us = 25;
        int mark_delay_us = 0;
        int jump_delay_us = 300;
        int polygon_delay_us = 0;
    };

    struct MachineConfig {
        TransportConfig transport;
        AxisConfig x_axis;
        AxisConfig y_axis;
        AxisConfig z_axis;
        AxisConfig a_axis;
        AxisConfig b_axis;
        DelayConfig delay;
        AxisMode axis_mode = AxisMode::ThreeAxis;
        bool apply_job_transform = true;
        bool dry_run_only = true;
        bool live_laser_enabled = false;
        bool mask_laser_output = false;
    };

    struct PreflightReport {
        size_t total_points = 0;
        size_t mark_points = 0;
        size_t jump_points = 0;
        bool used_job_transform = false;
        std::string summary;
    };

    BoardExecutor();
    ~BoardExecutor() override;

    using ProgressCallback = std::function<void(int segment_id)>;
    
    bool initialize() override;
    bool execute(const LaserJob& job) override;
    void stop() override;
    std::string getStatus() const override;
    bool isConnected() const override;
    void setMachineConfig(const MachineConfig& config);
    void setProgressCallback(ProgressCallback callback) { progress_callback_ = std::move(callback); }
    const MachineConfig& getMachineConfig() const { return machine_config_; }
    bool validateJob(const LaserJob& job,
                     std::string* out_error = nullptr,
                     PreflightReport* out_report = nullptr) const;

private:
    struct EncodedPoint {
        uint16_t x = 0;
        uint16_t y = 0;
        uint16_t z = 0;
        uint16_t a = 0x8000;
        uint16_t b = 0x8000;
    };

    std::unique_ptr<DataBuffer> data_buffer_;
    std::atomic<bool> is_running_{false};
    MachineConfig machine_config_;
    ProgressCallback progress_callback_;
    
    // 将LaserJob转换为DataBuffer数据
    void convertJobToBuffer(const LaserJob& job);
    void convertSegmentToBuffer(const LaserJob& job,
                               const PathSegment& segment,
                               const ProcessParams& default_params,
                               size_t segment_index,
                               int* io_active_freq_hz,
                               double* io_active_power_w);
    void appendInterpolatedLine(const EncodedPoint& start,
                                const EncodedPoint& end,
                                double speed_mm_s,
                                bool mark_segment,
                                int laser_on_delay_us,
                                int* mark_elapsed_us);
    void appendDelayAtPoint(const EncodedPoint& point, int delay_us, int delay_on_us);
    static int resolveLaserOnDelayUs(const ProcessParams* params, int fallback_us);
    static int resolveLaserOffDelayUs(const ProcessParams* params, int fallback_us);
    
    bool encodePointForMachine(const LaserJob& job,
                               const PathPoint& point,
                               size_t segment_index,
                               size_t point_index,
                               EncodedPoint* out_point,
                               std::string* out_error,
                               bool* out_used_job_transform = nullptr) const;
    void simulateDryRunExecution(const LaserJob& job) const;
    static bool encodeAxis(double mm,
                           const AxisConfig& axis_config,
                           const char* axis_name,
                           uint16_t* out_value,
                           std::string* out_error);
    bool transformCoordinates(double& x, double& y, double& z, const std::vector<double>& transform) const;
};

} // namespace nbcam
