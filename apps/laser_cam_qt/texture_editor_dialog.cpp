#include "texture_editor_dialog.h"
#include <QDialogButtonBox>
#include <QGridLayout>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr int kTranslateSliderScale = 1000;   // 0.001 UV精度
constexpr int kTranslateSliderMin = -10000;   // -10.0 UV
constexpr int kTranslateSliderMax = 10000;    //  10.0 UV
}  // namespace

TextureEditorDialog::TextureEditorDialog(QWidget *parent)
    : QDialog(parent)
    , scale_x_(1.0)
    , scale_y_(1.0)
    , translate_x_(0.0)
    , translate_y_(0.0)
    , rotation_(0.0)
    , uniform_scale_(true)
    , enable_inverse_stretch_prewarp_(true)
    , inverse_stretch_prewarp_strength_(1.2)
{
    setWindowTitle("纹理编辑器");
    setModal(false);  // 非模态对话框，允许实时预览
    resize(540, 520);
    
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    
    // 缩放组
    QGroupBox* scale_group = new QGroupBox("缩放", this);
    QFormLayout* scale_layout = new QFormLayout(scale_group);
    
    uniform_scale_check_ = new QCheckBox("统一缩放", this);
    uniform_scale_check_->setChecked(true);
    connect(uniform_scale_check_, &QCheckBox::toggled, this, &TextureEditorDialog::onUniformScaleToggled);
    scale_layout->addRow(uniform_scale_check_);
    
    // X缩放
    QHBoxLayout* scale_x_layout = new QHBoxLayout();
    scale_x_slider_ = new QSlider(Qt::Horizontal, this);
    scale_x_slider_->setRange(10, 500);  // 0.1x 到 5.0x
    scale_x_slider_->setValue(100);  // 1.0x
    scale_x_spin_ = new QDoubleSpinBox(this);
    scale_x_spin_->setRange(0.1, 5.0);
    scale_x_spin_->setSingleStep(0.1);
    scale_x_spin_->setDecimals(2);
    scale_x_spin_->setValue(1.0);
    scale_x_spin_->setSuffix("x");
    scale_x_layout->addWidget(scale_x_slider_);
    scale_x_layout->addWidget(scale_x_spin_);
    connect(scale_x_slider_, &QSlider::valueChanged, this, &TextureEditorDialog::onScaleXChanged);
    connect(scale_x_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            [this](double value) { 
                scale_x_slider_->setValue(static_cast<int>(value * 100)); 
            });
    scale_layout->addRow("U缩放:", scale_x_layout);
    
    // Y缩放
    QHBoxLayout* scale_y_layout = new QHBoxLayout();
    scale_y_slider_ = new QSlider(Qt::Horizontal, this);
    scale_y_slider_->setRange(10, 500);
    scale_y_slider_->setValue(100);
    scale_y_spin_ = new QDoubleSpinBox(this);
    scale_y_spin_->setRange(0.1, 5.0);
    scale_y_spin_->setSingleStep(0.1);
    scale_y_spin_->setDecimals(2);
    scale_y_spin_->setValue(1.0);
    scale_y_spin_->setSuffix("x");
    scale_y_layout->addWidget(scale_y_slider_);
    scale_y_layout->addWidget(scale_y_spin_);
    connect(scale_y_slider_, &QSlider::valueChanged, this, &TextureEditorDialog::onScaleYChanged);
    connect(scale_y_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            [this](double value) { 
                scale_y_slider_->setValue(static_cast<int>(value * 100)); 
            });
    scale_layout->addRow("V缩放:", scale_y_layout);
    
    main_layout->addWidget(scale_group);
    
    // 平移组
    QGroupBox* translate_group = new QGroupBox("平移（UV）", this);
    QFormLayout* translate_layout = new QFormLayout(translate_group);

    QHBoxLayout* translate_u_layout = new QHBoxLayout();
    translate_x_slider_ = new QSlider(Qt::Horizontal, this);
    translate_x_slider_->setRange(kTranslateSliderMin, kTranslateSliderMax);
    translate_x_slider_->setValue(0);
    translate_x_spin_ = new QDoubleSpinBox(this);
    translate_x_spin_->setRange(-10.0, 10.0);
    translate_x_spin_->setSingleStep(0.01);
    translate_x_spin_->setDecimals(2);
    translate_x_spin_->setValue(0.0);
    translate_x_spin_->setSuffix(" UV");
    translate_u_layout->addWidget(translate_x_slider_);
    translate_u_layout->addWidget(translate_x_spin_);
    connect(translate_x_slider_, &QSlider::valueChanged,
            this, &TextureEditorDialog::onTranslateXSliderChanged);
    connect(translate_x_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TextureEditorDialog::onTranslateXChanged);
    translate_layout->addRow("U平移:", translate_u_layout);

    QHBoxLayout* translate_v_layout = new QHBoxLayout();
    translate_y_slider_ = new QSlider(Qt::Horizontal, this);
    translate_y_slider_->setRange(kTranslateSliderMin, kTranslateSliderMax);
    translate_y_slider_->setValue(0);
    translate_y_spin_ = new QDoubleSpinBox(this);
    translate_y_spin_->setRange(-10.0, 10.0);
    translate_y_spin_->setSingleStep(0.01);
    translate_y_spin_->setDecimals(2);
    translate_y_spin_->setValue(0.0);
    translate_y_spin_->setSuffix(" UV");
    translate_v_layout->addWidget(translate_y_slider_);
    translate_v_layout->addWidget(translate_y_spin_);
    connect(translate_y_slider_, &QSlider::valueChanged,
            this, &TextureEditorDialog::onTranslateYSliderChanged);
    connect(translate_y_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TextureEditorDialog::onTranslateYChanged);
    translate_layout->addRow("V平移:", translate_v_layout);

    main_layout->addWidget(translate_group);
    
    // 旋转组
    QGroupBox* rotation_group = new QGroupBox("旋转", this);
    QFormLayout* rotation_layout = new QFormLayout(rotation_group);
    
    QHBoxLayout* rotation_h_layout = new QHBoxLayout();
    rotation_slider_ = new QSlider(Qt::Horizontal, this);
    rotation_slider_->setRange(-1800, 1800);  // -180度到180度，精度0.1度
    rotation_slider_->setValue(0);
    rotation_spin_ = new QDoubleSpinBox(this);
    rotation_spin_->setRange(-180.0, 180.0);
    rotation_spin_->setSingleStep(1.0);
    rotation_spin_->setDecimals(1);
    rotation_spin_->setValue(0.0);
    rotation_spin_->setSuffix(" °");
    rotation_h_layout->addWidget(rotation_slider_);
    rotation_h_layout->addWidget(rotation_spin_);
    connect(rotation_slider_, &QSlider::valueChanged, this, &TextureEditorDialog::onRotationChanged);
    connect(rotation_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this](double value) { 
                rotation_slider_->setValue(static_cast<int>(value * 10)); 
            });
    rotation_layout->addRow("角度:", rotation_h_layout);
    
    main_layout->addWidget(rotation_group);

    // 逆向预扭曲组
    QGroupBox* prewarp_group = new QGroupBox("畸变补偿", this);
    QFormLayout* prewarp_layout = new QFormLayout(prewarp_group);

    inverse_stretch_prewarp_check_ = new QCheckBox("启用逆向预扭曲（按局部拉伸补偿）", this);
    inverse_stretch_prewarp_check_->setChecked(true);
    connect(inverse_stretch_prewarp_check_, &QCheckBox::toggled,
            this, &TextureEditorDialog::onInverseStretchPrewarpToggled);
    prewarp_layout->addRow(inverse_stretch_prewarp_check_);

    QHBoxLayout* prewarp_strength_layout = new QHBoxLayout();
    inverse_stretch_prewarp_slider_ = new QSlider(Qt::Horizontal, this);
    inverse_stretch_prewarp_slider_->setRange(0, 200);
    inverse_stretch_prewarp_slider_->setValue(120);
    inverse_stretch_prewarp_spin_ = new QDoubleSpinBox(this);
    inverse_stretch_prewarp_spin_->setRange(0.0, 2.0);
    inverse_stretch_prewarp_spin_->setSingleStep(0.05);
    inverse_stretch_prewarp_spin_->setDecimals(2);
    inverse_stretch_prewarp_spin_->setValue(1.2);
    prewarp_strength_layout->addWidget(inverse_stretch_prewarp_slider_);
    prewarp_strength_layout->addWidget(inverse_stretch_prewarp_spin_);
    connect(inverse_stretch_prewarp_slider_, &QSlider::valueChanged,
            this, &TextureEditorDialog::onInverseStretchPrewarpStrengthChanged);
    connect(inverse_stretch_prewarp_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this](double value) {
                inverse_stretch_prewarp_slider_->setValue(static_cast<int>(std::round(value * 100.0)));
            });
    prewarp_layout->addRow("补偿强度:", prewarp_strength_layout);

    main_layout->addWidget(prewarp_group);
    
    // 重置按钮
    QPushButton* reset_btn = new QPushButton("重置", this);
    connect(reset_btn, &QPushButton::clicked, this, &TextureEditorDialog::onResetClicked);
    main_layout->addWidget(reset_btn);
    
    // 按钮
    QDialogButtonBox* button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main_layout->addWidget(button_box);
}

TextureEditorDialog::~TextureEditorDialog() = default;

void TextureEditorDialog::onScaleXChanged(int value)
{
    scale_x_ = value / 100.0;
    scale_x_spin_->blockSignals(true);
    scale_x_spin_->setValue(scale_x_);
    scale_x_spin_->blockSignals(false);
    
    if (uniform_scale_) {
        updateScaleYFromX();
    }
    
    emit transformChanged();
}

void TextureEditorDialog::onScaleYChanged(int value)
{
    scale_y_ = value / 100.0;
    scale_y_spin_->blockSignals(true);
    scale_y_spin_->setValue(scale_y_);
    scale_y_spin_->blockSignals(false);
    
    emit transformChanged();
}

void TextureEditorDialog::onTranslateXChanged(double value)
{
    translate_x_ = value;
    const int slider_value = static_cast<int>(std::round(translate_x_ * kTranslateSliderScale));
    translate_x_slider_->blockSignals(true);
    translate_x_slider_->setValue(std::clamp(slider_value, kTranslateSliderMin, kTranslateSliderMax));
    translate_x_slider_->blockSignals(false);
    emit transformChanged();
}

void TextureEditorDialog::onTranslateYChanged(double value)
{
    translate_y_ = value;
    const int slider_value = static_cast<int>(std::round(translate_y_ * kTranslateSliderScale));
    translate_y_slider_->blockSignals(true);
    translate_y_slider_->setValue(std::clamp(slider_value, kTranslateSliderMin, kTranslateSliderMax));
    translate_y_slider_->blockSignals(false);
    emit transformChanged();
}

void TextureEditorDialog::onTranslateXSliderChanged(int value)
{
    translate_x_ = static_cast<double>(value) / static_cast<double>(kTranslateSliderScale);
    translate_x_spin_->blockSignals(true);
    translate_x_spin_->setValue(translate_x_);
    translate_x_spin_->blockSignals(false);
    emit transformChanged();
}

void TextureEditorDialog::onTranslateYSliderChanged(int value)
{
    translate_y_ = static_cast<double>(value) / static_cast<double>(kTranslateSliderScale);
    translate_y_spin_->blockSignals(true);
    translate_y_spin_->setValue(translate_y_);
    translate_y_spin_->blockSignals(false);
    emit transformChanged();
}

void TextureEditorDialog::onRotationChanged(int value)
{
    rotation_ = value / 10.0;
    rotation_spin_->blockSignals(true);
    rotation_spin_->setValue(rotation_);
    rotation_spin_->blockSignals(false);
    
    emit transformChanged();
}

void TextureEditorDialog::onUniformScaleToggled(bool enabled)
{
    uniform_scale_ = enabled;
    scale_y_slider_->setEnabled(!enabled);
    scale_y_spin_->setEnabled(!enabled);
    
    if (enabled) {
        updateScaleYFromX();
    }
}

void TextureEditorDialog::onInverseStretchPrewarpToggled(bool enabled)
{
    enable_inverse_stretch_prewarp_ = enabled;
    inverse_stretch_prewarp_slider_->setEnabled(enabled);
    inverse_stretch_prewarp_spin_->setEnabled(enabled);
    emit transformChanged();
}

void TextureEditorDialog::onInverseStretchPrewarpStrengthChanged(int value)
{
    inverse_stretch_prewarp_strength_ = static_cast<double>(value) / 100.0;
    inverse_stretch_prewarp_spin_->blockSignals(true);
    inverse_stretch_prewarp_spin_->setValue(inverse_stretch_prewarp_strength_);
    inverse_stretch_prewarp_spin_->blockSignals(false);
    emit transformChanged();
}

void TextureEditorDialog::updateScaleYFromX()
{
    scale_y_ = scale_x_;
    scale_y_slider_->blockSignals(true);
    scale_y_slider_->setValue(static_cast<int>(scale_y_ * 100));
    scale_y_slider_->blockSignals(false);
    scale_y_spin_->blockSignals(true);
    scale_y_spin_->setValue(scale_y_);
    scale_y_spin_->blockSignals(false);
}

void TextureEditorDialog::onResetClicked()
{
    setScaleX(1.0);
    setScaleY(1.0);
    setTranslateX(0.0);
    setTranslateY(0.0);
    setRotation(0.0);
    setInverseStretchPrewarpEnabled(true);
    setInverseStretchPrewarpStrength(1.2);
    emit transformChanged();
}

void TextureEditorDialog::setScaleX(double value)
{
    scale_x_ = internalScaleToUi(value);
    scale_x_slider_->setValue(static_cast<int>(scale_x_ * 100));
    scale_x_spin_->setValue(scale_x_);
    if (uniform_scale_) {
        updateScaleYFromX();
    }
}

void TextureEditorDialog::setScaleY(double value)
{
    scale_y_ = internalScaleToUi(value);
    scale_y_slider_->setValue(static_cast<int>(scale_y_ * 100));
    scale_y_spin_->setValue(scale_y_);
}

void TextureEditorDialog::setTranslateX(double value)
{
    translate_x_ = std::clamp(value, -10.0, 10.0);
    translate_x_spin_->setValue(translate_x_);
    translate_x_slider_->setValue(static_cast<int>(std::round(translate_x_ * kTranslateSliderScale)));
}

void TextureEditorDialog::setTranslateY(double value)
{
    translate_y_ = std::clamp(value, -10.0, 10.0);
    translate_y_spin_->setValue(translate_y_);
    translate_y_slider_->setValue(static_cast<int>(std::round(translate_y_ * kTranslateSliderScale)));
}

void TextureEditorDialog::setRotation(double value)
{
    rotation_ = value;
    rotation_slider_->setValue(static_cast<int>(value * 10));
    rotation_spin_->setValue(value);
}

void TextureEditorDialog::setUniformScale(bool enabled)
{
    uniform_scale_ = enabled;
    uniform_scale_check_->setChecked(enabled);
    scale_y_slider_->setEnabled(!enabled);
    scale_y_spin_->setEnabled(!enabled);
    if (enabled) {
        updateScaleYFromX();
    }
}

void TextureEditorDialog::setInverseStretchPrewarpEnabled(bool enabled)
{
    enable_inverse_stretch_prewarp_ = enabled;
    inverse_stretch_prewarp_check_->setChecked(enabled);
    inverse_stretch_prewarp_slider_->setEnabled(enabled);
    inverse_stretch_prewarp_spin_->setEnabled(enabled);
}

void TextureEditorDialog::setInverseStretchPrewarpStrength(double value)
{
    inverse_stretch_prewarp_strength_ = std::clamp(value, 0.0, 2.0);
    inverse_stretch_prewarp_slider_->setValue(static_cast<int>(std::round(inverse_stretch_prewarp_strength_ * 100.0)));
    inverse_stretch_prewarp_spin_->setValue(inverse_stretch_prewarp_strength_);
}

double TextureEditorDialog::getScaleX() const
{
    return uiScaleToInternal(scale_x_);
}

double TextureEditorDialog::getScaleY() const
{
    return uiScaleToInternal(scale_y_);
}

double TextureEditorDialog::uiScaleToInternal(double ui_scale) const
{
    const double s = std::clamp(ui_scale, 0.1, 5.0);
    return std::clamp(1.0 / s, 0.1, 5.0);
}

double TextureEditorDialog::internalScaleToUi(double internal_scale) const
{
    const double s = std::clamp(internal_scale, 0.1, 5.0);
    return std::clamp(1.0 / s, 0.1, 5.0);
}
