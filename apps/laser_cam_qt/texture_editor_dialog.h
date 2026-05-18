#ifndef TEXTURE_EDITOR_DIALOG_H
#define TEXTURE_EDITOR_DIALOG_H

#include <QDialog>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>

class TextureEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TextureEditorDialog(QWidget *parent = nullptr);
    ~TextureEditorDialog() override;

    // 获取变换参数
    double getScaleX() const;
    double getScaleY() const;
    double getTranslateX() const { return translate_x_; }
    double getTranslateY() const { return translate_y_; }
    double getRotation() const { return rotation_; }  // 角度（度）
    bool getUniformScale() const { return uniform_scale_; }
    bool getInverseStretchPrewarpEnabled() const { return enable_inverse_stretch_prewarp_; }
    double getInverseStretchPrewarpStrength() const { return inverse_stretch_prewarp_strength_; }

    // 设置变换参数
    void setScaleX(double value);
    void setScaleY(double value);
    void setTranslateX(double value);
    void setTranslateY(double value);
    void setRotation(double value);
    void setUniformScale(bool enabled);
    void setInverseStretchPrewarpEnabled(bool enabled);
    void setInverseStretchPrewarpStrength(double value);

signals:
    void transformChanged();  // 变换参数改变时发出信号

private slots:
    void onScaleXChanged(int value);
    void onScaleYChanged(int value);
    void onTranslateXChanged(double value);
    void onTranslateYChanged(double value);
    void onTranslateXSliderChanged(int value);
    void onTranslateYSliderChanged(int value);
    void onRotationChanged(int value);
    void onUniformScaleToggled(bool enabled);
    void onInverseStretchPrewarpToggled(bool enabled);
    void onInverseStretchPrewarpStrengthChanged(int value);
    void onResetClicked();

private:
    double uiScaleToInternal(double ui_scale) const;
    double internalScaleToUi(double internal_scale) const;
    void updateScaleYFromX();  // 当uniform scale启用时，从X更新Y
    
    // 缩放控件
    QSlider* scale_x_slider_;
    QDoubleSpinBox* scale_x_spin_;
    QSlider* scale_y_slider_;
    QDoubleSpinBox* scale_y_spin_;
    QCheckBox* uniform_scale_check_;
    
    // 平移控件
    QSlider* translate_x_slider_;
    QDoubleSpinBox* translate_x_spin_;
    QSlider* translate_y_slider_;
    QDoubleSpinBox* translate_y_spin_;
    
    // 旋转控件
    QSlider* rotation_slider_;
    QDoubleSpinBox* rotation_spin_;

    // 预扭曲控件
    QCheckBox* inverse_stretch_prewarp_check_;
    QSlider* inverse_stretch_prewarp_slider_;
    QDoubleSpinBox* inverse_stretch_prewarp_spin_;
    
    // 当前值
    double scale_x_;
    double scale_y_;
    double translate_x_;
    double translate_y_;
    double rotation_;
    bool uniform_scale_;
    bool enable_inverse_stretch_prewarp_;
    double inverse_stretch_prewarp_strength_;
};

#endif // TEXTURE_EDITOR_DIALOG_H
