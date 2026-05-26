#include "model_view.h"
#include "../core/mesh/mesh_io.h"
#include "../core/mesh/patch_clusterer.h"
#include "../core/mesh/mesh_accelerator.h"
#include "../core/job/laser_job.h"
#include "svg_raster_cache.h"
#include <QVTKOpenGLNativeWidget.h>
#include <QHBoxLayout>
#include <QToolButton>
#include <QIcon>
#include <QKeyEvent>
#include <QTimerEvent>
#include <QMouseEvent>
#include <QImage>
#include <vtkCellPicker.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkTriangle.h>
#include <vtkLine.h>
#include <vtkProperty.h>
#include <vtkCamera.h>
#include <vtkCamera.h>
#include <vtkPlane.h>
#include <vtkClipPolyData.h>
#include <vtkPlaneSource.h>
#include <vtkTexture.h>
#include <vtkImageData.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include "../core/pattern/nanosvg_wrapper.h"
#include "../core/param/parameterizer_interface.h"
#include <vtkUnsignedCharArray.h>
#include <vtkCellData.h>
#include <vtkPointData.h>
#include <vtkLineSource.h>
#include <vtkSphereSource.h>
#include <cmath>
#include <algorithm>
#include <limits>
#include <utility>
#include <spdlog/spdlog.h>
#include <vector>
#include <unordered_map>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

struct UvAspectFitFrame {
    double u_min = 0.0;
    double u_max = 1.0;
    double v_min = 0.0;
    double v_max = 1.0;
    double u_range = 1.0;
    double v_range = 1.0;
};

bool computeUvAspectFitFrame(double patch_u_min,
                             double patch_u_max,
                             double patch_v_min,
                             double patch_v_max,
                             double content_aspect_wh,
                             UvAspectFitFrame& out)
{
    const double patch_u_range = patch_u_max - patch_u_min;
    const double patch_v_range = patch_v_max - patch_v_min;
    if (!(patch_u_range > 1e-9) || !(patch_v_range > 1e-9) ||
        !(content_aspect_wh > 1e-9) || !std::isfinite(content_aspect_wh)) {
        return false;
    }

    const double patch_center_u = (patch_u_min + patch_u_max) * 0.5;
    const double patch_center_v = (patch_v_min + patch_v_max) * 0.5;
    const double patch_aspect = patch_u_range / patch_v_range;

    double fit_u = patch_u_range;
    double fit_v = patch_v_range;
    if (patch_aspect > content_aspect_wh) {
        fit_v = patch_v_range;
        fit_u = fit_v * content_aspect_wh;
    } else {
        fit_u = patch_u_range;
        fit_v = fit_u / content_aspect_wh;
    }

    out.u_range = fit_u;
    out.v_range = fit_v;
    out.u_min = patch_center_u - fit_u * 0.5;
    out.u_max = patch_center_u + fit_u * 0.5;
    out.v_min = patch_center_v - fit_v * 0.5;
    out.v_max = patch_center_v + fit_v * 0.5;
    return out.u_range > 1e-9 && out.v_range > 1e-9;
}

std::pair<int, int> computeAspectFittedRasterSize(int requested_w,
                                                   int requested_h,
                                                   double content_aspect_wh)
{
    int out_w = std::max(1, requested_w);
    int out_h = std::max(1, requested_h);
    if (!(content_aspect_wh > 1e-9) || !std::isfinite(content_aspect_wh)) {
        return {out_w, out_h};
    }

    const double box_aspect = static_cast<double>(out_w) / static_cast<double>(out_h);
    if (box_aspect > content_aspect_wh) {
        out_h = std::max(1, requested_h);
        out_w = std::max(1, static_cast<int>(std::lround(static_cast<double>(out_h) * content_aspect_wh)));
    } else {
        out_w = std::max(1, requested_w);
        out_h = std::max(1, static_cast<int>(std::lround(static_cast<double>(out_w) / content_aspect_wh)));
    }

    out_w = std::clamp(out_w, 1, std::max(1, requested_w));
    out_h = std::clamp(out_h, 1, std::max(1, requested_h));
    return {out_w, out_h};
}

bool hasFiniteNormal(const nbcam::Vertex& v)
{
    return std::isfinite(v.nx) && std::isfinite(v.ny) && std::isfinite(v.nz) &&
           (v.nx * v.nx + v.ny * v.ny + v.nz * v.nz) > 1e-12;
}

std::vector<std::array<double, 3>> computeVertexNormalsOrFallback(const nbcam::TriangleMesh& mesh)
{
    std::vector<std::array<double, 3>> normals(mesh.vertices.size(), {0.0, 0.0, 1.0});
    bool all_have_normals = true;
    for (const auto& v : mesh.vertices) {
        if (!hasFiniteNormal(v)) {
            all_have_normals = false;
            break;
        }
    }

    if (all_have_normals) {
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const auto& v = mesh.vertices[i];
            const double len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
            if (len > 1e-12) {
                normals[i] = {v.nx / len, v.ny / len, v.nz / len};
            }
        }
        return normals;
    }

    std::vector<std::array<double, 3>> acc(mesh.vertices.size(), {0.0, 0.0, 0.0});
    for (const auto& tri : mesh.triangles) {
        if (tri.v0 >= mesh.vertices.size() || tri.v1 >= mesh.vertices.size() || tri.v2 >= mesh.vertices.size()) {
            continue;
        }
        const auto& a = mesh.vertices[tri.v0];
        const auto& b = mesh.vertices[tri.v1];
        const auto& c = mesh.vertices[tri.v2];
        const double abx = b.x - a.x;
        const double aby = b.y - a.y;
        const double abz = b.z - a.z;
        const double acx = c.x - a.x;
        const double acy = c.y - a.y;
        const double acz = c.z - a.z;
        const double nx = aby * acz - abz * acy;
        const double ny = abz * acx - abx * acz;
        const double nz = abx * acy - aby * acx;
        acc[tri.v0][0] += nx; acc[tri.v0][1] += ny; acc[tri.v0][2] += nz;
        acc[tri.v1][0] += nx; acc[tri.v1][1] += ny; acc[tri.v1][2] += nz;
        acc[tri.v2][0] += nx; acc[tri.v2][1] += ny; acc[tri.v2][2] += nz;
    }

    for (size_t i = 0; i < acc.size(); ++i) {
        const double len = std::sqrt(acc[i][0] * acc[i][0] + acc[i][1] * acc[i][1] + acc[i][2] * acc[i][2]);
        if (len > 1e-12) {
            normals[i] = {acc[i][0] / len, acc[i][1] / len, acc[i][2] / len};
        }
    }
    return normals;
}

std::array<unsigned char, 3> heatColorFrom01(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    const double x = t * 3.0;
    if (x < 1.0) {
        return {static_cast<unsigned char>(0),
                static_cast<unsigned char>(x * 255.0),
                static_cast<unsigned char>(255)};
    }
    if (x < 2.0) {
        const double y = x - 1.0;
        return {static_cast<unsigned char>(y * 255.0),
                static_cast<unsigned char>(255),
                static_cast<unsigned char>((1.0 - y) * 255.0)};
    }
    const double y = x - 2.0;
    return {static_cast<unsigned char>(255),
            static_cast<unsigned char>((1.0 - y) * 255.0),
            static_cast<unsigned char>(0)};
}

std::vector<double> estimateCurvatureProxy(const nbcam::TriangleMesh& mesh,
                                           const std::vector<std::array<double, 3>>& normals)
{
    std::vector<std::vector<size_t>> adjacency(mesh.vertices.size());
    for (const auto& tri : mesh.triangles) {
        if (tri.v0 >= mesh.vertices.size() || tri.v1 >= mesh.vertices.size() || tri.v2 >= mesh.vertices.size()) {
            continue;
        }
        adjacency[tri.v0].push_back(tri.v1); adjacency[tri.v0].push_back(tri.v2);
        adjacency[tri.v1].push_back(tri.v0); adjacency[tri.v1].push_back(tri.v2);
        adjacency[tri.v2].push_back(tri.v0); adjacency[tri.v2].push_back(tri.v1);
    }

    std::vector<double> curvature(mesh.vertices.size(), 0.0);
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& vi = mesh.vertices[i];
        double sum = 0.0;
        int cnt = 0;
        for (size_t j : adjacency[i]) {
            if (j >= mesh.vertices.size()) {
                continue;
            }
            const auto& vj = mesh.vertices[j];
            const double dx = vj.x - vi.x;
            const double dy = vj.y - vi.y;
            const double dz = vj.z - vi.z;
            const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist <= 1e-9) {
                continue;
            }
            const double dnx = normals[i][0] - normals[j][0];
            const double dny = normals[i][1] - normals[j][1];
            const double dnz = normals[i][2] - normals[j][2];
            sum += std::sqrt(dnx * dnx + dny * dny + dnz * dnz) / dist;
            ++cnt;
        }
        if (cnt > 0) {
            curvature[i] = sum / static_cast<double>(cnt);
        }
    }
    return curvature;
}

}  // namespace

ModelView::ModelView(QWidget *parent)
    : QVTKOpenGLNativeWidget(parent)
    , renderer_(nullptr)
    , render_window_(nullptr)
    , mesh_actor_(nullptr)
    , mesh_mapper_(nullptr)
    , mesh_polydata_(nullptr)
    , path_actor_(nullptr)
    , path_mapper_(nullptr)
    , path_polydata_(nullptr)
    , path_colors_(nullptr)
    , mesh_visible_(true)
    , path_visible_(true)
    , surface_color_mode_(SurfaceColorMode::SOLID)
    , view_controls_widget_(nullptr)
    , view_xy_btn_(nullptr)
    , view_xz_btn_(nullptr)
    , view_yz_btn_(nullptr)
    , view_default_btn_(nullptr)
    , view_autofit_btn_(nullptr)
    , axis_x_actor_(nullptr)
    , axis_y_actor_(nullptr)
    , axis_z_actor_(nullptr)
    , plane_actor_(nullptr)
    , plane_mapper_(nullptr)
    , plane_polydata_(nullptr)
    , bounding_box_actor_(nullptr)
    , bounding_box_mapper_(nullptr)
    , bounding_box_polydata_(nullptr)
    , processing_region_actor_(nullptr)
    , processing_region_mapper_(nullptr)
    , processing_region_polydata_(nullptr)
    , bounding_box_visible_(false)
    , processing_region_visible_(false)
    , axes_visible_(true)
    , key_w_pressed_(false)
    , key_a_pressed_(false)
    , key_s_pressed_(false)
    , key_d_pressed_(false)
    , key_q_pressed_(false)
    , key_e_pressed_(false)
    , camera_move_timer_id_(-1)
    , camera_move_speed_(0.01)  // 默认移动速度为模型大小的1%
    , selected_patch_id_(-1)
    , highlighted_patch_id_(-1)
    , patch_clusterer_(std::make_unique<nbcam::PatchClusterer>())
    , mesh_accelerator_(std::make_unique<nbcam::MeshAccelerator>())
    , current_mesh_(nullptr)
    , current_job_(nullptr)
    , path_spacing_hint_mm_(-1.0)
    , patch_highlight_actor_(nullptr)
    , patch_highlight_mapper_(nullptr)
    , patch_highlight_polydata_(nullptr)
    , patch_texture_actor_(nullptr)
    , patch_texture_mapper_(nullptr)
    , patch_texture_polydata_(nullptr)
    , patch_texture_(nullptr)
    , textured_patch_id_(-1)
    , vertex_highlight_actor_(nullptr)
    , vertex_highlight_mapper_(nullptr)
    , vertex_highlight_polydata_(nullptr)
    , highlighted_vertex_index_(static_cast<size_t>(-1))
{
    spdlog::info("ModelView ctor: begin");
    setupRenderer();
    setupViewControls();
    setFocusPolicy(Qt::StrongFocus);  // 允许接收键盘事件
    setMouseTracking(true);  // 启用鼠标跟踪以支持悬停高亮
    spdlog::info("ModelView ctor: end");
}

ModelView::~ModelView()
{
    // 清理纹理相关资源
    removePatchTexture(-1);  // 移除所有纹理
    for (vtkActor* actor : arrow_actors_) {
        if (renderer_) {
            renderer_->RemoveActor(actor);
        }
        actor->Delete();
    }
    arrow_actors_.clear();
    
    // VTK对象析构
    if (renderer_) {
        renderer_->Delete();
    }
}

void ModelView::setupRenderer()
{
    if (renderer_ && render_window_) {
        return;
    }

    render_window_ = renderWindow();
    if (!render_window_) {
        spdlog::critical("ModelView::setupRenderer failed: renderWindow() is null");
        return;
    }

    renderer_ = vtkRenderer::New();
    renderer_->SetBackground(0.2, 0.2, 0.2);
    render_window_->AddRenderer(renderer_);
    
    // 设置交互器
    // 不需要手动设置，QVTKOpenGLNativeWidget已经处理了
    
    // 创建坐标轴
    createAxes();
    createProcessingRegionBox();
    
    updateView();
}

void ModelView::setMesh(nbcam::TriangleMesh* mesh)
{
    if (!mesh) {
        if (mesh_actor_) {
            renderer_->RemoveActor(mesh_actor_);
            mesh_actor_ = nullptr;
        }
        current_mesh_ = nullptr;
        mesh_accelerator_->clear();
        patches_.clear();
        selected_patch_id_ = -1;
        highlighted_patch_id_ = -1;
        removePatchHighlightActor();
        updateView();
        return;
    }
    
    // 指针相同时也可能发生了原地顶点坐标更新（例如模型偏置/缩放）。
    // 若拓扑未变则只刷新显示，保留patch和选中状态；若拓扑变了再清空patch数据。
    if (current_mesh_ == mesh) {
        bool topology_changed = true;
        if (mesh_polydata_) {
            const vtkIdType point_count = mesh_polydata_->GetNumberOfPoints();
            const vtkIdType poly_count = mesh_polydata_->GetNumberOfPolys();
            topology_changed =
                point_count != static_cast<vtkIdType>(mesh->vertices.size()) ||
                poly_count != static_cast<vtkIdType>(mesh->triangles.size());
        }

        current_mesh_ = mesh;
        createMeshActor(mesh);
        mesh_accelerator_->build(*mesh);

        if (!topology_changed) {
            if (selected_patch_id_ >= 0 && selected_patch_id_ < static_cast<int>(patches_.size())) {
                highlighted_patch_id_ = -1;
                removePatchHighlightActor();
                createPatchHighlightActor(selected_patch_id_);
                highlighted_patch_id_ = selected_patch_id_;
            }
            updateView();
            return;
        }

        patches_.clear();
        selected_patch_id_ = -1;
        highlighted_patch_id_ = -1;
        removePatchHighlightActor();
        removePatchTexture();
        updateView();
        return;
    }

    current_mesh_ = mesh;
    createMeshActor(mesh);
    
    // 构建AABB树用于快速拾取
    mesh_accelerator_->build(*mesh);
    
    // 清除之前的patch数据（因为mesh改变了）
    patches_.clear();
    selected_patch_id_ = -1;
    highlighted_patch_id_ = -1;
    removePatchHighlightActor();
    removePatchTexture();  // 也清除纹理
    
    updateView();
}

void ModelView::setJob(nbcam::LaserJob* job)
{
    if (current_job_ != job) {
        highlighted_segment_ids_.clear();
    }
    current_job_ = job;

    if (!job) {
        if (path_actor_) {
            renderer_->RemoveActor(path_actor_);
            path_actor_ = nullptr;
        }
        updateView();
        return;
    }
    
    createPathActor(job);
    updateView();
}

void ModelView::setPathSpacingHint(double spacing_mm)
{
    const bool old_valid = std::isfinite(path_spacing_hint_mm_) && path_spacing_hint_mm_ > 0.0;
    const bool new_valid = std::isfinite(spacing_mm) && spacing_mm > 0.0;
    if (old_valid && new_valid && std::abs(path_spacing_hint_mm_ - spacing_mm) < 1e-9) {
        return;
    }
    if (!old_valid && !new_valid) {
        return;
    }

    path_spacing_hint_mm_ = new_valid ? spacing_mm : -1.0;
    if (current_job_) {
        createPathActor(current_job_);
        updateView();
    }
}

void ModelView::setHighlightedPathSegments(const std::vector<int>& segment_ids)
{
    std::unordered_set<int> next_highlight_ids(segment_ids.begin(), segment_ids.end());
    if (next_highlight_ids == highlighted_segment_ids_) {
        return;
    }

    const std::unordered_set<int> previous_highlight_ids = highlighted_segment_ids_;
    highlighted_segment_ids_ = std::move(next_highlight_ids);
    if (current_job_) {
        if (!path_colors_ || !path_polydata_) {
            updatePathHighlightColors();
        } else {
            for (int id : previous_highlight_ids) {
                if (highlighted_segment_ids_.find(id) == highlighted_segment_ids_.end()) {
                    updatePathHighlightColorsForSegment(id, false);
                }
            }
            for (int id : highlighted_segment_ids_) {
                if (previous_highlight_ids.find(id) == previous_highlight_ids.end()) {
                    updatePathHighlightColorsForSegment(id, true);
                }
            }
            path_colors_->Modified();
            path_polydata_->Modified();
        }
        updateView();
    }
}

void ModelView::addHighlightedPathSegment(int segment_id)
{
    if (segment_id < 0 || highlighted_segment_ids_.find(segment_id) != highlighted_segment_ids_.end()) {
        return;
    }

    highlighted_segment_ids_.insert(segment_id);
    if (current_job_) {
        if (!path_colors_ || !path_polydata_) {
            updatePathHighlightColors();
        } else {
            updatePathHighlightColorsForSegment(segment_id, true);
            path_colors_->Modified();
            path_polydata_->Modified();
        }
        updateView();
    }
}

void ModelView::clearHighlightedPathSegments()
{
    if (highlighted_segment_ids_.empty()) {
        return;
    }

    highlighted_segment_ids_.clear();
    if (current_job_) {
        updatePathHighlightColors();
        updateView();
    }
}

void ModelView::focusPathSegment(int segment_id)
{
    if (!renderer_ || !renderer_->GetActiveCamera() || !current_job_) {
        return;
    }

    const nbcam::PathSegment* target_segment = nullptr;
    for (const auto& segment : current_job_->segments) {
        if (segment.id == segment_id && segment.points.size() >= 2) {
            target_segment = &segment;
            break;
        }
    }
    if (!target_segment) {
        return;
    }

    const auto& points = target_segment->points;
    double min_x = points.front().x;
    double max_x = points.front().x;
    double min_y = points.front().y;
    double max_y = points.front().y;
    double min_z = points.front().z;
    double max_z = points.front().z;
    for (const auto& p : points) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
            continue;
        }
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
        min_z = std::min(min_z, p.z);
        max_z = std::max(max_z, p.z);
    }

    const double center_x = (min_x + max_x) * 0.5;
    const double center_y = (min_y + max_y) * 0.5;
    const double center_z = (min_z + max_z) * 0.5;
    double dir_x = points.back().x - points.front().x;
    double dir_y = points.back().y - points.front().y;
    double dir_z = points.back().z - points.front().z;
    double dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
    if (dir_len <= 1e-8) {
        for (size_t i = 1; i < points.size() && dir_len <= 1e-8; ++i) {
            dir_x = points[i].x - points[i - 1].x;
            dir_y = points[i].y - points[i - 1].y;
            dir_z = points[i].z - points[i - 1].z;
            dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
        }
    }
    if (dir_len <= 1e-8) {
        dir_x = 1.0;
        dir_y = 0.0;
        dir_z = 0.0;
        dir_len = 1.0;
    }
    dir_x /= dir_len;
    dir_y /= dir_len;
    dir_z /= dir_len;

    double bbox_len = std::sqrt((max_x - min_x) * (max_x - min_x) +
                                (max_y - min_y) * (max_y - min_y) +
                                (max_z - min_z) * (max_z - min_z));
    if (bbox_len <= 1e-6 && mesh_polydata_) {
        double bounds[6];
        mesh_polydata_->GetBounds(bounds);
        bbox_len = std::max({bounds[1] - bounds[0], bounds[3] - bounds[2], bounds[5] - bounds[4]}) * 0.08;
    }
    bbox_len = std::max(bbox_len, 3.0);

    vtkCamera* camera = renderer_->GetActiveCamera();
    double up_x = 0.0, up_y = 1.0, up_z = 0.0;
    if (std::abs(dir_y) > 0.9) {
        up_x = 0.0;
        up_y = 0.0;
        up_z = 1.0;
    }

    const double pos_x = center_x - dir_x * bbox_len * 1.8 + up_x * bbox_len * 0.35;
    const double pos_y = center_y - dir_y * bbox_len * 1.8 + up_y * bbox_len * 0.35;
    const double pos_z = center_z - dir_z * bbox_len * 1.8 + up_z * bbox_len * 0.35;

    camera->SetFocalPoint(center_x, center_y, center_z);
    camera->SetPosition(pos_x, pos_y, pos_z);
    camera->SetViewUp(up_x, up_y, up_z);
    renderer_->ResetCameraClippingRange();
    updateView();
}

void ModelView::createMeshActor(nbcam::TriangleMesh* mesh)
{
    if (!mesh || mesh->triangles.empty()) {
        return;
    }
    
    // 创建点
    vtkPoints* points = vtkPoints::New();
    for (const auto& vertex : mesh->vertices) {
        points->InsertNextPoint(vertex.x, vertex.y, vertex.z);
    }
    
    // 创建三角面
    vtkCellArray* triangles = vtkCellArray::New();
    for (const auto& tri : mesh->triangles) {
        vtkTriangle* triangle = vtkTriangle::New();
        triangle->GetPointIds()->SetId(0, static_cast<vtkIdType>(tri.v0));
        triangle->GetPointIds()->SetId(1, static_cast<vtkIdType>(tri.v1));
        triangle->GetPointIds()->SetId(2, static_cast<vtkIdType>(tri.v2));
        triangles->InsertNextCell(triangle);
        triangle->Delete();
    }
    
    // 创建PolyData
    if (mesh_polydata_) {
        mesh_polydata_->Delete();
    }
    mesh_polydata_ = vtkPolyData::New();
    mesh_polydata_->SetPoints(points);
    mesh_polydata_->SetPolys(triangles);
    
    points->Delete();
    triangles->Delete();
    
    // 创建Mapper和Actor
    if (mesh_mapper_) {
        mesh_mapper_->Delete();
    }
    mesh_mapper_ = vtkPolyDataMapper::New();
    mesh_mapper_->SetInputData(mesh_polydata_);
    
    if (mesh_actor_) {
        renderer_->RemoveActor(mesh_actor_);
        mesh_actor_->Delete();
    }
    mesh_actor_ = vtkActor::New();
    mesh_actor_->SetMapper(mesh_mapper_);
    mesh_actor_->GetProperty()->SetOpacity(0.8);
    applySurfaceColorMode();
    
    renderer_->AddActor(mesh_actor_);
    
    // 更新坐标轴大小以适应模型
    updateAxesSize();
    
    // 创建包围盒
    createBoundingBox();
    createProcessingRegionBox();
    
    // 重置相机以适应新模型
    resetCamera();
}

void ModelView::setSurfaceColorModeSolid()
{
    if (surface_color_mode_ == SurfaceColorMode::SOLID) {
        return;
    }
    surface_color_mode_ = SurfaceColorMode::SOLID;
    applySurfaceColorMode();
    updateView();
}

void ModelView::setSurfaceColorModeNormal()
{
    if (surface_color_mode_ == SurfaceColorMode::NORMAL) {
        return;
    }
    surface_color_mode_ = SurfaceColorMode::NORMAL;
    applySurfaceColorMode();
    updateView();
}

void ModelView::setSurfaceColorModeCurvature()
{
    if (surface_color_mode_ == SurfaceColorMode::CURVATURE) {
        return;
    }
    surface_color_mode_ = SurfaceColorMode::CURVATURE;
    applySurfaceColorMode();
    updateView();
}

void ModelView::applySurfaceColorMode()
{
    if (!mesh_mapper_ || !mesh_actor_ || !mesh_polydata_ || !current_mesh_) {
        return;
    }

    if (surface_color_mode_ == SurfaceColorMode::SOLID) {
        mesh_mapper_->ScalarVisibilityOff();
        mesh_actor_->GetProperty()->SetColor(0.8, 0.8, 0.9);
        return;
    }

    const auto normals = computeVertexNormalsOrFallback(*current_mesh_);

    vtkUnsignedCharArray* colors = vtkUnsignedCharArray::New();
    colors->SetNumberOfComponents(3);
    colors->SetName("MeshPointColors");
    colors->SetNumberOfTuples(static_cast<vtkIdType>(current_mesh_->vertices.size()));

    if (surface_color_mode_ == SurfaceColorMode::NORMAL) {
        for (size_t i = 0; i < normals.size(); ++i) {
            const unsigned char r = static_cast<unsigned char>(std::clamp((normals[i][0] * 0.5 + 0.5) * 255.0, 0.0, 255.0));
            const unsigned char g = static_cast<unsigned char>(std::clamp((normals[i][1] * 0.5 + 0.5) * 255.0, 0.0, 255.0));
            const unsigned char b = static_cast<unsigned char>(std::clamp((normals[i][2] * 0.5 + 0.5) * 255.0, 0.0, 255.0));
            colors->SetTypedTuple(static_cast<vtkIdType>(i), std::array<unsigned char, 3>{r, g, b}.data());
        }
    } else {
        const auto curvature = estimateCurvatureProxy(*current_mesh_, normals);
        std::vector<double> sorted = curvature;
        std::sort(sorted.begin(), sorted.end());
        const auto percentileValue = [&sorted](double p) {
            if (sorted.empty()) {
                return 0.0;
            }
            const size_t idx = static_cast<size_t>(std::clamp(p, 0.0, 1.0) * static_cast<double>(sorted.size() - 1));
            return sorted[idx];
        };
        const double c_min = percentileValue(0.05);
        const double c_max = std::max(c_min + 1e-9, percentileValue(0.95));
        for (size_t i = 0; i < curvature.size(); ++i) {
            const double t = (curvature[i] - c_min) / (c_max - c_min);
            const auto rgb = heatColorFrom01(t);
            colors->SetTypedTuple(static_cast<vtkIdType>(i), rgb.data());
        }
    }

    mesh_polydata_->GetPointData()->SetScalars(colors);
    colors->Delete();

    mesh_mapper_->SetScalarModeToUsePointData();
    mesh_mapper_->SetColorModeToDirectScalars();
    mesh_mapper_->ScalarVisibilityOn();
    mesh_actor_->GetProperty()->SetColor(1.0, 1.0, 1.0);
}

void ModelView::setWireframeMode(bool enabled)
{
    if (mesh_actor_) {
        mesh_actor_->GetProperty()->SetRepresentationToWireframe();
        if (!enabled) {
            mesh_actor_->GetProperty()->SetRepresentationToSurface();
        }
        updateView();
    }
}

void ModelView::createPathActor(nbcam::LaserJob* job)
{
    if (path_actor_) {
        renderer_->RemoveActor(path_actor_);
        path_actor_->Delete();
        path_actor_ = nullptr;
    }
    if (path_mapper_) {
        path_mapper_->Delete();
        path_mapper_ = nullptr;
    }
    if (path_polydata_) {
        path_polydata_->Delete();
        path_polydata_ = nullptr;
    }
    path_colors_ = nullptr;
    path_cell_segment_ids_.clear();
    path_segment_cell_indices_.clear();
    path_cell_base_colors_.clear();
    path_cell_highlight_colors_.clear();
    for (vtkActor* actor : arrow_actors_) {
        renderer_->RemoveActor(actor);
        actor->Delete();
    }
    arrow_actors_.clear();

    if (!job || job->segments.empty()) {
        return;
    }
    const bool use_grayscale = job->grayscale_enabled;

    vtkPoints* points = vtkPoints::New();
    vtkCellArray* lines = vtkCellArray::New();
    vtkUnsignedCharArray* colors = vtkUnsignedCharArray::New();
    colors->SetNumberOfComponents(3);
    colors->SetName("Colors");

    const auto appendCellColor = [&](int segment_id,
                                     unsigned char base_r,
                                     unsigned char base_g,
                                     unsigned char base_b,
                                     unsigned char highlight_r,
                                     unsigned char highlight_g,
                                     unsigned char highlight_b,
                                     bool highlighted) {
        const vtkIdType cell_index = static_cast<vtkIdType>(path_cell_segment_ids_.size());
        path_cell_segment_ids_.push_back(segment_id);
        path_segment_cell_indices_[segment_id].push_back(cell_index);
        path_cell_base_colors_.push_back(base_r);
        path_cell_base_colors_.push_back(base_g);
        path_cell_base_colors_.push_back(base_b);
        path_cell_highlight_colors_.push_back(highlight_r);
        path_cell_highlight_colors_.push_back(highlight_g);
        path_cell_highlight_colors_.push_back(highlight_b);
        if (highlighted) {
            colors->InsertNextTuple3(highlight_r, highlight_g, highlight_b);
        } else {
            colors->InsertNextTuple3(base_r, base_g, base_b);
        }
    };

    double base_head_len = 0.8;
    if (mesh_polydata_) {
        double bounds[6];
        mesh_polydata_->GetBounds(bounds);
        const double max_dim = std::max({bounds[1] - bounds[0], bounds[3] - bounds[2], bounds[5] - bounds[4]});
        if (std::isfinite(max_dim) && max_dim > 1e-6) {
            base_head_len = std::clamp(max_dim * 0.006, 0.2, max_dim * 0.02);
        }
    }
    if (std::isfinite(path_spacing_hint_mm_) && path_spacing_hint_mm_ > 0.0) {
        constexpr double kRefSpacing = 0.2;
        const double spacing_scale = std::clamp(path_spacing_hint_mm_ / kRefSpacing, 0.45, 2.2);
        base_head_len *= spacing_scale;
    }

    const auto appendArrowHead = [&](int segment_id,
                                     const nbcam::PathPoint& from,
                                     const nbcam::PathPoint& tip,
                                     bool is_jump,
                                     bool highlighted,
                                     bool is_contour) {
        const double dx = tip.x - from.x;
        const double dy = tip.y - from.y;
        const double dz = tip.z - from.z;
        const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len <= 1e-6) {
            return;
        }

        const double ux = dx / len;
        const double uy = dy / len;
        const double uz = dz / len;
        const double head_len = std::min(base_head_len, len * 0.45);
        const double head_width = head_len * 0.45;

        double rx = 0.0, ry = 0.0, rz = 1.0;
        if (std::abs(uz) > 0.9) {
            rx = 0.0;
            ry = 1.0;
            rz = 0.0;
        }

        double sx = uy * rz - uz * ry;
        double sy = uz * rx - ux * rz;
        double sz = ux * ry - uy * rx;
        const double s_len = std::sqrt(sx * sx + sy * sy + sz * sz);
        if (s_len <= 1e-9) {
            return;
        }
        sx /= s_len;
        sy /= s_len;
        sz /= s_len;

        const double ax = tip.x - ux * head_len + sx * head_width;
        const double ay = tip.y - uy * head_len + sy * head_width;
        const double az = tip.z - uz * head_len + sz * head_width;

        const double bx = tip.x - ux * head_len - sx * head_width;
        const double by = tip.y - uy * head_len - sy * head_width;
        const double bz = tip.z - uz * head_len - sz * head_width;

        auto grayscaleToRgb = [](double gray01) -> std::array<unsigned char, 3> {
            const int gray = static_cast<int>(std::clamp(gray01 * 255.0, 0.0, 255.0));
            return {static_cast<unsigned char>(gray),
                    static_cast<unsigned char>(gray),
                    static_cast<unsigned char>(gray)};
        };

        unsigned char base_r = 220;
        unsigned char base_g = 20;
        unsigned char base_b = 20;
        unsigned char highlight_r = 56;
        unsigned char highlight_g = 200;
        unsigned char highlight_b = 88;
        if (is_jump) {
            base_r = 120;
            base_g = 120;
            base_b = 120;
            highlight_r = 45;
            highlight_g = 115;
            highlight_b = 255;
        } else if (use_grayscale) {
            const auto rgb = grayscaleToRgb(tip.grayscale);
            base_r = rgb[0];
            base_g = rgb[1];
            base_b = rgb[2];
        } else if (is_contour) {
            base_r = 255;
            base_g = 140;
            base_b = 0;
        }

        vtkIdType tip_id = points->InsertNextPoint(tip.x, tip.y, tip.z);
        vtkIdType a_id = points->InsertNextPoint(ax, ay, az);
        vtkIdType b_id = points->InsertNextPoint(bx, by, bz);

        vtkLine* line1 = vtkLine::New();
        line1->GetPointIds()->SetId(0, tip_id);
        line1->GetPointIds()->SetId(1, a_id);
        lines->InsertNextCell(line1);
        line1->Delete();
        appendCellColor(segment_id,
                        base_r,
                        base_g,
                        base_b,
                        highlight_r,
                        highlight_g,
                        highlight_b,
                        highlighted);

        vtkLine* line2 = vtkLine::New();
        line2->GetPointIds()->SetId(0, tip_id);
        line2->GetPointIds()->SetId(1, b_id);
        lines->InsertNextCell(line2);
        line2->Delete();
        appendCellColor(segment_id,
                        base_r,
                        base_g,
                        base_b,
                        highlight_r,
                        highlight_g,
                        highlight_b,
                        highlighted);
    };

    for (const auto& segment : job->segments) {
        if (segment.points.size() < 2) {
            continue;
        }

        const bool is_jump = (segment.type == nbcam::SegmentType::JUMP);
        const bool is_contour = (segment.strategy == nbcam::FillStrategy::CONTOUR);
        const bool is_highlighted = highlighted_segment_ids_.find(segment.id) != highlighted_segment_ids_.end();

        const auto grayscaleToRgb = [](double gray01) -> std::array<unsigned char, 3> {
            const int gray = static_cast<int>(std::clamp(gray01 * 255.0, 0.0, 255.0));
            return {static_cast<unsigned char>(gray),
                    static_cast<unsigned char>(gray),
                    static_cast<unsigned char>(gray)};
        };

        unsigned char base_r = 220;
        unsigned char base_g = 20;
        unsigned char base_b = 20;
        unsigned char highlight_r = 56;
        unsigned char highlight_g = 200;
        unsigned char highlight_b = 88;
        if (is_jump) {
            base_r = 120;
            base_g = 120;
            base_b = 120;
            highlight_r = 45;
            highlight_g = 115;
            highlight_b = 255;
        } else if (use_grayscale) {
            const double gray01 = segment.points.empty() ? 0.0 : segment.points.back().grayscale;
            const auto rgb = grayscaleToRgb(gray01);
            base_r = rgb[0];
            base_g = rgb[1];
            base_b = rgb[2];
        } else if (is_contour) {
            base_r = 255;
            base_g = 140;
            base_b = 0;
        }

        if (!is_jump) {
            std::vector<vtkIdType> ids;
            ids.reserve(segment.points.size());
            for (const auto& p : segment.points) {
                ids.push_back(points->InsertNextPoint(p.x, p.y, p.z));
            }

            for (size_t i = 0; i + 1 < ids.size(); ++i) {
                vtkLine* line = vtkLine::New();
                line->GetPointIds()->SetId(0, ids[i]);
                line->GetPointIds()->SetId(1, ids[i + 1]);
                lines->InsertNextCell(line);
                line->Delete();
                unsigned char line_r = base_r;
                unsigned char line_g = base_g;
                unsigned char line_b = base_b;
                if (use_grayscale) {
                    const double gray01 = (segment.points[i].grayscale + segment.points[i + 1].grayscale) * 0.5;
                    const auto rgb = grayscaleToRgb(gray01);
                    line_r = rgb[0];
                    line_g = rgb[1];
                    line_b = rgb[2];
                }
                appendCellColor(segment.id,
                                line_r,
                                line_g,
                                line_b,
                                highlight_r,
                                highlight_g,
                                highlight_b,
                                is_highlighted);
            }
            appendArrowHead(segment.id,
                            segment.points[segment.points.size() - 2],
                            segment.points.back(),
                            false,
                            is_highlighted,
                            is_contour);
            continue;
        }

        // 关光跳转：灰色虚线（按线段打断成短划线）
        const auto& start = segment.points.front();
        const auto& end = segment.points.back();

        constexpr int kDashParts = 12;
        for (int dash = 0; dash < kDashParts; dash += 2) {
            const double t0 = static_cast<double>(dash) / static_cast<double>(kDashParts);
            const double t1 = static_cast<double>(dash + 1) / static_cast<double>(kDashParts);
            const double x0 = start.x + (end.x - start.x) * t0;
            const double y0 = start.y + (end.y - start.y) * t0;
            const double z0 = start.z + (end.z - start.z) * t0;
            const double x1 = start.x + (end.x - start.x) * t1;
            const double y1 = start.y + (end.y - start.y) * t1;
            const double z1 = start.z + (end.z - start.z) * t1;

            vtkIdType id0 = points->InsertNextPoint(x0, y0, z0);
            vtkIdType id1 = points->InsertNextPoint(x1, y1, z1);
            vtkLine* line = vtkLine::New();
            line->GetPointIds()->SetId(0, id0);
            line->GetPointIds()->SetId(1, id1);
            lines->InsertNextCell(line);
            line->Delete();
            appendCellColor(segment.id,
                            base_r,
                            base_g,
                            base_b,
                            highlight_r,
                            highlight_g,
                            highlight_b,
                            is_highlighted);
        }
        appendArrowHead(segment.id, start, end, true, is_highlighted, false);
    }

    path_polydata_ = vtkPolyData::New();
    path_polydata_->SetPoints(points);
    path_polydata_->SetLines(lines);
    path_polydata_->GetCellData()->SetScalars(colors);
    path_colors_ = colors;
    points->Delete();
    lines->Delete();
    colors->Delete();

    path_mapper_ = vtkPolyDataMapper::New();
    path_mapper_->SetInputData(path_polydata_);
    path_mapper_->SetScalarModeToUseCellData();

    path_actor_ = vtkActor::New();
    path_actor_->SetMapper(path_mapper_);
    path_actor_->GetProperty()->SetLineWidth(1.0);  // 路径线细一些
    renderer_->AddActor(path_actor_);

}

void ModelView::updatePathHighlightColors()
{
    if (!path_colors_ || !path_polydata_) {
        if (current_job_) {
            createPathActor(current_job_);
        }
        return;
    }

    const size_t cell_count = path_cell_segment_ids_.size();
    if (path_cell_base_colors_.size() != cell_count * 3 ||
        path_cell_highlight_colors_.size() != cell_count * 3 ||
        static_cast<size_t>(path_colors_->GetNumberOfTuples()) != cell_count) {
        if (current_job_) {
            createPathActor(current_job_);
        }
        return;
    }

    for (size_t i = 0; i < cell_count; ++i) {
        const bool highlighted = highlighted_segment_ids_.find(path_cell_segment_ids_[i]) != highlighted_segment_ids_.end();
        const auto& source = highlighted ? path_cell_highlight_colors_ : path_cell_base_colors_;
        const unsigned char rgb[3] = {
            source[i * 3],
            source[i * 3 + 1],
            source[i * 3 + 2],
        };
        path_colors_->SetTypedTuple(static_cast<vtkIdType>(i), rgb);
    }

    path_colors_->Modified();
    path_polydata_->Modified();
}

void ModelView::updatePathHighlightColorsForSegment(int segment_id, bool highlighted)
{
    if (!path_colors_) {
        return;
    }

    const auto it = path_segment_cell_indices_.find(segment_id);
    if (it == path_segment_cell_indices_.end()) {
        return;
    }

    const std::vector<unsigned char>& source = highlighted ? path_cell_highlight_colors_ : path_cell_base_colors_;
    const size_t tuple_count = static_cast<size_t>(path_colors_->GetNumberOfTuples());
    for (vtkIdType cell_index : it->second) {
        if (cell_index < 0 || static_cast<size_t>(cell_index) >= tuple_count) {
            continue;
        }
        const size_t offset = static_cast<size_t>(cell_index) * 3;
        if (offset + 2 >= source.size()) {
            continue;
        }
        const unsigned char rgb[3] = {
            source[offset],
            source[offset + 1],
            source[offset + 2],
        };
        path_colors_->SetTypedTuple(cell_index, rgb);
    }
}

void ModelView::updateView()
{
    if (render_window_ && renderer_ && isEnabled() && isVisible()) {
        render_window_->Render();
    }
}

void ModelView::setUpdatesEnabled(bool enabled)
{
    QVTKOpenGLNativeWidget::setUpdatesEnabled(enabled);
    if (enabled && render_window_) {
        render_window_->Render();
    }
}

void ModelView::resetCamera()
{
    if (renderer_) {
        renderer_->ResetCamera();
        updateView();
    }
}

void ModelView::setupViewControls()
{
    // 相对路径
    QString iconBasePath = "../../../assets/view/";
    
    // 创建视图控制按钮容器
    view_controls_widget_ = new QWidget(this);
    view_controls_widget_->setStyleSheet(
        "QWidget { background-color: rgba(50, 50, 50, 200); border-radius: 5px; }"
        "QToolButton { background-color: rgba(70, 70, 70, 200); border: 1px solid rgba(100, 100, 100, 200); border-radius: 3px; padding: 5px; }"
        "QToolButton:hover { background-color: rgba(90, 90, 90, 200); }"
        "QToolButton:pressed { background-color: rgba(110, 110, 110, 200); }"
    );
    
    QHBoxLayout* layout = new QHBoxLayout(view_controls_widget_);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);
    
    // 创建视图按钮
    view_xy_btn_ = new QToolButton(view_controls_widget_);
    view_xy_btn_->setIcon(QIcon(iconBasePath + "xy_view.png"));
    view_xy_btn_->setToolTip("XY视图");
    view_xy_btn_->setIconSize(QSize(24, 24));
    connect(view_xy_btn_, &QToolButton::clicked, this, &ModelView::setViewXY);
    layout->addWidget(view_xy_btn_);
    
    view_xz_btn_ = new QToolButton(view_controls_widget_);
    view_xz_btn_->setIcon(QIcon(iconBasePath + "xz_view.png"));
    view_xz_btn_->setToolTip("XZ视图");
    view_xz_btn_->setIconSize(QSize(24, 24));
    connect(view_xz_btn_, &QToolButton::clicked, this, &ModelView::setViewXZ);
    layout->addWidget(view_xz_btn_);
    
    view_yz_btn_ = new QToolButton(view_controls_widget_);
    view_yz_btn_->setIcon(QIcon(iconBasePath + "yz_view.png"));
    view_yz_btn_->setToolTip("YZ视图");
    view_yz_btn_->setIconSize(QSize(24, 24));
    connect(view_yz_btn_, &QToolButton::clicked, this, &ModelView::setViewYZ);
    layout->addWidget(view_yz_btn_);
    
    view_default_btn_ = new QToolButton(view_controls_widget_);
    view_default_btn_->setIcon(QIcon(iconBasePath + "default_view.png"));
    view_default_btn_->setToolTip("默认视图");
    view_default_btn_->setIconSize(QSize(24, 24));
    connect(view_default_btn_, &QToolButton::clicked, this, &ModelView::setViewDefault);
    layout->addWidget(view_default_btn_);
    
    view_autofit_btn_ = new QToolButton(view_controls_widget_);
    view_autofit_btn_->setIcon(QIcon(iconBasePath + "autofit.png"));
    view_autofit_btn_->setToolTip("自动适应");
    view_autofit_btn_->setIconSize(QSize(24, 24));
    connect(view_autofit_btn_, &QToolButton::clicked, this, &ModelView::setViewAutoFit);
    layout->addWidget(view_autofit_btn_);
    
    view_controls_widget_->setLayout(layout);
    view_controls_widget_->setFixedSize(150, 40);
    view_controls_widget_->move(10, 10);  // 左上角位置
    view_controls_widget_->raise();  // 置于最上层
    view_controls_widget_->setAttribute(Qt::WA_TransparentForMouseEvents, false);  // 确保可以接收鼠标事件
    view_controls_widget_->show();  // 确保显示
}

void ModelView::setViewXY()
{
    if (!renderer_ || !renderer_->GetActiveCamera()) return;
    
    vtkCamera* camera = renderer_->GetActiveCamera();
    camera->SetPosition(0, 0, 1);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 1, 0);
    renderer_->ResetCamera();
    updateView();
}

void ModelView::setViewXZ()
{
    if (!renderer_ || !renderer_->GetActiveCamera()) return;
    
    vtkCamera* camera = renderer_->GetActiveCamera();
    camera->SetPosition(0, 1, 0);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 0, -1);
    renderer_->ResetCamera();
    updateView();
}

void ModelView::setViewYZ()
{
    if (!renderer_ || !renderer_->GetActiveCamera()) return;
    
    vtkCamera* camera = renderer_->GetActiveCamera();
    camera->SetPosition(1, 0, 0);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 1, 0);
    renderer_->ResetCamera();
    updateView();
}

void ModelView::setViewDefault()
{
    if (!renderer_ || !renderer_->GetActiveCamera()) return;
    
    vtkCamera* camera = renderer_->GetActiveCamera();
    camera->SetPosition(1, 1, 1);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 1, 0);
    renderer_->ResetCamera();
    updateView();
}

void ModelView::setViewAutoFit()
{
    resetCamera();
}

void ModelView::createAxes()
{
    if (!renderer_) {
        return;
    }
    
    // 坐标轴长度（初始值，会在updateAxesSize中更新）
    double axis_length = 50.0;
    
    // 创建X轴（蓝色）
    vtkPoints* x_points = vtkPoints::New();
    x_points->InsertNextPoint(0, 0, 0);
    x_points->InsertNextPoint(axis_length, 0, 0);
    
    vtkCellArray* x_lines = vtkCellArray::New();
    vtkLine* x_line = vtkLine::New();
    x_line->GetPointIds()->SetId(0, 0);
    x_line->GetPointIds()->SetId(1, 1);
    x_lines->InsertNextCell(x_line);
    x_line->Delete();
    
    vtkPolyData* x_polydata = vtkPolyData::New();
    x_polydata->SetPoints(x_points);
    x_polydata->SetLines(x_lines);
    
    vtkPolyDataMapper* x_mapper = vtkPolyDataMapper::New();
    x_mapper->SetInputData(x_polydata);
    
    axis_x_actor_ = vtkActor::New();
    axis_x_actor_->SetMapper(x_mapper);
    axis_x_actor_->GetProperty()->SetColor(0.0, 0.0, 1.0);  // 蓝色
    axis_x_actor_->GetProperty()->SetLineWidth(2.0);
    axis_x_actor_->SetVisibility(axes_visible_ ? 1 : 0);
    
    renderer_->AddActor(axis_x_actor_);
    
    x_points->Delete();
    x_lines->Delete();
    x_polydata->Delete();
    x_mapper->Delete();
    
    // 创建Y轴（绿色）
    vtkPoints* y_points = vtkPoints::New();
    y_points->InsertNextPoint(0, 0, 0);
    y_points->InsertNextPoint(0, axis_length, 0);
    
    vtkCellArray* y_lines = vtkCellArray::New();
    vtkLine* y_line = vtkLine::New();
    y_line->GetPointIds()->SetId(0, 0);
    y_line->GetPointIds()->SetId(1, 1);
    y_lines->InsertNextCell(y_line);
    y_line->Delete();
    
    vtkPolyData* y_polydata = vtkPolyData::New();
    y_polydata->SetPoints(y_points);
    y_polydata->SetLines(y_lines);
    
    vtkPolyDataMapper* y_mapper = vtkPolyDataMapper::New();
    y_mapper->SetInputData(y_polydata);
    
    axis_y_actor_ = vtkActor::New();
    axis_y_actor_->SetMapper(y_mapper);
    axis_y_actor_->GetProperty()->SetColor(0.0, 1.0, 0.0);  // 绿色
    axis_y_actor_->GetProperty()->SetLineWidth(2.0);
    axis_y_actor_->SetVisibility(axes_visible_ ? 1 : 0);
    
    renderer_->AddActor(axis_y_actor_);
    
    y_points->Delete();
    y_lines->Delete();
    y_polydata->Delete();
    y_mapper->Delete();
    
    // 创建Z轴（橙色）
    vtkPoints* z_points = vtkPoints::New();
    z_points->InsertNextPoint(0, 0, 0);
    z_points->InsertNextPoint(0, 0, axis_length);
    
    vtkCellArray* z_lines = vtkCellArray::New();
    vtkLine* z_line = vtkLine::New();
    z_line->GetPointIds()->SetId(0, 0);
    z_line->GetPointIds()->SetId(1, 1);
    z_lines->InsertNextCell(z_line);
    z_line->Delete();
    
    vtkPolyData* z_polydata = vtkPolyData::New();
    z_polydata->SetPoints(z_points);
    z_polydata->SetLines(z_lines);
    
    vtkPolyDataMapper* z_mapper = vtkPolyDataMapper::New();
    z_mapper->SetInputData(z_polydata);
    
    axis_z_actor_ = vtkActor::New();
    axis_z_actor_->SetMapper(z_mapper);
    axis_z_actor_->GetProperty()->SetColor(1.0, 0.5, 0.0);  // 橙色
    axis_z_actor_->GetProperty()->SetLineWidth(2.0);
    axis_z_actor_->SetVisibility(axes_visible_ ? 1 : 0);
    
    renderer_->AddActor(axis_z_actor_);
    
    z_points->Delete();
    z_lines->Delete();
    z_polydata->Delete();
    z_mapper->Delete();
}

bool ModelView::getMeshBounds(double bounds[6]) const
{
    if (!mesh_polydata_) {
        return false;
    }
    
    mesh_polydata_->GetBounds(bounds);
    
    // 检查边界是否有效
    if (bounds[0] >= bounds[1] || bounds[2] >= bounds[3] || bounds[4] >= bounds[5]) {
        return false;
    }
    
    return true;
}

void ModelView::updateAxesSize()
{
    if (!renderer_ || !mesh_polydata_) {
        return;
    }
    
    // 获取模型的边界
    double bounds[6];
    mesh_polydata_->GetBounds(bounds);
    
    // 计算边界框的对角线长度
    double dx = bounds[1] - bounds[0];  // x范围
    double dy = bounds[3] - bounds[2];  // y范围
    double dz = bounds[5] - bounds[4];  // z范围
    
    // 计算最大尺寸
    double max_dim = std::max({dx, dy, dz});
    
    // 坐标轴长度设为模型最大尺寸的1/6
    double axis_length = max_dim / 6.0;
    
    // 如果模型为空或太小，使用默认值
    if (max_dim < 1e-6) {
        axis_length = 50.0;
    }
    
    // 更新坐标轴（重新创建）
    if (axis_x_actor_) {
        renderer_->RemoveActor(axis_x_actor_);
        axis_x_actor_->Delete();
        axis_x_actor_ = nullptr;
    }
    if (axis_y_actor_) {
        renderer_->RemoveActor(axis_y_actor_);
        axis_y_actor_->Delete();
        axis_y_actor_ = nullptr;
    }
    if (axis_z_actor_) {
        renderer_->RemoveActor(axis_z_actor_);
        axis_z_actor_->Delete();
        axis_z_actor_ = nullptr;
    }
    
    // 重新创建坐标轴
    // X轴（蓝色）
    vtkPoints* x_points = vtkPoints::New();
    x_points->InsertNextPoint(0, 0, 0);
    x_points->InsertNextPoint(axis_length, 0, 0);
    
    vtkCellArray* x_lines = vtkCellArray::New();
    vtkLine* x_line = vtkLine::New();
    x_line->GetPointIds()->SetId(0, 0);
    x_line->GetPointIds()->SetId(1, 1);
    x_lines->InsertNextCell(x_line);
    x_line->Delete();
    
    vtkPolyData* x_polydata = vtkPolyData::New();
    x_polydata->SetPoints(x_points);
    x_polydata->SetLines(x_lines);
    
    vtkPolyDataMapper* x_mapper = vtkPolyDataMapper::New();
    x_mapper->SetInputData(x_polydata);
    
    axis_x_actor_ = vtkActor::New();
    axis_x_actor_->SetMapper(x_mapper);
    axis_x_actor_->GetProperty()->SetColor(0.0, 0.0, 1.0);  // 蓝色
    axis_x_actor_->GetProperty()->SetLineWidth(2.0);
    axis_x_actor_->SetVisibility(axes_visible_ ? 1 : 0);
    
    renderer_->AddActor(axis_x_actor_);
    
    x_points->Delete();
    x_lines->Delete();
    x_polydata->Delete();
    x_mapper->Delete();
    
    // Y轴（绿色）
    vtkPoints* y_points = vtkPoints::New();
    y_points->InsertNextPoint(0, 0, 0);
    y_points->InsertNextPoint(0, axis_length, 0);
    
    vtkCellArray* y_lines = vtkCellArray::New();
    vtkLine* y_line = vtkLine::New();
    y_line->GetPointIds()->SetId(0, 0);
    y_line->GetPointIds()->SetId(1, 1);
    y_lines->InsertNextCell(y_line);
    y_line->Delete();
    
    vtkPolyData* y_polydata = vtkPolyData::New();
    y_polydata->SetPoints(y_points);
    y_polydata->SetLines(y_lines);
    
    vtkPolyDataMapper* y_mapper = vtkPolyDataMapper::New();
    y_mapper->SetInputData(y_polydata);
    
    axis_y_actor_ = vtkActor::New();
    axis_y_actor_->SetMapper(y_mapper);
    axis_y_actor_->GetProperty()->SetColor(0.0, 1.0, 0.0);  // 绿色
    axis_y_actor_->GetProperty()->SetLineWidth(2.0);
    axis_y_actor_->SetVisibility(axes_visible_ ? 1 : 0);
    
    renderer_->AddActor(axis_y_actor_);
    
    y_points->Delete();
    y_lines->Delete();
    y_polydata->Delete();
    y_mapper->Delete();
    
    // Z轴（橙色）
    vtkPoints* z_points = vtkPoints::New();
    z_points->InsertNextPoint(0, 0, 0);
    z_points->InsertNextPoint(0, 0, axis_length);
    
    vtkCellArray* z_lines = vtkCellArray::New();
    vtkLine* z_line = vtkLine::New();
    z_line->GetPointIds()->SetId(0, 0);
    z_line->GetPointIds()->SetId(1, 1);
    z_lines->InsertNextCell(z_line);
    z_line->Delete();
    
    vtkPolyData* z_polydata = vtkPolyData::New();
    z_polydata->SetPoints(z_points);
    z_polydata->SetLines(z_lines);
    
    vtkPolyDataMapper* z_mapper = vtkPolyDataMapper::New();
    z_mapper->SetInputData(z_polydata);
    
    axis_z_actor_ = vtkActor::New();
    axis_z_actor_->SetMapper(z_mapper);
    axis_z_actor_->GetProperty()->SetColor(1.0, 0.5, 0.0);  // 橙色
    axis_z_actor_->GetProperty()->SetLineWidth(2.0);
    axis_z_actor_->SetVisibility(axes_visible_ ? 1 : 0);
    
    renderer_->AddActor(axis_z_actor_);
    
    z_points->Delete();
    z_lines->Delete();
    z_polydata->Delete();
    z_mapper->Delete();
}

void ModelView::createBoundingBox()
{
    if (!renderer_ || !mesh_polydata_) {
        return;
    }
    
    // 获取模型的边界
    double bounds[6];
    mesh_polydata_->GetBounds(bounds);
    
    // 如果模型为空，不创建包围盒
    if (bounds[0] >= bounds[1] || bounds[2] >= bounds[3] || bounds[4] >= bounds[5]) {
        return;
    }
    
    // 删除旧的包围盒
    if (bounding_box_actor_) {
        renderer_->RemoveActor(bounding_box_actor_);
        bounding_box_actor_->Delete();
        bounding_box_actor_ = nullptr;
    }
    if (bounding_box_mapper_) {
        bounding_box_mapper_->Delete();
        bounding_box_mapper_ = nullptr;
    }
    if (bounding_box_polydata_) {
        bounding_box_polydata_->Delete();
        bounding_box_polydata_ = nullptr;
    }
    
    // 创建包围盒的8个顶点
    vtkPoints* points = vtkPoints::New();
    points->InsertNextPoint(bounds[0], bounds[2], bounds[4]);  // 0: 左下后
    points->InsertNextPoint(bounds[1], bounds[2], bounds[4]);  // 1: 右下后
    points->InsertNextPoint(bounds[1], bounds[3], bounds[4]);  // 2: 右上后
    points->InsertNextPoint(bounds[0], bounds[3], bounds[4]);  // 3: 左上后
    points->InsertNextPoint(bounds[0], bounds[2], bounds[5]);  // 4: 左下前
    points->InsertNextPoint(bounds[1], bounds[2], bounds[5]);  // 5: 右下前
    points->InsertNextPoint(bounds[1], bounds[3], bounds[5]);  // 6: 右上前
    points->InsertNextPoint(bounds[0], bounds[3], bounds[5]);  // 7: 左上前
    
    // 创建12条边（立方体的12条边）
    vtkCellArray* lines = vtkCellArray::New();
    
    // 后面4条边
    vtkLine* line1 = vtkLine::New();
    line1->GetPointIds()->SetId(0, 0);
    line1->GetPointIds()->SetId(1, 1);
    lines->InsertNextCell(line1);
    line1->Delete();
    
    vtkLine* line2 = vtkLine::New();
    line2->GetPointIds()->SetId(0, 1);
    line2->GetPointIds()->SetId(1, 2);
    lines->InsertNextCell(line2);
    line2->Delete();
    
    vtkLine* line3 = vtkLine::New();
    line3->GetPointIds()->SetId(0, 2);
    line3->GetPointIds()->SetId(1, 3);
    lines->InsertNextCell(line3);
    line3->Delete();
    
    vtkLine* line4 = vtkLine::New();
    line4->GetPointIds()->SetId(0, 3);
    line4->GetPointIds()->SetId(1, 0);
    lines->InsertNextCell(line4);
    line4->Delete();
    
    // 前面4条边
    vtkLine* line5 = vtkLine::New();
    line5->GetPointIds()->SetId(0, 4);
    line5->GetPointIds()->SetId(1, 5);
    lines->InsertNextCell(line5);
    line5->Delete();
    
    vtkLine* line6 = vtkLine::New();
    line6->GetPointIds()->SetId(0, 5);
    line6->GetPointIds()->SetId(1, 6);
    lines->InsertNextCell(line6);
    line6->Delete();
    
    vtkLine* line7 = vtkLine::New();
    line7->GetPointIds()->SetId(0, 6);
    line7->GetPointIds()->SetId(1, 7);
    lines->InsertNextCell(line7);
    line7->Delete();
    
    vtkLine* line8 = vtkLine::New();
    line8->GetPointIds()->SetId(0, 7);
    line8->GetPointIds()->SetId(1, 4);
    lines->InsertNextCell(line8);
    line8->Delete();
    
    // 连接前后面的4条边
    vtkLine* line9 = vtkLine::New();
    line9->GetPointIds()->SetId(0, 0);
    line9->GetPointIds()->SetId(1, 4);
    lines->InsertNextCell(line9);
    line9->Delete();
    
    vtkLine* line10 = vtkLine::New();
    line10->GetPointIds()->SetId(0, 1);
    line10->GetPointIds()->SetId(1, 5);
    lines->InsertNextCell(line10);
    line10->Delete();
    
    vtkLine* line11 = vtkLine::New();
    line11->GetPointIds()->SetId(0, 2);
    line11->GetPointIds()->SetId(1, 6);
    lines->InsertNextCell(line11);
    line11->Delete();
    
    vtkLine* line12 = vtkLine::New();
    line12->GetPointIds()->SetId(0, 3);
    line12->GetPointIds()->SetId(1, 7);
    lines->InsertNextCell(line12);
    line12->Delete();
    
    // 创建PolyData
    bounding_box_polydata_ = vtkPolyData::New();
    bounding_box_polydata_->SetPoints(points);
    bounding_box_polydata_->SetLines(lines);
    
    points->Delete();
    lines->Delete();
    
    // 创建Mapper和Actor
    bounding_box_mapper_ = vtkPolyDataMapper::New();
    bounding_box_mapper_->SetInputData(bounding_box_polydata_);
    
    bounding_box_actor_ = vtkActor::New();
    bounding_box_actor_->SetMapper(bounding_box_mapper_);
    bounding_box_actor_->GetProperty()->SetColor(1.0, 1.0, 0.0);  // 黄色
    bounding_box_actor_->GetProperty()->SetLineWidth(1.5);
    bounding_box_actor_->GetProperty()->SetOpacity(0.8);
    
    // 根据bounding_box_visible_决定是否显示
    if (bounding_box_visible_) {
        renderer_->AddActor(bounding_box_actor_);
    }
}

void ModelView::createProcessingRegionBox()
{
    if (!renderer_) {
        return;
    }

    if (processing_region_actor_) {
        renderer_->RemoveActor(processing_region_actor_);
        processing_region_actor_->Delete();
        processing_region_actor_ = nullptr;
    }
    if (processing_region_mapper_) {
        processing_region_mapper_->Delete();
        processing_region_mapper_ = nullptr;
    }
    if (processing_region_polydata_) {
        processing_region_polydata_->Delete();
        processing_region_polydata_ = nullptr;
    }

    constexpr double kMin = -32.768;
    constexpr double kMax = 32.768;

    vtkPoints* points = vtkPoints::New();
    points->InsertNextPoint(kMin, kMin, kMin);
    points->InsertNextPoint(kMax, kMin, kMin);
    points->InsertNextPoint(kMax, kMax, kMin);
    points->InsertNextPoint(kMin, kMax, kMin);
    points->InsertNextPoint(kMin, kMin, kMax);
    points->InsertNextPoint(kMax, kMin, kMax);
    points->InsertNextPoint(kMax, kMax, kMax);
    points->InsertNextPoint(kMin, kMax, kMax);

    vtkCellArray* lines = vtkCellArray::New();
    const std::array<std::array<vtkIdType, 2>, 12> edges = {{
        {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
        {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
        {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}},
    }};
    for (const auto& edge : edges) {
        vtkLine* line = vtkLine::New();
        line->GetPointIds()->SetId(0, edge[0]);
        line->GetPointIds()->SetId(1, edge[1]);
        lines->InsertNextCell(line);
        line->Delete();
    }

    processing_region_polydata_ = vtkPolyData::New();
    processing_region_polydata_->SetPoints(points);
    processing_region_polydata_->SetLines(lines);
    points->Delete();
    lines->Delete();

    processing_region_mapper_ = vtkPolyDataMapper::New();
    processing_region_mapper_->SetInputData(processing_region_polydata_);

    processing_region_actor_ = vtkActor::New();
    processing_region_actor_->SetMapper(processing_region_mapper_);
    processing_region_actor_->GetProperty()->SetColor(1.0, 1.0, 0.0);
    processing_region_actor_->GetProperty()->SetLineWidth(2.0);
    processing_region_actor_->GetProperty()->SetOpacity(0.95);

    if (processing_region_visible_) {
        renderer_->AddActor(processing_region_actor_);
    }
}

void ModelView::setBoundingBoxVisible(bool visible)
{
    bounding_box_visible_ = visible;
    
    if (!renderer_) {
        return;
    }
    
    if (visible) {
        if (bounding_box_actor_ && !renderer_->HasViewProp(bounding_box_actor_)) {
            renderer_->AddActor(bounding_box_actor_);
        }
    } else {
        if (bounding_box_actor_ && renderer_->HasViewProp(bounding_box_actor_)) {
            renderer_->RemoveActor(bounding_box_actor_);
        }
    }
    
    updateView();
}

void ModelView::setAxesVisible(bool visible)
{
    if (axes_visible_ == visible) {
        return;
    }
    axes_visible_ = visible;
    if (axis_x_actor_) {
        axis_x_actor_->SetVisibility(axes_visible_ ? 1 : 0);
    }
    if (axis_y_actor_) {
        axis_y_actor_->SetVisibility(axes_visible_ ? 1 : 0);
    }
    if (axis_z_actor_) {
        axis_z_actor_->SetVisibility(axes_visible_ ? 1 : 0);
    }
    updateView();
}

void ModelView::setProcessingRegionVisible(bool visible)
{
    processing_region_visible_ = visible;
    if (!renderer_) {
        return;
    }

    if (!processing_region_actor_) {
        createProcessingRegionBox();
    }

    if (visible) {
        if (processing_region_actor_ && !renderer_->HasViewProp(processing_region_actor_)) {
            renderer_->AddActor(processing_region_actor_);
        }
    } else {
        if (processing_region_actor_ && renderer_->HasViewProp(processing_region_actor_)) {
            renderer_->RemoveActor(processing_region_actor_);
        }
    }
    updateView();
}

void ModelView::setWorkingPlanePose(double pos_x, double pos_y, double pos_z,
                                    double rot_x, double rot_y, double rot_z)
{
    // 将位置和旋转转换为平面方程 ax + by + cz + d = 0
    // 旋转使用欧拉角（ZYX顺序）
    // 默认平面法向量为 (0, 0, 1)，经过旋转后得到新的法向量
    
    // 简化的实现：根据旋转角度计算法向量
    // 完整实现应该使用旋转矩阵
    double rot_x_rad = rot_x * M_PI / 180.0;
    double rot_y_rad = rot_y * M_PI / 180.0;
    double rot_z_rad = rot_z * M_PI / 180.0;
    
    // 计算旋转后的法向量（简化：只考虑Y和X轴旋转）
    double nx = sin(rot_y_rad);
    double ny = -sin(rot_x_rad) * cos(rot_y_rad);
    double nz = cos(rot_x_rad) * cos(rot_y_rad);
    
    // 归一化
    double len = sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 1e-10) {
        nx /= len;
        ny /= len;
        nz /= len;
    } else {
        nx = 0.0;
        ny = 0.0;
        nz = 1.0;
    }
    
    // 计算d：d = -(nx*px + ny*py + nz*pz)
    double d = -(nx * pos_x + ny * pos_y + nz * pos_z);
    
    createPlaneActor(nx, ny, nz, d);
    updateView();
}

void ModelView::removeWorkingPlane()
{
    if (!renderer_) {
        return;
    }
    
    // 移除平面actor
    if (plane_actor_) {
        renderer_->RemoveActor(plane_actor_);
        plane_actor_->Delete();
        plane_actor_ = nullptr;
    }
    
    // 清理mapper和polydata
    if (plane_mapper_) {
        plane_mapper_->Delete();
        plane_mapper_ = nullptr;
    }
    
    if (plane_polydata_) {
        plane_polydata_->Delete();
        plane_polydata_ = nullptr;
    }
    
    updateView();
}

void ModelView::createPlaneActor(double a, double b, double c, double d)
{
    if (!renderer_) {
        return;
    }
    
    // 移除旧的平面
    if (plane_actor_) {
        renderer_->RemoveActor(plane_actor_);
        plane_actor_->Delete();
        plane_actor_ = nullptr;
    }
    
    // 归一化法向量
    double len = sqrt(a * a + b * b + c * c);
    if (len < 1e-10) {
        spdlog::warn("平面法向量为零，无法创建平面");
        return;
    }
    
    double nx = a / len;
    double ny = b / len;
    double nz = c / len;
    double dist = d / len;
    
    // 计算平面上的一点（如果d != 0，选择距离原点最近的点）
    double px = 0.0, py = 0.0, pz = 0.0;
    if (abs(nz) > 1e-6) {
        pz = -dist / nz;
    } else if (abs(ny) > 1e-6) {
        py = -dist / ny;
    } else if (abs(nx) > 1e-6) {
        px = -dist / nx;
    }
    
    // 获取模型边界以计算平面大小
    double bounds[6] = {0, 0, 0, 0, 0, 0};
    bool has_bounds = false;
    if (mesh_polydata_) {
        mesh_polydata_->GetBounds(bounds);
        has_bounds = (bounds[0] < bounds[1] && bounds[2] < bounds[3] && bounds[4] < bounds[5]);
    }
    
    // 计算平面上两个正交方向的大小（与包围盒一致）
    double plane_size_u = 200.0;  // 默认值
    double plane_size_v = 200.0;  // 默认值
    
    if (has_bounds) {
        double dx = bounds[1] - bounds[0];  // x范围
        double dy = bounds[3] - bounds[2];  // y范围
        double dz = bounds[5] - bounds[4];  // z范围
        
        // 根据法向量方向确定平面的两个正交方向
        // 如果法向量接近(0,1,0)（XZ平面），则使用X和Z的范围
        // 如果法向量接近(1,0,0)（YZ平面），则使用Y和Z的范围
        // 如果法向量接近(0,0,1)（XY平面），则使用X和Y的范围
        
        // 计算法向量在三个轴上的投影绝对值
        double nx_abs = std::abs(nx);
        double ny_abs = std::abs(ny);
        double nz_abs = std::abs(nz);
        
        if (ny_abs > nx_abs && ny_abs > nz_abs) {
            // 法向量主要沿Y轴（XZ平面）
            plane_size_u = dx * 1.2;  // X方向，增加20%边距
            plane_size_v = dz * 1.2;  // Z方向，增加20%边距
        } else if (nx_abs > ny_abs && nx_abs > nz_abs) {
            // 法向量主要沿X轴（YZ平面）
            plane_size_u = dy * 1.2;  // Y方向
            plane_size_v = dz * 1.2;  // Z方向
        } else {
            // 法向量主要沿Z轴（XY平面）
            plane_size_u = dx * 1.2;  // X方向
            plane_size_v = dy * 1.2;  // Y方向
        }
        
        // 确保最小值
        plane_size_u = std::max(plane_size_u, 10.0);
        plane_size_v = std::max(plane_size_v, 10.0);
    }
    
    // 创建平面源
    vtkPlaneSource* plane_source = vtkPlaneSource::New();
    plane_source->SetCenter(px, py, pz);
    plane_source->SetNormal(nx, ny, nz);
    plane_source->SetResolution(10, 10);
    
    // 计算平面上的两个正交方向向量
    // 首先找到一个与法向量垂直的向量
    double ux = 0.0, uy = 0.0, uz = 0.0;
    double vx = 0.0, vy = 0.0, vz = 0.0;
    
    // 选择一个与法向量不平行的基准向量
    if (std::abs(nx) < 0.9) {
        // 使用X轴作为基准
        ux = 1.0; uy = 0.0; uz = 0.0;
    } else if (std::abs(ny) < 0.9) {
        // 使用Y轴作为基准
        ux = 0.0; uy = 1.0; uz = 0.0;
    } else {
        // 使用Z轴作为基准
        ux = 0.0; uy = 0.0; uz = 1.0;
    }
    
    // 计算第一个方向向量（与法向量垂直）
    // u = (基准向量 × 法向量) / |基准向量 × 法向量|
    double cross_x = uy * nz - uz * ny;
    double cross_y = uz * nx - ux * nz;
    double cross_z = ux * ny - uy * nx;
    double cross_len = std::sqrt(cross_x * cross_x + cross_y * cross_y + cross_z * cross_z);
    
    if (cross_len > 1e-6) {
        ux = cross_x / cross_len;
        uy = cross_y / cross_len;
        uz = cross_z / cross_len;
    } else {
        // 如果叉积为零，使用另一个基准向量
        ux = 0.0; uy = 1.0; uz = 0.0;
        cross_x = uy * nz - uz * ny;
        cross_y = uz * nx - ux * nz;
        cross_z = ux * ny - uy * nx;
        cross_len = std::sqrt(cross_x * cross_x + cross_y * cross_y + cross_z * cross_z);
        if (cross_len > 1e-6) {
            ux = cross_x / cross_len;
            uy = cross_y / cross_len;
            uz = cross_z / cross_len;
        }
    }
    
    // 计算第二个方向向量（与法向量和第一个方向向量都垂直）
    // v = 法向量 × u
    vx = ny * uz - nz * uy;
    vy = nz * ux - nx * uz;
    vz = nx * uy - ny * ux;
    double v_len = std::sqrt(vx * vx + vy * vy + vz * vz);
    if (v_len > 1e-6) {
        vx /= v_len;
        vy /= v_len;
        vz /= v_len;
    }
    
    // 设置平面的两个点（定义平面的两个方向，大小与包围盒一致）
    double point1_x = px + ux * plane_size_u * 0.5;
    double point1_y = py + uy * plane_size_u * 0.5;
    double point1_z = pz + uz * plane_size_u * 0.5;
    
    double point2_x = px + vx * plane_size_v * 0.5;
    double point2_y = py + vy * plane_size_v * 0.5;
    double point2_z = pz + vz * plane_size_v * 0.5;
    
    plane_source->SetPoint1(point1_x, point1_y, point1_z);
    plane_source->SetPoint2(point2_x, point2_y, point2_z);
    plane_source->Update();
    
    // 创建Mapper和Actor
    if (plane_mapper_) {
        plane_mapper_->Delete();
    }
    plane_mapper_ = vtkPolyDataMapper::New();
    plane_mapper_->SetInputConnection(plane_source->GetOutputPort());
    
    plane_actor_ = vtkActor::New();
    plane_actor_->SetMapper(plane_mapper_);
    plane_actor_->GetProperty()->SetColor(1.0, 1.0, 0.0);  // 黄色
    plane_actor_->GetProperty()->SetOpacity(0.3);
    plane_actor_->GetProperty()->SetRepresentationToSurface();
    
    renderer_->AddActor(plane_actor_);
    
    plane_source->Delete();
}

void ModelView::keyPressEvent(QKeyEvent* event)
{
    bool key_handled = false;
    
    switch (event->key()) {
        case Qt::Key_W:
            if (!key_w_pressed_) {
                key_w_pressed_ = true;
                startCameraMoveTimer();
            }
            key_handled = true;
            break;
        case Qt::Key_A:
            if (!key_a_pressed_) {
                key_a_pressed_ = true;
                startCameraMoveTimer();
            }
            key_handled = true;
            break;
        case Qt::Key_S:
            if (!key_s_pressed_) {
                key_s_pressed_ = true;
                startCameraMoveTimer();
            }
            key_handled = true;
            break;
        case Qt::Key_D:
            if (!key_d_pressed_) {
                key_d_pressed_ = true;
                startCameraMoveTimer();
            }
            key_handled = true;
            break;
        case Qt::Key_Q:
            if (!key_q_pressed_) {
                key_q_pressed_ = true;
                startCameraMoveTimer();
            }
            key_handled = true;
            break;
        case Qt::Key_E:
            if (!key_e_pressed_) {
                key_e_pressed_ = true;
                startCameraMoveTimer();
            }
            key_handled = true;
            break;
    }
    
    if (!key_handled) {
        QVTKOpenGLNativeWidget::keyPressEvent(event);
    }
}

void ModelView::keyReleaseEvent(QKeyEvent* event)
{
    bool key_handled = false;
    
    switch (event->key()) {
        case Qt::Key_W:
            key_w_pressed_ = false;
            stopCameraMoveTimerIfNeeded();
            key_handled = true;
            break;
        case Qt::Key_A:
            key_a_pressed_ = false;
            stopCameraMoveTimerIfNeeded();
            key_handled = true;
            break;
        case Qt::Key_S:
            key_s_pressed_ = false;
            stopCameraMoveTimerIfNeeded();
            key_handled = true;
            break;
        case Qt::Key_D:
            key_d_pressed_ = false;
            stopCameraMoveTimerIfNeeded();
            key_handled = true;
            break;
        case Qt::Key_Q:
            key_q_pressed_ = false;
            stopCameraMoveTimerIfNeeded();
            key_handled = true;
            break;
        case Qt::Key_E:
            key_e_pressed_ = false;
            stopCameraMoveTimerIfNeeded();
            key_handled = true;
            break;
    }
    
    if (!key_handled) {
        QVTKOpenGLNativeWidget::keyReleaseEvent(event);
    }
}

void ModelView::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == camera_move_timer_id_) {
        updateCameraPosition();
    } else {
        QVTKOpenGLNativeWidget::timerEvent(event);
    }
}

void ModelView::startCameraMoveTimer()
{
    if (camera_move_timer_id_ == -1) {
        camera_move_timer_id_ = startTimer(16);  // 约60fps
    }
}

void ModelView::stopCameraMoveTimerIfNeeded()
{
    if (!key_w_pressed_ && !key_a_pressed_ && !key_s_pressed_ && 
        !key_d_pressed_ && !key_q_pressed_ && !key_e_pressed_) {
        if (camera_move_timer_id_ != -1) {
            killTimer(camera_move_timer_id_);
            camera_move_timer_id_ = -1;
        }
    }
}

void ModelView::updateCameraPosition()
{
    if (!renderer_ || !renderer_->GetActiveCamera()) {
        return;
    }
    
    vtkCamera* camera = renderer_->GetActiveCamera();
    
    // 获取当前相机位置和焦点
    double pos[3], focal[3], up[3];
    camera->GetPosition(pos);
    camera->GetFocalPoint(focal);
    camera->GetViewUp(up);
    
    // 计算相机的前、右、上方向向量
    double forward[3], right[3], camera_up[3];
    for (int i = 0; i < 3; i++) {
        forward[i] = focal[i] - pos[i];
    }
    
    // 归一化前向量
    double forward_len = sqrt(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
    if (forward_len > 1e-6) {
        forward[0] /= forward_len;
        forward[1] /= forward_len;
        forward[2] /= forward_len;
    }
    
    // 计算右向量（前向量 × 上向量）
    right[0] = forward[1] * up[2] - forward[2] * up[1];
    right[1] = forward[2] * up[0] - forward[0] * up[2];
    right[2] = forward[0] * up[1] - forward[1] * up[0];
    
    // 归一化右向量
    double right_len = sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
    if (right_len > 1e-6) {
        right[0] /= right_len;
        right[1] /= right_len;
        right[2] /= right_len;
    }
    
    // 计算移动距离（基于模型大小）
    double move_distance = 0.0;
    if (mesh_polydata_) {
        double bounds[6];
        mesh_polydata_->GetBounds(bounds);
        double dx = bounds[1] - bounds[0];
        double dy = bounds[3] - bounds[2];
        double dz = bounds[5] - bounds[4];
        double max_dim = std::max({dx, dy, dz});
        move_distance = max_dim * camera_move_speed_;
    } else {
        move_distance = 10.0;  // 默认移动距离
    }
    
    // 根据按键状态移动相机
    if (key_w_pressed_) {
        // W: 向前移动（沿前向量方向）
        pos[0] += forward[0] * move_distance;
        pos[1] += forward[1] * move_distance;
        pos[2] += forward[2] * move_distance;
        focal[0] += forward[0] * move_distance;
        focal[1] += forward[1] * move_distance;
        focal[2] += forward[2] * move_distance;
    }
    if (key_s_pressed_) {
        // S: 向后移动（沿前向量反方向）
        pos[0] -= forward[0] * move_distance;
        pos[1] -= forward[1] * move_distance;
        pos[2] -= forward[2] * move_distance;
        focal[0] -= forward[0] * move_distance;
        focal[1] -= forward[1] * move_distance;
        focal[2] -= forward[2] * move_distance;
    }
    if (key_a_pressed_) {
        // A: 向左移动（沿右向量反方向）
        pos[0] -= right[0] * move_distance;
        pos[1] -= right[1] * move_distance;
        pos[2] -= right[2] * move_distance;
        focal[0] -= right[0] * move_distance;
        focal[1] -= right[1] * move_distance;
        focal[2] -= right[2] * move_distance;
    }
    if (key_d_pressed_) {
        // D: 向右移动（沿右向量方向）
        pos[0] += right[0] * move_distance;
        pos[1] += right[1] * move_distance;
        pos[2] += right[2] * move_distance;
        focal[0] += right[0] * move_distance;
        focal[1] += right[1] * move_distance;
        focal[2] += right[2] * move_distance;
    }
    if (key_q_pressed_) {
        // Q: 向上移动（沿上向量方向）
        pos[0] += up[0] * move_distance;
        pos[1] += up[1] * move_distance;
        pos[2] += up[2] * move_distance;
        focal[0] += up[0] * move_distance;
        focal[1] += up[1] * move_distance;
        focal[2] += up[2] * move_distance;
    }
    if (key_e_pressed_) {
        // E: 向下移动（沿上向量反方向）
        pos[0] -= up[0] * move_distance;
        pos[1] -= up[1] * move_distance;
        pos[2] -= up[2] * move_distance;
        focal[0] -= up[0] * move_distance;
        focal[1] -= up[1] * move_distance;
        focal[2] -= up[2] * move_distance;
    }
    
    // 更新相机位置
    camera->SetPosition(pos);
    camera->SetFocalPoint(focal);
    
    updateView();
}

// Removed: clipMeshWithPlane - plane clipping functionality removed

// Patch相关方法实现
void ModelView::clusterPatches(double dihedral_threshold)
{
    if (!current_mesh_) {
        spdlog::warn("没有加载模型，无法进行patch聚类");
        return;
    }
    
    // 保存当前选中的patch ID（如果存在）
    int previous_selected_patch_id = selected_patch_id_;
    
    patches_ = patch_clusterer_->clusterPatches(*current_mesh_, dihedral_threshold);
    spdlog::info("面片聚类完成，共 {} 个面片", patches_.size());
    
    // 如果之前有选中的patch，且该patch仍然有效，则恢复选中状态
    if (previous_selected_patch_id >= 0 && 
        previous_selected_patch_id < static_cast<int>(patches_.size())) {
        // 检查是否是同一个patch（通过比较三角形数量等特征）
        // 这里简单恢复选中状态
        selected_patch_id_ = previous_selected_patch_id;
        highlightPatch(previous_selected_patch_id);
        spdlog::info("已恢复选中面片: {}", previous_selected_patch_id);
    } else {
        // 清除之前的选中状态（因为patch数量或结构已改变）
        selected_patch_id_ = -1;
        highlighted_patch_id_ = -1;
        removePatchHighlightActor();
    }
}

void ModelView::highlightPatch(int patch_id)
{
    if (highlighted_patch_id_ == patch_id) {
        return;  // 已经高亮，无需重复
    }
    
    // 移除之前的高亮
    removePatchHighlightActor();
    
    if (patch_id < 0 || patch_id >= static_cast<int>(patches_.size())) {
        highlighted_patch_id_ = -1;
        updateView();
        return;
    }
    
    highlighted_patch_id_ = patch_id;
    createPatchHighlightActor(patch_id);
    updateView();
}

void ModelView::selectPatch(int patch_id)
{
    if (patch_id < 0 || patch_id >= static_cast<int>(patches_.size())) {
        selected_patch_id_ = -1;
        highlighted_patch_id_ = -1;
        removePatchHighlightActor();
        emit patchSelected(-1);
        updateView();
        return;
    }
    
    // 如果选择的是不同的patch，记录日志
    if (selected_patch_id_ != patch_id && selected_patch_id_ >= 0) {
        spdlog::info("切换面片选择: 从面片 {} 切换到面片 {}", selected_patch_id_, patch_id);
    }
    
    selected_patch_id_ = patch_id;
    emit patchSelected(patch_id);
    
    // 强制高亮选中的patch（即使已经高亮也要重新设置，确保显示）
    highlighted_patch_id_ = patch_id;
    removePatchHighlightActor();  // 先移除旧的
    createPatchHighlightActor(patch_id);  // 重新创建高亮
    updateView();
    
    // 在日志中记录选中的patch信息
    if (patch_id < static_cast<int>(patches_.size())) {
        const auto& patch = patches_[patch_id];
        spdlog::info("面片已选定并高亮: ID={}, 三角形数={}, 面积={:.4f}, 法向量=({:.3f}, {:.3f}, {:.3f})（高亮将保持直到选择其他面片）",
                     patch_id, patch.triangle_indices.size(), patch.total_area,
                     patch.normal_x, patch.normal_y, patch.normal_z);
    }
}

void ModelView::clearPatchSelection()
{
    selected_patch_id_ = -1;
    highlighted_patch_id_ = -1;
    removePatchHighlightActor();
    emit patchSelected(-1);
    updateView();
}

const std::vector<nbcam::Patch>& ModelView::getPatches() const
{
    return patches_;
}

int ModelView::findPatchByNormal(double nx, double ny, double nz, double angle_tolerance) const
{
    if (patches_.empty()) {
        return -1;
    }
    
    // 归一化目标法向量
    double len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len < 1e-10) {
        return -1;
    }
    nx /= len;
    ny /= len;
    nz /= len;
    
    int best_patch_index = -1;
    double best_dot = -1.0;  // 点积越大，角度越小
    
    // 查找法向量最接近目标方向的patch
    // 注意：返回的是在patches_向量中的索引，不是patch.id
    for (size_t i = 0; i < patches_.size(); ++i) {
        const auto& patch = patches_[i];
        
        // 计算patch法向量与目标法向量的点积
        double dot = patch.normal_x * nx + patch.normal_y * ny + patch.normal_z * nz;
        
        // 计算角度（arccos）
        dot = std::max(-1.0, std::min(1.0, dot));  // 限制在[-1, 1]
        double angle = std::acos(dot);
        
        // 如果角度在容差范围内，且是当前最佳匹配
        if (angle <= angle_tolerance && dot > best_dot) {
            best_dot = dot;
            best_patch_index = static_cast<int>(i);  // 返回索引而不是patch.id
        }
    }
    
    return best_patch_index;
}

int ModelView::pickTriangle(int x, int y)
{
    if (!render_window_ || !renderer_ || !mesh_polydata_) {
        return -1;
    }
    
    // 使用VTK的CellPicker进行拾取（更简单可靠）
    vtkCellPicker* picker = vtkCellPicker::New();
    picker->SetTolerance(0.001);
    
    int* size = render_window_->GetSize();
    
    // 执行拾取（注意Y坐标需要翻转）
    int picked = picker->Pick(x, size[1] - y - 1, 0, renderer_);
    
    int cell_id = -1;
    if (picked && picker->GetCellId() >= 0) {
        cell_id = picker->GetCellId();
    }
    
    picker->Delete();
    return cell_id;
}

void ModelView::createPatchHighlightActor(int patch_id)
{
    if (patch_id < 0 || patch_id >= static_cast<int>(patches_.size()) || !current_mesh_) {
        return;
    }
    
    const auto& patch = patches_[patch_id];
    
    // 创建边界边的点
    vtkPoints* points = vtkPoints::New();
    vtkCellArray* lines = vtkCellArray::New();
    
    // 提取边界边并创建线段
    std::unordered_map<size_t, vtkIdType> vertex_to_point_id;
    
    for (size_t i = 0; i < patch.boundary_edges.size(); i += 2) {
        if (i + 1 >= patch.boundary_edges.size()) break;
        
        size_t v0_idx = patch.boundary_edges[i];
        size_t v1_idx = patch.boundary_edges[i + 1];
        
        if (v0_idx >= current_mesh_->vertices.size() || 
            v1_idx >= current_mesh_->vertices.size()) {
            continue;
        }
        
        const auto& v0 = current_mesh_->vertices[v0_idx];
        const auto& v1 = current_mesh_->vertices[v1_idx];
        
        vtkIdType id0, id1;
        
        // 查找或添加点
        auto it0 = vertex_to_point_id.find(v0_idx);
        if (it0 == vertex_to_point_id.end()) {
            id0 = points->InsertNextPoint(v0.x, v0.y, v0.z);
            vertex_to_point_id[v0_idx] = id0;
        } else {
            id0 = it0->second;
        }
        
        auto it1 = vertex_to_point_id.find(v1_idx);
        if (it1 == vertex_to_point_id.end()) {
            id1 = points->InsertNextPoint(v1.x, v1.y, v1.z);
            vertex_to_point_id[v1_idx] = id1;
        } else {
            id1 = it1->second;
        }
        
        // 创建线段
        vtkLine* line = vtkLine::New();
        line->GetPointIds()->SetId(0, id0);
        line->GetPointIds()->SetId(1, id1);
        lines->InsertNextCell(line);
        line->Delete();
    }
    
    if (points->GetNumberOfPoints() == 0) {
        points->Delete();
        lines->Delete();
        return;
    }
    
    // 创建PolyData
    if (patch_highlight_polydata_) {
        patch_highlight_polydata_->Delete();
    }
    patch_highlight_polydata_ = vtkPolyData::New();
    patch_highlight_polydata_->SetPoints(points);
    patch_highlight_polydata_->SetLines(lines);
    
    points->Delete();
    lines->Delete();
    
    // 创建Mapper和Actor
    if (patch_highlight_mapper_) {
        patch_highlight_mapper_->Delete();
    }
    patch_highlight_mapper_ = vtkPolyDataMapper::New();
    patch_highlight_mapper_->SetInputData(patch_highlight_polydata_);
    
    if (patch_highlight_actor_) {
        renderer_->RemoveActor(patch_highlight_actor_);
        patch_highlight_actor_->Delete();
    }
    patch_highlight_actor_ = vtkActor::New();
    patch_highlight_actor_->SetMapper(patch_highlight_mapper_);
    patch_highlight_actor_->GetProperty()->SetColor(1.0, 0.0, 0.0);  // 红色高亮
    patch_highlight_actor_->GetProperty()->SetLineWidth(3.0);
    
    renderer_->AddActor(patch_highlight_actor_);
}

void ModelView::removePatchHighlightActor()
{
    if (patch_highlight_actor_) {
        renderer_->RemoveActor(patch_highlight_actor_);
        patch_highlight_actor_->Delete();
        patch_highlight_actor_ = nullptr;
    }
    if (patch_highlight_mapper_) {
        patch_highlight_mapper_->Delete();
        patch_highlight_mapper_ = nullptr;
    }
    if (patch_highlight_polydata_) {
        patch_highlight_polydata_->Delete();
        patch_highlight_polydata_ = nullptr;
    }
}

void ModelView::applyTextureToPatch(int patch_id, const std::string& svg_filepath,
                                    const std::vector<nbcam::UVCoord>& uv_coords,
                                    int texture_width, int texture_height,
                                    double opacity, double ambient, 
                                    double diffuse, double specular,
                                    double scale_x, double scale_y,
                                    double translate_x, double translate_y,
                                    double rotation_deg)
{
    if (!current_mesh_ || patch_id < 0 || patch_id >= static_cast<int>(patches_.size())) {
        spdlog::error("无法应用纹理：无效的patch ID或mesh");
        return;
    }
    if (texture_width <= 0 || texture_height <= 0 || texture_width > 8192 || texture_height > 8192) {
        spdlog::error("无法应用纹理：纹理分辨率无效 {}x{}", texture_width, texture_height);
        return;
    }
    
    const auto& patch = patches_[patch_id];
    
    // 移除之前的纹理（如果当前显示的是同一个patch，则移除）
    removePatchTexture(patch_id);
    
    try {
        // 1. 加载并渲染SVG为图像
        nbcam::NanosvgWrapper svg_parser;
        if (!svg_parser.loadFromFile(svg_filepath)) {
            spdlog::error("SVG文件加载失败: {}", svg_filepath);
            return;
        }
        
        // 获取SVG映射边界：优先使用画布边界，保证与路径规划映射一致。
        double svg_x_min = 0.0, svg_y_min = 0.0, svg_x_max = 0.0, svg_y_max = 0.0;
        svg_parser.getCanvasBounds(svg_x_min, svg_y_min, svg_x_max, svg_y_max);
        double svg_width = svg_x_max - svg_x_min;
        double svg_height = svg_y_max - svg_y_min;
        if (!(svg_width > 1e-9) || !(svg_height > 1e-9)) {
            svg_parser.getBounds(svg_x_min, svg_y_min, svg_x_max, svg_y_max);
            svg_width = svg_x_max - svg_x_min;
            svg_height = svg_y_max - svg_y_min;
        }
        
        if (svg_width <= 0 || svg_height <= 0) {
            spdlog::error("SVG边界无效");
            return;
        }
        
        // 计算patch的UV边界（与路径规划保持一致：使用稳健分位范围抑制异常UV尖点）
        double u_min = std::numeric_limits<double>::max();
        double u_max = std::numeric_limits<double>::lowest();
        double v_min = std::numeric_limits<double>::max();
        double v_max = std::numeric_limits<double>::lowest();
        std::vector<double> u_samples;
        std::vector<double> v_samples;
        
        const auto collectVertex = [&](size_t v_idx) {
            if (v_idx >= uv_coords.size()) {
                return;
            }
            const auto& uv = uv_coords[v_idx];
            if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) {
                return;
            }
            u_min = std::min(u_min, uv.u);
            u_max = std::max(u_max, uv.u);
            v_min = std::min(v_min, uv.v);
            v_max = std::max(v_max, uv.v);
            u_samples.push_back(uv.u);
            v_samples.push_back(uv.v);
        };

        for (size_t i = 0; i + 1 < patch.boundary_edges.size(); i += 2) {
            collectVertex(patch.boundary_edges[i]);
            collectVertex(patch.boundary_edges[i + 1]);
        }

        for (size_t tri_idx : patch.triangle_indices) {
            if (tri_idx >= current_mesh_->triangles.size()) {
                continue;
            }
            const auto& tri = current_mesh_->triangles[tri_idx];
            collectVertex(tri.v0);
            collectVertex(tri.v1);
            collectVertex(tri.v2);
        }
        
        if (u_max <= u_min || v_max <= v_min) {
            spdlog::error("Patch UV边界无效");
            return;
        }

        if (u_samples.size() >= 8 && v_samples.size() >= 8) {
            std::sort(u_samples.begin(), u_samples.end());
            std::sort(v_samples.begin(), v_samples.end());
            const auto pickPercentile = [](const std::vector<double>& values, double p) {
                const double pos = p * static_cast<double>(values.size() - 1);
                const size_t lo = static_cast<size_t>(std::floor(pos));
                const size_t hi = static_cast<size_t>(std::ceil(pos));
                if (lo == hi) {
                    return values[lo];
                }
                const double t = pos - static_cast<double>(lo);
                return values[lo] * (1.0 - t) + values[hi] * t;
            };

            const double raw_u_range = u_max - u_min;
            const double raw_v_range = v_max - v_min;
            const double robust_u_min = pickPercentile(u_samples, 0.05);
            const double robust_u_max = pickPercentile(u_samples, 0.95);
            const double robust_v_min = pickPercentile(v_samples, 0.05);
            const double robust_v_max = pickPercentile(v_samples, 0.95);
            const double robust_u_range = robust_u_max - robust_u_min;
            const double robust_v_range = robust_v_max - robust_v_min;

            if (robust_u_range > 1e-9 && raw_u_range > robust_u_range * 1.5) {
                u_min = robust_u_min;
                u_max = robust_u_max;
            }
            if (robust_v_range > 1e-9 && raw_v_range > robust_v_range * 1.5) {
                v_min = robust_v_min;
                v_max = robust_v_max;
            }
        }
        
        // 计算UV中心点（用于旋转和平移）
        double center_u = (u_min + u_max) * 0.5;
        double center_v = (v_min + v_max) * 0.5;
        UvAspectFitFrame fit_frame;
        if (!computeUvAspectFitFrame(u_min, u_max, v_min, v_max, svg_width / svg_height, fit_frame)) {
            spdlog::error("无法应用纹理：无法计算等比例映射框");
            return;
        }
        
        // 旋转角度（转换为弧度）
        double rotation_rad = rotation_deg * M_PI / 180.0;
        double cos_r = std::cos(rotation_rad);
        double sin_r = std::sin(rotation_rad);
        
        // 2. 渲染SVG为图像（使用指定的纹理分辨率）
        const auto raster_size = computeAspectFittedRasterSize(texture_width, texture_height, svg_width / svg_height);
        const int raster_w = raster_size.first;
        const int raster_h = raster_size.second;

        QImage svg_image;
        if (!loadSvgImageCached(svg_filepath, raster_w, raster_h, svg_image,
                                SvgRasterColorMode::Grayscale) || svg_image.isNull()) {
            spdlog::error("SVG渲染失败");
            return;
        }
        
        // 3. 创建VTK图像数据
        vtkImageData* image_data = vtkImageData::New();
        image_data->SetDimensions(raster_w, raster_h, 1);
        image_data->AllocateScalars(VTK_UNSIGNED_CHAR, 4);  // RGBA
        
        // 复制图像数据（注意Y轴翻转，因为VTK和VTK纹理坐标系差异）
        unsigned char* vtk_data = static_cast<unsigned char*>(image_data->GetScalarPointer());
        for (int y = 0; y < raster_h; ++y) {
            int src_y = raster_h - 1 - y;  // Y轴翻转
            for (int x = 0; x < raster_w; ++x) {
                const QRgb pixel = svg_image.pixel(x, src_y);
                int dst_idx = (y * raster_w + x) * 4;
                vtk_data[dst_idx + 0] = qRed(pixel);
                vtk_data[dst_idx + 1] = qGreen(pixel);
                vtk_data[dst_idx + 2] = qBlue(pixel);
                vtk_data[dst_idx + 3] = qAlpha(pixel);
            }
        }
        
        // 4. 创建纹理
        patch_texture_ = vtkTexture::New();
        patch_texture_->SetInputData(image_data);
        patch_texture_->InterpolateOn();  // 启用纹理插值，使纹理更平滑
        patch_texture_->RepeatOff();  // 不重复纹理
        patch_texture_->EdgeClampOn();  // 边缘夹紧，避免纹理边缘问题
        patch_texture_->SetQualityTo32Bit();  // 使用32位质量
        image_data->Delete();
        
        // 5. 创建patch的子网格（只包含patch的三角形）
        vtkPoints* points = vtkPoints::New();
        vtkCellArray* triangles = vtkCellArray::New();
        vtkFloatArray* tcoords = vtkFloatArray::New();
        tcoords->SetNumberOfComponents(2);
        tcoords->SetName("TextureCoordinates");
        
        // 收集patch使用的所有顶点
        std::unordered_map<size_t, vtkIdType> vertex_map;
        vtkIdType point_id = 0;
        
        size_t valid_triangle_count = 0;
        for (size_t tri_idx : patch.triangle_indices) {
            if (tri_idx >= current_mesh_->triangles.size()) continue;
            const auto& tri = current_mesh_->triangles[tri_idx];

            vtkIdType ids[3] = {-1, -1, -1};
            bool triangle_valid = true;
            for (int i = 0; i < 3; ++i) {
                size_t v_idx = (i == 0) ? tri.v0 : ((i == 1) ? tri.v1 : tri.v2);

                if (v_idx >= current_mesh_->vertices.size()) {
                    triangle_valid = false;
                    break;
                }

                auto it = vertex_map.find(v_idx);
                if (it == vertex_map.end()) {
                    const auto& vertex = current_mesh_->vertices[v_idx];
                    points->InsertNextPoint(vertex.x, vertex.y, vertex.z);

                    // 设置纹理坐标（基于UV坐标，应用变换后映射到[0,1]范围）
                    if (v_idx < uv_coords.size()) {
                        const auto& uv = uv_coords[v_idx];
                        if (std::isfinite(uv.u) && std::isfinite(uv.v)) {
                            // 1. 相对于中心点的坐标
                            double u_rel = uv.u - center_u;
                            double v_rel = uv.v - center_v;

                            // 2. 应用缩放
                            double u_scaled = u_rel * scale_x;
                            double v_scaled = v_rel * scale_y;

                            // 3. 应用旋转（围绕中心点）
                            double u_rot = u_scaled * cos_r - v_scaled * sin_r;
                            double v_rot = u_scaled * sin_r + v_scaled * cos_r;

                            // 4. 应用平移
                            double u_transformed = u_rot + translate_x + center_u;
                            double v_transformed = v_rot + translate_y + center_v;

                            // 5. 归一化到等比例映射框内，然后映射到[0,1]
                            if (fit_frame.u_range > 1e-6 && fit_frame.v_range > 1e-6 &&
                                std::isfinite(u_transformed) && std::isfinite(v_transformed)) {
                                double u_norm = (u_transformed - fit_frame.u_min) / fit_frame.u_range;
                                double v_norm = (v_transformed - fit_frame.v_min) / fit_frame.v_range;

                                // 确保纹理坐标在[0,1]范围内（允许稍微超出以支持平移）
                                u_norm = std::max(-0.5, std::min(1.5, u_norm));
                                v_norm = std::max(-0.5, std::min(1.5, v_norm));

                                // V轴翻转（SVG坐标系Y向下，VTK纹理坐标系V向上）
                                tcoords->InsertNextTuple2(u_norm, 1.0 - v_norm);
                            } else {
                                tcoords->InsertNextTuple2(0.0, 0.0);
                            }
                        } else {
                            tcoords->InsertNextTuple2(0.0, 0.0);
                        }
                    } else {
                        tcoords->InsertNextTuple2(0.0, 0.0);
                    }

                    vertex_map[v_idx] = point_id;
                    ids[i] = point_id;
                    ++point_id;
                } else {
                    ids[i] = it->second;
                }
            }

            if (!triangle_valid || ids[0] < 0 || ids[1] < 0 || ids[2] < 0) {
                continue;
            }

            vtkTriangle* triangle = vtkTriangle::New();
            triangle->GetPointIds()->SetId(0, ids[0]);
            triangle->GetPointIds()->SetId(1, ids[1]);
            triangle->GetPointIds()->SetId(2, ids[2]);
            triangles->InsertNextCell(triangle);
            triangle->Delete();
            ++valid_triangle_count;
        }

        if (valid_triangle_count == 0) {
            spdlog::error("应用纹理失败：patch {} 没有可用三角形（可能是索引或UV异常）", patch_id);
            points->Delete();
            triangles->Delete();
            tcoords->Delete();
            removePatchTexture(-1);
            return;
        }
        
        // 6. 创建PolyData
        patch_texture_polydata_ = vtkPolyData::New();
        patch_texture_polydata_->SetPoints(points);
        patch_texture_polydata_->SetPolys(triangles);
        patch_texture_polydata_->GetPointData()->SetTCoords(tcoords);
        
        points->Delete();
        triangles->Delete();
        tcoords->Delete();
        
        // 7. 创建Mapper和Actor
        patch_texture_mapper_ = vtkPolyDataMapper::New();
        patch_texture_mapper_->SetInputData(patch_texture_polydata_);
        
        patch_texture_actor_ = vtkActor::New();
        patch_texture_actor_->SetMapper(patch_texture_mapper_);
        patch_texture_actor_->SetTexture(patch_texture_);
        
        // 设置渲染属性（使用用户指定的参数）
        vtkProperty* prop = patch_texture_actor_->GetProperty();
        prop->SetOpacity(opacity);  // 不透明度
        prop->SetAmbient(ambient);  // 环境光
        prop->SetDiffuse(diffuse);  // 漫反射
        prop->SetSpecular(specular);  // 镜面反射
        prop->SetInterpolationToFlat();  // 使用平面插值，纹理更清晰
        
        // 确保纹理actor在正确的渲染层（在mesh之上）
        
        // 临时降低mesh actor的不透明度，让纹理更明显
        if (mesh_actor_) {
            mesh_actor_->GetProperty()->SetOpacity(0.5);  // 降低mesh不透明度
        }
        
        // 添加到渲染器（确保在mesh actor之后添加，这样纹理会显示在mesh之上）
        renderer_->AddActor(patch_texture_actor_);
        textured_patch_id_ = patch_id;
        
        // 记录详细信息
        spdlog::info("SVG纹理已应用到patch {}: {} 个三角形", patch_id, patch.triangle_indices.size());
        spdlog::info("  UV范围: U=[{:.6f}, {:.6f}], V=[{:.6f}, {:.6f}]", u_min, u_max, v_min, v_max);
        spdlog::info("  等比框: U=[{:.6f}, {:.6f}], V=[{:.6f}, {:.6f}]",
                     fit_frame.u_min, fit_frame.u_max, fit_frame.v_min, fit_frame.v_max);
        spdlog::info("  纹理分辨率: {}x{}, 顶点数: {}", texture_width, texture_height, point_id);
        spdlog::info("  渲染参数: 不透明度={:.2f}, 环境光={:.2f}, 漫反射={:.2f}, 镜面反射={:.2f}", 
                     opacity, ambient, diffuse, specular);
        
        // 强制更新视图
        updateView();
        
    } catch (const std::exception& e) {
        spdlog::error("应用纹理异常: {}", e.what());
        removePatchTexture(-1);
    }
}

void ModelView::removePatchTexture(int patch_id)
{
    // 如果指定了patch_id，只有当当前显示的纹理是该patch时才移除
    if (patch_id >= 0 && textured_patch_id_ != patch_id) {
        return;  // 不是要移除的patch，直接返回
    }
    
    if (patch_texture_actor_) {
        renderer_->RemoveActor(patch_texture_actor_);
        patch_texture_actor_->Delete();
        patch_texture_actor_ = nullptr;
    }
    if (patch_texture_mapper_) {
        patch_texture_mapper_->Delete();
        patch_texture_mapper_ = nullptr;
    }
    if (patch_texture_polydata_) {
        patch_texture_polydata_->Delete();
        patch_texture_polydata_ = nullptr;
    }
    if (patch_texture_) {
        patch_texture_->Delete();
        patch_texture_ = nullptr;
    }
    textured_patch_id_ = -1;
    
    // 恢复mesh actor的不透明度
    if (mesh_actor_) {
        mesh_actor_->GetProperty()->SetOpacity(0.8);
    }
    
    updateView();
}

void ModelView::clearPatchData()
{
    spdlog::info("清理面片数据：清除选中的patch、高亮、纹理和参数化信息");
    
    // 清除选中的patch
    clearPatchSelection();
    
    // 清除纹理
    removePatchTexture(-1);
    
    // 清除高亮
    highlighted_patch_id_ = -1;
    removePatchHighlightActor();
    
    // 清除顶点高亮
    clearVertexHighlight();
    
    // 恢复mesh actor的不透明度
    if (mesh_actor_) {
        mesh_actor_->GetProperty()->SetOpacity(0.8);
    }
    
    updateView();
}

void ModelView::mousePressEvent(QMouseEvent* event)
{
    // 左键单击不再选择patch，只处理其他交互
    QVTKOpenGLNativeWidget::mousePressEvent(event);
}

void ModelView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !patches_.empty() && current_mesh_) {
        int tri_idx = pickTriangle(event->x(), event->y());
        if (tri_idx >= 0 && tri_idx < static_cast<int>(current_mesh_->triangles.size())) {
            int patch_id = patch_clusterer_->getTrianglePatchId(tri_idx);
            if (patch_id >= 0) {
                // 双击选择patch
                if (patch_id == selected_patch_id_) {
                    // 双击已选中的patch，取消选择
                    clearPatchSelection();
                } else {
                    // 选择新的patch
                    selectPatch(patch_id);
                }
            } else {
                // 双击的不是任何patch，清除选择
                clearPatchSelection();
            }
        } else {
            // 双击的不是模型，清除选择
            clearPatchSelection();
        }
    }
    
    QVTKOpenGLNativeWidget::mouseDoubleClickEvent(event);
}

void ModelView::mouseMoveEvent(QMouseEvent* event)
{
    if (!patches_.empty() && current_mesh_) {
        // 如果有选中的patch，不响应其他patch的悬停交互
        if (selected_patch_id_ >= 0) {
            // 保持选中patch的高亮，不响应其他patch
            if (highlighted_patch_id_ != selected_patch_id_) {
                highlighted_patch_id_ = selected_patch_id_;
                removePatchHighlightActor();
                createPatchHighlightActor(selected_patch_id_);
                updateView();
            }
        } else {
            // 没有选中的patch时，正常响应悬停
            int tri_idx = pickTriangle(event->x(), event->y());
            if (tri_idx >= 0 && tri_idx < static_cast<int>(current_mesh_->triangles.size())) {
                int patch_id = patch_clusterer_->getTrianglePatchId(tri_idx);
                if (patch_id >= 0) {
                    highlightPatch(patch_id);
                } else {
                    // 鼠标不在任何patch上，清除高亮
                    if (highlighted_patch_id_ >= 0) {
                        highlighted_patch_id_ = -1;
                        removePatchHighlightActor();
                        updateView();
                    }
                }
            } else {
                // 鼠标不在任何patch上，清除高亮
                if (highlighted_patch_id_ >= 0) {
                    highlighted_patch_id_ = -1;
                    removePatchHighlightActor();
                    updateView();
                }
            }
        }
    }
    
    QVTKOpenGLNativeWidget::mouseMoveEvent(event);
}

void ModelView::highlightVertex(size_t vertex_index)
{
    if (!current_mesh_ || vertex_index >= current_mesh_->vertices.size()) {
        clearVertexHighlight();
        return;
    }
    
    highlighted_vertex_index_ = vertex_index;
    
    // 移除旧的高亮
    clearVertexHighlight();
    
    // 创建顶点高亮（使用球体）
    const auto& vertex = current_mesh_->vertices[vertex_index];
    
    // 创建球体源（用于显示顶点）
    vtkSphereSource* sphere = vtkSphereSource::New();
    // 根据模型大小计算合适的球体半径
    double bbox[6];
    if (mesh_polydata_) {
        mesh_polydata_->GetBounds(bbox);
        double model_size = std::sqrt(
            (bbox[1] - bbox[0]) * (bbox[1] - bbox[0]) +
            (bbox[3] - bbox[2]) * (bbox[3] - bbox[2]) +
            (bbox[5] - bbox[4]) * (bbox[5] - bbox[4])
        );
        sphere->SetRadius(model_size * 0.01);  // 模型大小的1%
    } else {
        sphere->SetRadius(0.01);  // 默认半径
    }
    sphere->SetCenter(vertex.x, vertex.y, vertex.z);
    sphere->SetThetaResolution(16);
    sphere->SetPhiResolution(16);
    sphere->Update();
    
    // 创建PolyData
    vertex_highlight_polydata_ = vtkPolyData::New();
    vertex_highlight_polydata_->ShallowCopy(sphere->GetOutput());
    
    sphere->Delete();
    
    // 创建Mapper和Actor
    vertex_highlight_mapper_ = vtkPolyDataMapper::New();
    vertex_highlight_mapper_->SetInputData(vertex_highlight_polydata_);
    
    vertex_highlight_actor_ = vtkActor::New();
    vertex_highlight_actor_->SetMapper(vertex_highlight_mapper_);
    vertex_highlight_actor_->GetProperty()->SetColor(1.0, 0.0, 0.0);  // 红色
    vertex_highlight_actor_->GetProperty()->SetPointSize(10.0);
    
    renderer_->AddActor(vertex_highlight_actor_);
    updateView();
}

void ModelView::clearVertexHighlight()
{
    if (vertex_highlight_actor_) {
        renderer_->RemoveActor(vertex_highlight_actor_);
        vertex_highlight_actor_->Delete();
        vertex_highlight_actor_ = nullptr;
    }
    if (vertex_highlight_mapper_) {
        vertex_highlight_mapper_->Delete();
        vertex_highlight_mapper_ = nullptr;
    }
    if (vertex_highlight_polydata_) {
        vertex_highlight_polydata_->Delete();
        vertex_highlight_polydata_ = nullptr;
    }
    highlighted_vertex_index_ = static_cast<size_t>(-1);
    updateView();
}
