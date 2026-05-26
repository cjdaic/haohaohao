#ifndef TEXTURE_LIST_WIDGET_H
#define TEXTURE_LIST_WIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QContextMenuEvent>
#include <QAction>
#include <map>

struct TextureInfo {
    int patch_id;
    std::string svg_filepath;
    int texture_width;
    int texture_height;
    double opacity;
    double ambient;
    double diffuse;
    double specular;
    double scale_x;
    double scale_y;
    double translate_x;
    double translate_y;
    double rotation_deg;
    bool enable_inverse_stretch_prewarp;
    double inverse_stretch_prewarp_strength;
    bool grayscale_converted;
    bool grayscale_power_reserved;
    double grayscale_power_min_w;
    double grayscale_power_max_w;
    double grayscale_gamma;
    bool has_saved_path;
    int contour_loop_count;
    int contour_mark_segment_count;
    int contour_jump_segment_count;
    int contour_mark_point_count;
    
    TextureInfo() 
        : patch_id(-1)
        , texture_width(1024)
        , texture_height(1024)
        , opacity(1.0)
        , ambient(0.3)
        , diffuse(0.7)
        , specular(0.0)
        , scale_x(1.0)
        , scale_y(1.0)
        , translate_x(0.0)
        , translate_y(0.0)
        , rotation_deg(0.0)
        , enable_inverse_stretch_prewarp(true)
        , inverse_stretch_prewarp_strength(1.2)
        , grayscale_converted(true)
        , grayscale_power_reserved(true)
        , grayscale_power_min_w(0.0)
        , grayscale_power_max_w(100.0)
        , grayscale_gamma(1.0)
        , has_saved_path(false)
        , contour_loop_count(0)
        , contour_mark_segment_count(0)
        , contour_jump_segment_count(0)
        , contour_mark_point_count(0)
    {}
};

class TextureListWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit TextureListWidget(QWidget *parent = nullptr);
    ~TextureListWidget() override;

    // 添加纹理信息
    void addTexture(int patch_id, const TextureInfo& info);
    
    // 移除纹理信息
    void removeTexture(int patch_id);
    
    // 更新纹理信息
    void updateTexture(int patch_id, const TextureInfo& info);
    
    // 获取纹理信息
    TextureInfo getTexture(int patch_id) const;
    
    // 获取所有纹理信息
    std::map<int, TextureInfo> getAllTextures() const { return textures_; }
    
    // 清除所有纹理
    void clearTextures();
    void setSelectedPatchId(int patch_id);

signals:
    void textureSelected(int patch_id);
    void addTextureRequested();
    void removeTextureRequested(int patch_id);
    void removePathRequested(int patch_id);
    void editTextureRequested(int patch_id);
    void showDetailsRequested(int patch_id);  // 显示详细信息

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onAddTexture();
    void onRemoveTexture();
    void onRemovePath();
    void onEditTexture();
    void onShowDetails();

private:
    void updateList();
    int getSelectedPatchId() const;
    
    std::map<int, TextureInfo> textures_;  // patch_id -> TextureInfo
    QMenu* context_menu_;
    QAction* add_action_;
    QAction* remove_action_;
    QAction* remove_path_action_;
    QAction* edit_action_;
    QAction* details_action_;  // 详细信息动作
    int selected_patch_id_;
};

#endif // TEXTURE_LIST_WIDGET_H
