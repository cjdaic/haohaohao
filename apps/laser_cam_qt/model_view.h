#ifndef MODEL_VIEW_H
#define MODEL_VIEW_H

#include <QWidget>
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <QVTKOpenGLNativeWidget.h>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include "../core/param/parameterizer_interface.h"

// VTK前向声明
class vtkRenderer;
class vtkRenderWindow;
class vtkActor;
class vtkPolyDataMapper;
class vtkPolyData;

class vtkRenderer;
class vtkRenderWindow;
class vtkActor;
class vtkPolyDataMapper;
class vtkPolyData;
class vtkPoints;
class vtkCellArray;
class vtkTexture;
class vtkImageData;
class vtkFloatArray;
class vtkUnsignedCharArray;

namespace nbcam {
class TriangleMesh;
class LaserJob;
struct Patch;
struct UVCoord;
class PatchClusterer;
class MeshAccelerator;
}

class ModelView : public QVTKOpenGLNativeWidget
{
    Q_OBJECT

public:
    explicit ModelView(QWidget *parent = nullptr);
    ~ModelView() override;

    // 设置模型
    void setMesh(nbcam::TriangleMesh* mesh);
    
    // 获取网格边界（用于获取边界）
    bool getMeshBounds(double bounds[6]) const;
    
    // 设置路径
    void setJob(nbcam::LaserJob* job);
    void setPathSpacingHint(double spacing_mm);
    void setHighlightedPathSegments(const std::vector<int>& segment_ids);
    void addHighlightedPathSegment(int segment_id);
    void clearHighlightedPathSegments();
    void focusPathSegment(int segment_id);
    
    // 更新视图
    void updateView();
    // 重置相机
    void resetCamera();
    
    // 视图控制
    void setViewXY();
    void setViewXZ();
    void setViewYZ();
    void setViewDefault();
    void setViewAutoFit();
    
    // 线框模式
    void setWireframeMode(bool enabled);
    void setSurfaceColorModeSolid();
    void setSurfaceColorModeNormal();
    void setSurfaceColorModeCurvature();
    
    // 显示/隐藏包围盒
    void setBoundingBoxVisible(bool visible);
    bool isBoundingBoxVisible() const { return bounding_box_visible_; }

    // 显示/隐藏坐标轴
    void setAxesVisible(bool visible);
    bool isAxesVisible() const { return axes_visible_; }
    void setProcessingRegionVisible(bool visible);
    bool isProcessingRegionVisible() const { return processing_region_visible_; }
    
    // 启用/禁用更新（用于避免DirectX错误）
    void setUpdatesEnabled(bool enabled);
    
    // 设置工作平面位姿（仅用于显示，不裁剪）
    void setWorkingPlanePose(double pos_x, double pos_y, double pos_z, 
                            double rot_x, double rot_y, double rot_z);
    
    // 移除工作平面
    void removeWorkingPlane();
    
    // Patch相关功能
    void clusterPatches(double dihedral_threshold = 0.1);
    void highlightPatch(int patch_id);
    void selectPatch(int patch_id);
    void clearPatchSelection();
    int getSelectedPatchId() const { return selected_patch_id_; }
    const std::vector<nbcam::Patch>& getPatches() const;
    
    // 纹理贴图功能
    void applyTextureToPatch(int patch_id, const std::string& svg_filepath, 
                            const std::vector<nbcam::UVCoord>& uv_coords,
                            int texture_width = 1024, int texture_height = 1024,
                            double opacity = 1.0, double ambient = 0.3, 
                            double diffuse = 0.7, double specular = 0.0,
                            double scale_x = 1.0, double scale_y = 1.0,
                            double translate_x = 0.0, double translate_y = 0.0,
                            double rotation_deg = 0.0);
    void removePatchTexture(int patch_id = -1);  // -1表示移除当前显示的纹理
    
    // 清理功能：清除选中的patch、参数化信息、纹理等
    void clearPatchData();
    
    // 高亮单个顶点（用于UV校验）
    void highlightVertex(size_t vertex_index);
    void clearVertexHighlight();
    
    // 根据法向量方向查找patch
    // target_normal: 目标法向量 (nx, ny, nz)，归一化
    // angle_tolerance: 角度容差（弧度），默认0.1弧度（约5.7度）
    // 返回最匹配的patch ID，如果没有找到返回-1
    int findPatchByNormal(double nx, double ny, double nz, double angle_tolerance = 0.1) const;
    
signals:
    void patchSelected(int patch_id);
    
protected:
    // 鼠标事件处理
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    
protected:
    // 键盘事件处理（WASD控制相机）
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    enum class SurfaceColorMode {
        SOLID = 0,
        NORMAL = 1,
        CURVATURE = 2
    };

    void setupRenderer();
    void setupViewControls();
    void createMeshActor(nbcam::TriangleMesh* mesh);
    void createPathActor(nbcam::LaserJob* job);
    void updatePathHighlightColors();
    void updatePathHighlightColorsForSegment(int segment_id, bool highlighted);
    void createPlaneActor(double a, double b, double c, double d);
    void updateAxesSize();  // 根据模型大小更新坐标轴尺寸
    void createBoundingBox();  // 创建包围盒
    void createProcessingRegionBox();
    void applySurfaceColorMode();
    
    vtkRenderer* renderer_;
    vtkRenderWindow* render_window_;
    
    vtkActor* mesh_actor_;
    vtkPolyDataMapper* mesh_mapper_;
    vtkPolyData* mesh_polydata_;
    
    vtkActor* path_actor_;
    vtkPolyDataMapper* path_mapper_;
    vtkPolyData* path_polydata_;
    vtkUnsignedCharArray* path_colors_;
    std::vector<int> path_cell_segment_ids_;
    std::unordered_map<int, std::vector<vtkIdType>> path_segment_cell_indices_;
    std::vector<unsigned char> path_cell_base_colors_;
    std::vector<unsigned char> path_cell_highlight_colors_;
    
    // 箭头actors（用于显示路径方向）
    std::vector<vtkActor*> arrow_actors_;
    
    // 坐标轴
    vtkActor* axis_x_actor_;
    vtkActor* axis_y_actor_;
    vtkActor* axis_z_actor_;
    
    // 工作平面
    vtkActor* plane_actor_;
    vtkPolyDataMapper* plane_mapper_;
    vtkPolyData* plane_polydata_;
    
    // 包围盒
    vtkActor* bounding_box_actor_;
    vtkPolyDataMapper* bounding_box_mapper_;
    vtkPolyData* bounding_box_polydata_;
    vtkActor* processing_region_actor_;
    vtkPolyDataMapper* processing_region_mapper_;
    vtkPolyData* processing_region_polydata_;
    bool bounding_box_visible_;
    bool processing_region_visible_;
    bool axes_visible_;
    
    bool mesh_visible_;
    bool path_visible_;
    SurfaceColorMode surface_color_mode_;
    
    // 视图控制按钮
    QWidget* view_controls_widget_;
    QToolButton* view_xy_btn_;
    QToolButton* view_xz_btn_;
    QToolButton* view_yz_btn_;
    QToolButton* view_default_btn_;
    QToolButton* view_autofit_btn_;
    
    // WASD相机控制
    bool key_w_pressed_;
    bool key_a_pressed_;
    bool key_s_pressed_;
    bool key_d_pressed_;
    bool key_q_pressed_;  // Q键向上
    bool key_e_pressed_;  // E键向下
    int camera_move_timer_id_;
    double camera_move_speed_;  // 相机移动速度（相对于模型大小）
    
    void createAxes();
    void updateCameraPosition();  // 根据按键状态更新相机位置
    void startCameraMoveTimer();  // 启动相机移动定时器
    void stopCameraMoveTimerIfNeeded();  // 停止相机移动定时器（如果没有按键按下）
    
    // Patch相关私有方法
    int pickTriangle(int x, int y);  // 拾取鼠标位置的三角形
    void createPatchHighlightActor(int patch_id);  // 创建patch高亮actor
    void removePatchHighlightActor();  // 移除patch高亮actor
    
    // Patch数据
    std::vector<nbcam::Patch> patches_;
    int selected_patch_id_;
    int highlighted_patch_id_;
    std::unique_ptr<nbcam::PatchClusterer> patch_clusterer_;
    std::unique_ptr<nbcam::MeshAccelerator> mesh_accelerator_;
    nbcam::TriangleMesh* current_mesh_;  // 非拥有指针，指向当前网格
    nbcam::LaserJob* current_job_;       // 非拥有指针，指向当前任务
    double path_spacing_hint_mm_;        // 最近一次规划的间隔参数，用于箭头尺寸缩放
    std::unordered_set<int> highlighted_segment_ids_;
    
    // Patch高亮显示
    vtkActor* patch_highlight_actor_;
    vtkPolyDataMapper* patch_highlight_mapper_;
    vtkPolyData* patch_highlight_polydata_;
    
    // Patch纹理贴图
    vtkActor* patch_texture_actor_;
    vtkPolyDataMapper* patch_texture_mapper_;
    vtkPolyData* patch_texture_polydata_;
    vtkTexture* patch_texture_;
    int textured_patch_id_;  // 当前应用纹理的patch ID
    
    // 顶点高亮（用于UV校验）
    vtkActor* vertex_highlight_actor_;
    vtkPolyDataMapper* vertex_highlight_mapper_;
    vtkPolyData* vertex_highlight_polydata_;
    size_t highlighted_vertex_index_;  // 当前高亮的顶点索引
};

#endif // MODEL_VIEW_H
