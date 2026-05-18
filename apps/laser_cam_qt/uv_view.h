#ifndef UV_VIEW_H
#define UV_VIEW_H

#include <QWidget>
#include <QPainter>
#include <QWheelEvent>
#include <QImage>
#include <QSize>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nbcam {
struct UVCoord;
struct UVPathPoint;
struct Triangle;
class TriangleMesh;
class LaserJob;
}

class UVView : public QWidget
{
    Q_OBJECT

public:
    explicit UVView(QWidget *parent = nullptr);
    ~UVView() override;

    // 设置UV坐标
    void setUVCoords(const std::vector<nbcam::UVCoord>& coords);
    
    // 设置UV路径
    void setUVPath(const std::vector<nbcam::UVPathPoint>& path);
    
    // 设置三角形信息（用于线框显示）
    void setTriangles(const std::vector<nbcam::Triangle>& triangles);
    
    // 设置纹理信息（用于在UV视图中绘制纹理）
    void setTexture(int patch_id, const std::string& svg_filepath, 
                    double u_min, double u_max, double v_min, double v_max,
                    double scale_x = 1.0, double scale_y = 1.0,
                    double translate_x = 0.0, double translate_y = 0.0,
                    double rotation_deg = 0.0);
    
    // 移除纹理
    void removeTexture(int patch_id);
    
    // 清除显示
    void clear();
    
    // 线框模式
    void setWireframeMode(bool enabled);
    
    // UV校验模式
    void setValidationMode(bool enabled);
    bool getValidationMode() const { return validation_mode_; }
    
    // 设置3D网格信息（用于查找对应的3D顶点）
    void setMeshInfo(nbcam::TriangleMesh* mesh, const std::vector<nbcam::Triangle>& triangles);
    void setJob(nbcam::LaserJob* job);
    void setHighlightedPathSegments(const std::vector<int>& segment_ids);
    void addHighlightedPathSegment(int segment_id);

signals:
    // 当点击UV点时发出信号，传递顶点索引和UV坐标
    void uvVertexClicked(size_t vertex_index, double u, double v);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void updateTransform();
    QPointF uvToScreen(double u, double v) const;
    void invalidateHighlightCache();
    void drawHighlightedSegment(QPainter& painter, int segment_id);
    void rebuildHighlightCache();
    
    std::vector<nbcam::UVCoord> uv_coords_;
    std::vector<nbcam::UVPathPoint> uv_path_;
    std::vector<nbcam::Triangle> triangles_;  // 三角形信息（用于线框显示）
    
    // 纹理信息
    struct TextureData {
        std::string svg_filepath;
        double u_min, u_max, v_min, v_max;
        double scale_x, scale_y;
        double translate_x, translate_y;
        double rotation_deg;
        double content_aspect_wh = 1.0;
        QImage image;  // 缓存的纹理图像（比QPixmap更稳定）
    };
    std::map<int, TextureData> textures_;  // patch_id -> TextureData
    
    // 变换参数
    double u_min_, u_max_, v_min_, v_max_;
    double scale_x_, scale_y_;
    double offset_x_, offset_y_;
    bool has_bounds_;
    bool wireframe_mode_;
    
    // UV校验模式
    bool validation_mode_;
    size_t highlighted_vertex_index_;  // 当前高亮的顶点索引
    QPointF highlighted_screen_pos_;  // 高亮点的屏幕位置（用于绘制标签）
    
    // 网格信息（用于查找对应的3D顶点）
    nbcam::TriangleMesh* mesh_info_;
    std::vector<nbcam::Triangle> triangles_info_;
    nbcam::LaserJob* current_job_;
    std::unordered_map<int, size_t> job_segment_indices_;
    std::unordered_set<int> highlighted_segment_ids_;
    QImage highlight_overlay_cache_;
    QSize highlight_overlay_cache_size_;
    std::unordered_set<int> cached_highlighted_segment_ids_;
    
    // 辅助方法
    QPointF screenToUV(int x, int y) const;  // 屏幕坐标转UV坐标
    size_t findNearestVertex(double u, double v) const;  // 找到最近的UV顶点
};

#endif // UV_VIEW_H
