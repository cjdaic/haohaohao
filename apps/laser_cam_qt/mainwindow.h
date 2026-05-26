#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QSplitter>
#include <QShowEvent>
#include <QSet>
#include <QStringList>
#include <map>
#include <memory>
#include <vector>
#include <spdlog/common.h>

QT_BEGIN_NAMESPACE
class QAction;
class QActionGroup;
class QAbstractButton;
class QMenu;
class QDialog;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QToolButton;
class QTranslator;
class QTimer;
class QWidget;
QT_END_NAMESPACE

class ModelView;
class UVView;
class PathPreview;
class ParameterPanel;
class ApplicationController;
class GrpcClient;
class PlanePoseWidget;
class TextureEditorDialog;
class TextureListWidget;
class TextureDetailsDialog;

namespace nbcam {
struct UVCoord;
struct UVPathPoint;
class LaserJob;
class DataBuffer;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openModel();
    void exportModel();
    void executeJob();
    void parameterize();
    void generatePattern();
    void about();
    void toggleWireframe3D(bool enabled);
    void toggleWireframeUV(bool enabled);
    void toggleBoundingBox(bool enabled);
    void toggleAxes(bool enabled);
    void toggleUVView(bool enabled);  // 显示/隐藏UV视图
    void toggleUVValidation(bool enabled);  // 开启/关闭UV校验模式
    void importSVG();
    void clusterPatches();
    void onPatchSelected(int patch_id);
    void parameterizeSelectedPatch();
    // void importSVGToPatch();  // SVG路径规划功能已注释
    void applyTextureToPatch();  // 应用SVG纹理到patch
    void adaptiveSubdivideAndExportStl();  // 自适应均匀细分
    void removeTextureFromPatch(int patch_id);  // 移除patch的纹理
    void removeSavedPathFromPatch(int patch_id);  // 仅移除已保存路径
    void editTextureForPatch(int patch_id);  // 编辑patch的纹理
    void showTextureDetails(int patch_id);  // 显示纹理详细信息
    void openTextureEditor();  // 打开纹理编辑器
    void clearPatchData();  // 清理面片数据
    void selectTopSurfacePatch();  // 自动选择上表面patch
    void onPlanPath();  // 路径规划（运行当前配置的算法并在模型上绘制）
    void measureArea();
    void measureSurfacePointCoordinate();
    void measureLineDistance();
    void measureGeodesicDistance();
    void testGrpcCommunication();
    void testTcpCommunication();
    void testSerialCommunication();
    void connectCommunication();
    void openManualOperationPanel();
    void openCurrentLog();
    void clearCurrentLog();
    void clearAllLogs();
    void openCommandLineMode();
    void openModelOffsetDialog();
    void onLanguageSelected(QAction* action);
    void onStartMachining();
    void onPauseMachining();
    void onEmergencyStop();
    void onRefreshMachining();
    void onSimulationToggled(bool enabled);
    void processMachiningStep();
    void openCalibrationDialog();
    
    // 控制器信号响应
    void onModelLoaded(bool success);
    void onParameterizationCompleted(bool success);
    void onPatternGenerated(bool success);
    void onPathMapped(bool success);
    void onJobReady(bool ready);
    void onExecutionStatusChanged(const QString& status);

private:
    struct ValidationExampleOptions {
        QString svg_relative_path = "docs/nano.svg";
        QString strategy_override;
        size_t adaptive_target_triangles = 0;
        bool run_comm_probe_for_log = false;
        bool force_laser_output = false;
        bool force_swap_yz_axes = false;
        double arc_center_x = 20.0;
        double arc_center_z = -15.0;
        double arc_radius = 20.0;
    };

    struct UVDisplayRange {
        double u_min = 0.0;
        double u_max = 1.0;
        double v_min = 0.0;
        double v_max = 1.0;
    };

    struct SavedUvPoint {
        double u = 0.0;
        double v = 0.0;
        double grayscale = 0.0;
        bool jump_before = false;
    };

    void createMenus();
    void createStatusBar();
    void setupUI();
    void connectSignals();
    void updateUI();
    void updateUIWithoutMesh();  // 更新UI但不更新mesh（避免清除patch选中状态）
    void openPathPreviewDialog();
    void saveCurrentPathFromPreview();
    void runValidationExample(double spacing_mm,
                              const ValidationExampleOptions& options = ValidationExampleOptions{});  // 一键验证示例流程
    QString resolveExampleFile(const QString& relative_path) const;
    QString resolveProjectLogRoot() const;
    QString sanitizeLogNamePart(const QString& raw, const QString& fallback) const;
    QString buildUnifiedLogFileName(const QString& prefix, const QString& model_path, const QString& svg_path) const;
    bool captureGeneratedLog(const std::string& generated_log_path,
                             const QString& model_path = QString(),
                             const QString& svg_path = QString(),
                             const QString& prefix = QString("frame"));
    bool computePatchUVBounds(int patch_id, double& u_min, double& u_max, double& v_min, double& v_max) const;
    void registerPatchInUVLayout(int patch_id, const UVDisplayRange& raw_range);
    UVDisplayRange resolveDisplayRangeForPatch(int patch_id, const UVDisplayRange& raw_fallback) const;
    std::vector<nbcam::UVCoord> remapUVCoordsToDisplayRange(const std::vector<nbcam::UVCoord>& uv_coords,
                                                            const UVDisplayRange& raw_range,
                                                            const UVDisplayRange& display_range) const;
    std::vector<nbcam::UVPathPoint> remapUVPathToDisplayRange(const std::vector<nbcam::UVPathPoint>& uv_path,
                                                              const UVDisplayRange& raw_range,
                                                              const UVDisplayRange& display_range) const;
    void rebuildUVLayoutDisplayRanges();
    void refreshUVTexturesForLayout();
    void resetUVLayoutState();
    void refreshExecuteActionState();
    bool hasSavedPathForPatch(int patch_id) const;
    nbcam::LaserJob* buildDisplayJobForView();
    void updateTextureContourInfoFromJob(int patch_id, const nbcam::LaserJob* job);
    bool queryVertexIndex(const QString& title, size_t& out_index) const;
    bool queryTwoVertexIndices(const QString& title, size_t& index_a, size_t& index_b) const;
    double computeMeshTotalArea() const;
    double computeGeodesicDistance(size_t start_idx, size_t end_idx) const;
    bool configureCommunicationSettings();
    bool configureDeviceBoardSettings();
    bool configureDeviceSettings();
    bool configureBoardSettings();
    bool configureLaserSettings();
    bool testGrpcCommunicationImpl(QString* out_feedback = nullptr);
    bool testTcpCommunicationImpl(QString* out_feedback = nullptr);
    bool testSerialCommunicationImpl(QString* out_feedback = nullptr);
    void ensureManualOperationDialog();
    void updateManualOperationStatus();
    void handleManualJog(double delta_x_mm, double delta_y_mm, double delta_z_mm, const QString& action_name);
    void resetManualJogToCenter();
    bool sendManualJogTo(double x_mm, double y_mm, double z_mm, const QString& action_name);
    bool sendCalibrationSquareGrid(double square_size_mm,
                                   double spacing_mm,
                                   int grid_count,
                                   bool laser_output_enabled,
                                   QString* out_error);
    void createLanguageMenu();
    void createMachiningControlWidget();
    QStringList discoverQtTranslationLocales() const;
    bool applyQtTranslation(const QString& locale_name);
    QString formatLanguageLabel(const QString& locale_name) const;
    void applyHighlightedSegmentIds(const QSet<int>& segment_ids);
    void applyMachiningProgressHighlight(int current_segment_id);
    void resetMachiningProgress();
    void stopMachiningSession(bool reset_to_start, bool disconnect_socket, bool keep_simulate_toggle = false);
    bool sendMachiningResetToStartFrame(QString* out_error);
    bool prepareMachiningPlan(QString* out_error);
    bool validateActiveJobForMachining(QString* out_error) const;
    bool currentModelFitsProcessingRegion(QString* out_error = nullptr) const;
    void refreshModelProcessingRegionStatus();
    bool validateTcpTargetForMachining(QString* out_error) const;
    bool ensureMachiningTransportConnected(QString* out_error);
    bool sendCurrentMachiningFrame(QString* out_error);
    bool flushMachiningTransportTail(QString* out_error);
    void updateMachiningUiState();

    // UI组件
    QSplitter* main_splitter_;
    QSplitter* left_splitter_;  // 用于控制UV视图的显示/隐藏
    TextureListWidget* texture_list_widget_;  // 纹理列表
    ModelView* model_view_;
    UVView* uv_view_;
    PathPreview* path_preview_;
    QPushButton* path_preview_button_;
    QDialog* path_preview_dialog_;
    ParameterPanel* parameter_panel_;
    PlanePoseWidget* plane_pose_widget_;
    // 控制器
    ApplicationController* controller_;
    
    // 菜单
    QMenu* file_menu_;
    QMenu* view_menu_;
    QMenu* tools_menu_;
    QMenu* connection_menu_;
    QMenu* device_board_menu_;
    QMenu* manual_menu_;
    QMenu* calibration_menu_;
    QMenu* measure_menu_;
    QMenu* log_menu_;
    QMenu* language_menu_;
    QMenu* help_menu_;
    QDoubleSpinBox* manual_jog_step_spin_ = nullptr;
    QLabel* manual_jog_position_label_ = nullptr;
    double manual_jog_x_mm_ = 0.0;
    double manual_jog_y_mm_ = 0.0;
    double manual_jog_z_mm_ = 0.0;
    bool manual_jog_position_initialized_ = false;
    
    // 纹理编辑器对话框
    TextureEditorDialog* texture_editor_dialog_;
    TextureDetailsDialog* texture_details_dialog_;
    std::map<int, std::unique_ptr<nbcam::LaserJob>> saved_patch_jobs_;
    std::map<int, std::vector<SavedUvPoint>> saved_patch_uv_paths_;
    std::unique_ptr<nbcam::LaserJob> display_job_cache_;
    std::map<int, UVDisplayRange> patch_raw_uv_ranges_;
    std::map<int, UVDisplayRange> patch_display_ranges_;
    std::vector<int> uv_layout_order_;
    
    // 动作
    QAction* open_model_action_;
    QAction* execute_job_action_;
    QAction* parameterize_action_;
    QAction* generate_pattern_action_;
    QAction* import_svg_action_;
    QAction* exit_action_;
    QAction* about_action_;
    QAction* plan_path_action_;
    
    // 参数化算法动作组
    QActionGroup* parameterization_group_;
    QAction* param_lscm_action_;
    QAction* param_arap_action_;
    
    // 视图选项
    QAction* wireframe_3d_action_;
    QAction* wireframe_uv_action_;
    QAction* bounding_box_action_;
    QAction* axes_action_;
    QDialog* manual_operation_dialog_;
    QActionGroup* language_group_;
    QTranslator* qt_translator_;
    QWidget* machining_control_widget_;
    QToolButton* machining_start_button_;
    QToolButton* machining_pause_button_;
    QToolButton* machining_stop_button_;
    QToolButton* machining_refresh_button_;
    QToolButton* machining_simulate_button_;
    QTimer* machining_timer_;
    std::unique_ptr<nbcam::DataBuffer> machining_data_buffer_;

    enum class ExecutorBackend {
        DataGenerator,
        RTC6,
    };

    enum class AxisMode {
        ThreeAxis,
        FiveAxis,
    };

    struct MachiningBatch {
        QByteArray payload;
        int segment_id = -1;
        int duration_us = 0;
        bool mark_output = false;
        int frame_count = 0;
    };

    struct MachiningPlan {
        QVector<MachiningBatch> batches;
        QSet<int> all_segment_ids;
    };

    enum class MachiningRunState {
        Idle,
        Running,
        Paused,
    };

    struct CommunicationConfig {
        ExecutorBackend executor_backend = ExecutorBackend::DataGenerator;
        QString grpc_endpoint = "127.0.0.1:50051";
        bool enable_grpc_test = false;
        QString tcp_host = "192.168.1.10";
        int tcp_port = 7;
        int tcp_io_timeout_ms = 200;
        int tcp_connect_retry_ms = 100;
        int tcp_max_connect_attempts = 5;
        QString serial_port = "COM1";
        int serial_baud = 115200;
        bool enable_serial_test = false;
        bool apply_job_transform = true;
        bool swap_yz_axes = false;
        bool dry_run_only = false;
        bool require_preexecute_comm_check = true;
        AxisMode axis_mode = AxisMode::ThreeAxis;
        double x_offset_mm = 0.0;
        double y_offset_mm = 0.0;
        double z_offset_mm = 0.0;
        double a_offset_mm = 0.0;
        double b_offset_mm = 0.0;
        double x_min_mm = -32.768;
        double x_max_mm = 32.767;
        double y_min_mm = -32.768;
        double y_max_mm = 32.767;
        double z_min_mm = -32.768;
        double z_max_mm = 32.767;
        double a_min_mm = -32.768;
        double a_max_mm = 32.767;
        double b_min_mm = -32.768;
        double b_max_mm = 32.767;
        int jump_speed_mm_s = 500;
        int laser_on_delay_us = 100;
        int laser_off_delay_us = 580;
        int mark_delay_us = 600;
        int jump_delay_us = 150;
        int polygon_delay_us = 450;
        int rtc6_card_no = 1;
        double rtc6_units_per_mm = 1000.0;
        double rtc6_mark_speed_mm_s = 300.0;
        double rtc6_jump_speed_mm_s = 500.0;
        QString rtc6_program_file = "RTC6/RTC6OUT.out";
        QString laser_model = "M8 100W";
        QString laser_params_source = "reference/M8 100w 产品说明书(1).pdf";
        double laser_rated_power_w = 100.0;
        double laser_min_power_w = 0.0;
        double laser_max_power_w = 100.0;
        int laser_min_freq_hz = 20000;
        int laser_max_freq_hz = 60000;
        QString laser_control_mode = "串口+TTL";
        QString laser_notes = "默认参数来自 reference/M8 100w 产品说明书(1).pdf，可在“激光器设置”中导入覆盖。";
    } comm_config_;
    QString current_model_path_;
    QString current_svg_path_;
    QString current_log_path_;
    MachiningPlan machining_plan_;
    MachiningRunState machining_run_state_ = MachiningRunState::Idle;
    int machining_batch_index_ = 0;
    size_t machining_transport_pending_bytes_ = 0;
    QSet<int> machining_completed_segment_ids_;
    bool emergency_stop_requested_ = false;
    bool machining_refresh_available_ = false;
    bool is_simulation_mode_ = false;
    bool ignore_simulation_toggle_signal_ = false;
    bool machining_laser_output_enabled_ = false;
    bool machining_segment_feedback_enabled_ = false;
    spdlog::level::level_enum machining_previous_log_level_ = spdlog::level::info;
    bool machining_log_level_overridden_ = false;

protected:
    void showEvent(QShowEvent *event) override;
};

#endif // MAINWINDOW_H
