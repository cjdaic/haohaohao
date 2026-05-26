#ifndef APPLICATION_CONTROLLER_H
#define APPLICATION_CONTROLLER_H

#include <QObject>
#include <array>
#include <memory>
#include <string>
#include "../core/mesh/mesh_io.h"
#include "../core/param/parameterizer_interface.h"
#include "../core/param/lscm_parameterizer.h"
#include "../core/param/arap_parameterizer.h"
#include "../core/param/authalic_parameterizer.h"
#include "../core/mesh/patch_clusterer.h"
#include "../core/pattern/fill_strategy_interface.h"
#include "../core/mapping/uv_mapper.h"
#include "../core/process/process_model_interface.h"
#include "../core/planner/path_planner.h"
#include "../core/job/laser_job.h"
#include "../core/job/json_serializer.h"
#include "../core/executor/executor_interface.h"

namespace nbcam {
class TriangleMesh;
class LaserJob;
struct Patch;
}

class ApplicationController : public QObject
{
    Q_OBJECT

public:
    struct ModelTransform {
        double uniform_scale = 1.0;
        double offset_x_mm = 0.0;
        double offset_y_mm = 0.0;
        double offset_z_mm = 0.0;
    };

    struct PatternPlanOptions {
        enum class ContourProcessMode {
            NONE,          // 不加工轮廓
            CONTOUR_ONLY,  // 只加工轮廓
            ALL            // 轮廓+内部填充
        };

        std::string strategy = "line_hatch";
        double spacing = 0.1;             // mm（在UV域中按当前实现直接使用）
        double angle_deg = 0.0;
        std::string direction_mode = "angle";  // angle/u_axis/v_axis
        ContourProcessMode contour_mode = ContourProcessMode::ALL;
        bool z_layer_enabled = false;
        double layer_height = 0.1;        // mm
        std::string svg_filepath;         // 可选：用于提取外轮廓
        double tex_scale_x = 1.0;
        double tex_scale_y = 1.0;
        double tex_translate_x = 0.0;
        double tex_translate_y = 0.0;
        double tex_rotation_deg = 0.0;
        bool enable_inverse_stretch_prewarp = true;
        double inverse_stretch_prewarp_strength = 1.2; // [0,2]，>1 为增强补偿
        // 圆弧往返填充参数（当 strategy=arc_hatch 时生效）
        double arc_center_x = 20.0;
        double arc_center_z = -15.0;
        double arc_radius = 20.0;
        // 扫描速度（mm/s，>0 时用于圆弧重采样步进）
        double scan_speed_mm_s = -1.0;
    };

    explicit ApplicationController(QObject *parent = nullptr);
    ~ApplicationController();

    // 模型操作
    bool loadModel(const std::string& filepath);
    void clearModel();
    bool adaptiveSubdivideCurrentMesh(size_t target_triangles = 5000,
                                      size_t max_triangles = 80000,
                                      int max_iterations = 6,
                                      int* out_applied_iterations = nullptr);
    bool saveCurrentMesh(const std::string& filepath);
    nbcam::TriangleMesh* getCurrentMesh() const { return current_mesh_.get(); }
    ModelTransform getModelTransform() const { return model_transform_; }
    bool setModelTransform(const ModelTransform& transform);
    bool autoCenterModelToBounds(double x_min, double x_max,
                                 double y_min, double y_max,
                                 double z_min, double z_max);
    bool getCurrentMeshBounds(std::array<double, 6>& bounds) const;
    bool currentMeshFitsBounds(double x_min, double x_max,
                               double y_min, double y_max,
                               double z_min, double z_max,
                               std::string* out_message = nullptr) const;
    

    // 参数化操作
    bool parameterizeMesh(const std::string& algorithm = "LSCM");
    bool hasParameterization() const { return !uv_coords_.empty(); }

    // 图案生成
    bool generatePattern(const std::string& strategy, double spacing, double angle = 0.0);
    bool hasPattern() const { return !uv_path_.empty(); }
    int getParameterizedPatchId() const { return parameterized_patch_id_; }
    int getPatternPatchId() const { return pattern_patch_id_; }
    
    // SVG导入
    bool importSVG(const std::string& filepath, double center_u = 0.5, double center_v = 0.5, double scale = 0.9);
    
    // 导入SVG图案并平铺到UV空间
    // tile_u, tile_v: 每个平铺单元的UV尺寸
    // uv_bounds: UV边界（u_min, v_min, u_max, v_max），如果为空则使用整个UV空间
    bool importSVGWithTiling(const std::string& filepath, 
                            double tile_u = 0.1, 
                            double tile_v = 0.1,
                            const std::vector<double>& uv_bounds = {});
    
    // Patch相关功能
    // 对选中的patch进行参数化
    bool parameterizePatch(int patch_id, const std::vector<nbcam::Patch>& patches, const std::string& algorithm = "LSCM");
    
    // 在模型上的图案区域（patch）内按选定策略生成填充路径
    // strategy/spacing/angle 来自右侧参数面板，路径落在 patch 表面
    bool generatePatternInPatch(int patch_id, const std::vector<nbcam::Patch>& patches,
                               const std::string& strategy, double spacing, double angle);
    bool generatePatternInPatch(int patch_id, const std::vector<nbcam::Patch>& patches,
                               const PatternPlanOptions& options);
    void clearPatternForPatch(int patch_id);

    // 映射和参数分配
    bool mapToXYZ(const std::vector<size_t>* patch_triangle_indices = nullptr);
    bool assignProcessParams(const std::string& model_type = "curvature");

    // 路径规划：将xyz_path_转换为LaserJob分段并应用后处理，结果用于模型视图绘制
    bool planPath();

    // 任务操作
    bool saveJob(const std::string& filepath);
    bool loadJob(const std::string& filepath);
    nbcam::LaserJob* getCurrentJob() const { return current_job_.get(); }

    // 执行
    bool executeJob(nbcam::IJobExecutor* executor);

    // 获取数据
    const std::vector<nbcam::UVCoord>& getUVCoords() const { return uv_coords_; }
    const std::vector<nbcam::UVPathPoint>& getUVPath() const { return uv_path_; }
    const std::vector<nbcam::PathPoint>& getXYZPath() const { return xyz_path_; }
    double getPlannedSpacingHint() const { return planned_spacing_hint_mm_; }
    
    // 清理参数化数据（保留模型）
    void clearParameterization();
    // 清理路径/参数化/SVG相关状态（保留当前模型）
    void clearProcessingStateKeepMesh();
    
    // 路径断开标记（用于SVG导入）
    std::vector<bool> path_break_flags_;  // 对应uv_path_，true表示路径断开

signals:
    void modelLoaded(bool success);
    void parameterizationCompleted(bool success);
    void patternGenerated(bool success);
    void pathMapped(bool success);
    void jobReady(bool ready);
    void executionStatusChanged(const QString& status);

private:
    void resetProcessingStateKeepMeshInternal(bool emit_job_ready);
    bool rebuildCurrentMeshFromSource(bool emit_model_loaded);

    // 数据
    std::unique_ptr<nbcam::TriangleMesh> source_mesh_;
    std::unique_ptr<nbcam::TriangleMesh> current_mesh_;
    ModelTransform model_transform_;
    std::vector<nbcam::UVCoord> uv_coords_;
    std::vector<nbcam::UVPathPoint> uv_path_;
    std::vector<nbcam::PathPoint> xyz_path_;
    std::vector<nbcam::UVCoord> svg_boundary_;  // SVG形状的UV边界，用于填充时作为填充区域
    std::unique_ptr<nbcam::LaserJob> current_job_;
    double planned_spacing_hint_mm_ = -1.0;
    
    // Patch相关数据缓存
    int parameterized_patch_id_;  // 当前已参数化的patch ID，-1表示没有
    int pattern_patch_id_;        // 当前UV路径所属patch ID，-1表示非patch上下文或无路径

    // 工具类
    nbcam::MeshIO mesh_io_;
    std::unique_ptr<nbcam::IParameterizer> parameterizer_;
    std::unique_ptr<nbcam::IFillStrategy> fill_strategy_;
    nbcam::FillStrategy current_fill_strategy_ = nbcam::FillStrategy::HATCH;
    nbcam::UVMapper uv_mapper_;
    std::unique_ptr<nbcam::IProcessModel> process_model_;
    nbcam::PathPlanner path_planner_;
    nbcam::JsonSerializer serializer_;
    std::string current_strategy_name_ = "line_hatch";
    double arc_center_x_ = 20.0;
    double arc_center_z_ = -15.0;
    double arc_radius_ = 20.0;
    double arc_scan_speed_mm_s_ = -1.0;
    bool arc_path_prebaked_ = false;
    bool current_path_uses_grayscale_ = false;
};

#endif // APPLICATION_CONTROLLER_H
