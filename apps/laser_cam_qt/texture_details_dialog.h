#ifndef TEXTURE_DETAILS_DIALOG_H
#define TEXTURE_DETAILS_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>

// 前向声明TextureInfo（定义在texture_list_widget.h中）
struct TextureInfo;

class TextureDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TextureDetailsDialog(QWidget *parent = nullptr);
    ~TextureDetailsDialog() override;

    void setTextureInfo(int patch_id, const TextureInfo& info);

private:
    QLabel* patch_id_label_;
    QLabel* svg_file_label_;
    QLabel* resolution_label_;
    QLabel* opacity_label_;
    QLabel* ambient_label_;
    QLabel* diffuse_label_;
    QLabel* specular_label_;
    QLabel* scale_label_;
    QLabel* translate_label_;
    QLabel* rotation_label_;
    QLabel* prewarp_label_;
    QLabel* contour_info_label_;
};

#endif // TEXTURE_DETAILS_DIALOG_H
