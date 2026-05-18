#include "texture_details_dialog.h"
#include "texture_list_widget.h"
#include <QFileInfo>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <cmath>

TextureDetailsDialog::TextureDetailsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("纹理详细信息");
    setModal(true);
    resize(500, 400);
    
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    
    // 基本信息组
    QGroupBox* basic_group = new QGroupBox("基本信息", this);
    QFormLayout* basic_layout = new QFormLayout(basic_group);
    
    patch_id_label_ = new QLabel(this);
    basic_layout->addRow("Patch ID:", patch_id_label_);
    
    svg_file_label_ = new QLabel(this);
    svg_file_label_->setWordWrap(true);
    basic_layout->addRow("SVG文件:", svg_file_label_);
    
    resolution_label_ = new QLabel(this);
    basic_layout->addRow("分辨率:", resolution_label_);
    
    main_layout->addWidget(basic_group);
    
    // 渲染参数组
    QGroupBox* render_group = new QGroupBox("渲染参数", this);
    QFormLayout* render_layout = new QFormLayout(render_group);
    
    opacity_label_ = new QLabel(this);
    render_layout->addRow("不透明度:", opacity_label_);
    
    ambient_label_ = new QLabel(this);
    render_layout->addRow("环境光:", ambient_label_);
    
    diffuse_label_ = new QLabel(this);
    render_layout->addRow("漫反射:", diffuse_label_);
    
    specular_label_ = new QLabel(this);
    render_layout->addRow("镜面反射:", specular_label_);
    
    main_layout->addWidget(render_group);
    
    // 变换参数组
    QGroupBox* transform_group = new QGroupBox("变换参数", this);
    QFormLayout* transform_layout = new QFormLayout(transform_group);
    
    scale_label_ = new QLabel(this);
    transform_layout->addRow("缩放:", scale_label_);
    
    translate_label_ = new QLabel(this);
    transform_layout->addRow("平移:", translate_label_);
    
    rotation_label_ = new QLabel(this);
    transform_layout->addRow("旋转:", rotation_label_);

    prewarp_label_ = new QLabel(this);
    transform_layout->addRow("逆向预扭曲:", prewarp_label_);

    contour_info_label_ = new QLabel(this);
    contour_info_label_->setWordWrap(true);
    transform_layout->addRow("轮廓信息:", contour_info_label_);
    
    main_layout->addWidget(transform_group);
    
    // 按钮
    QDialogButtonBox* button_box = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    main_layout->addWidget(button_box);
}

TextureDetailsDialog::~TextureDetailsDialog() = default;

void TextureDetailsDialog::setTextureInfo(int patch_id, const TextureInfo& info)
{
    patch_id_label_->setText(QString::number(patch_id));
    
    QFileInfo file_info(QString::fromStdString(info.svg_filepath));
    svg_file_label_->setText(file_info.fileName() + "\n" + file_info.absoluteFilePath());
    
    resolution_label_->setText(QString("%1 x %2 像素").arg(info.texture_width).arg(info.texture_height));
    
    opacity_label_->setText(QString::number(info.opacity, 'f', 2));
    ambient_label_->setText(QString::number(info.ambient, 'f', 2));
    diffuse_label_->setText(QString::number(info.diffuse, 'f', 2));
    specular_label_->setText(QString::number(info.specular, 'f', 2));
    
    const double display_scale_u = (std::abs(info.scale_x) > 1e-9) ? (1.0 / info.scale_x) : 0.0;
    const double display_scale_v = (std::abs(info.scale_y) > 1e-9) ? (1.0 / info.scale_y) : 0.0;
    scale_label_->setText(QString("U: %1, V: %2").arg(display_scale_u, 0, 'f', 2).arg(display_scale_v, 0, 'f', 2));
    translate_label_->setText(QString("U: %1, V: %2").arg(info.translate_x, 0, 'f', 2).arg(info.translate_y, 0, 'f', 2));
    rotation_label_->setText(QString("%1°").arg(info.rotation_deg, 0, 'f', 1));
    prewarp_label_->setText(QString("%1, 强度=%2")
                                .arg(info.enable_inverse_stretch_prewarp ? "启用" : "关闭")
                                .arg(info.inverse_stretch_prewarp_strength, 0, 'f', 2));
    contour_info_label_->setText(
        QString("轮廓回路=%1, 轮廓段(MARK)=%2, 跳转段(JUMP)=%3, 轮廓点数=%4, 路径保存=%5")
            .arg(info.contour_loop_count)
            .arg(info.contour_mark_segment_count)
            .arg(info.contour_jump_segment_count)
            .arg(info.contour_mark_point_count)
            .arg(info.has_saved_path ? "是" : "否"));
}
