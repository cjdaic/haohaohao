#include "parameter_panel.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QPushButton>

ParameterPanel::ParameterPanel(QWidget *parent)
    : QWidget(parent)
    , speed_value_mms_(300.0)
    , spacing_value_mm_(0.1)
    , power_value_w_(20.0)
    , freq_value_hz_(20000.0)
    , angle_value_deg_(0.0)
    , prev_speed_unit_index_(0)
    , prev_spacing_unit_index_(0)
    , prev_power_unit_index_(0)
    , prev_freq_unit_index_(0)
    , prev_angle_unit_index_(0)
{
    setupUI();
    setupUnitConversion();
}

ParameterPanel::~ParameterPanel() = default;

void ParameterPanel::setupUI()
{
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    
    // 全局参数组
    QGroupBox* global_group = new QGroupBox("全局加工参数", this);
    QFormLayout* global_layout = new QFormLayout(global_group);
    
    // 功率
    QHBoxLayout* power_layout = new QHBoxLayout();
    power_spinbox_ = new QDoubleSpinBox(this);
    power_spinbox_->setRange(0.0, 100000.0);
    power_spinbox_->setValue(20.0);
    power_spinbox_->setDecimals(3);
    power_layout->addWidget(power_spinbox_);
    
    power_unit_combo_ = new QComboBox(this);
    power_unit_combo_->addItems(QStringList() << "W" << "kW" << "mW");
    power_unit_combo_->setCurrentIndex(0);
    power_layout->addWidget(power_unit_combo_);
    global_layout->addRow("功率:", power_layout);
    
    // 频率
    QHBoxLayout* freq_layout = new QHBoxLayout();
    freq_spinbox_ = new QDoubleSpinBox(this);
    freq_spinbox_->setRange(0.001, 1000000.0);
    freq_spinbox_->setValue(20000.0);
    freq_spinbox_->setDecimals(3);
    freq_layout->addWidget(freq_spinbox_);
    
    freq_unit_combo_ = new QComboBox(this);
    freq_unit_combo_->addItems(QStringList() << "Hz" << "kHz" << "MHz");
    freq_unit_combo_->setCurrentIndex(0);
    freq_layout->addWidget(freq_unit_combo_);
    global_layout->addRow("频率:", freq_layout);
    
    // 速度
    QHBoxLayout* speed_layout = new QHBoxLayout();
    speed_spinbox_ = new QDoubleSpinBox(this);
    speed_spinbox_->setRange(0.001, 100000.0);
    speed_spinbox_->setValue(300.0);
    speed_spinbox_->setDecimals(3);
    speed_layout->addWidget(speed_spinbox_);
    
    speed_unit_combo_ = new QComboBox(this);
    speed_unit_combo_->addItems(QStringList() << "mm/s" << "m/s" << "cm/s" << "in/s" << "ft/s");
    speed_unit_combo_->setCurrentIndex(0);
    speed_layout->addWidget(speed_unit_combo_);
    global_layout->addRow("速度:", speed_layout);
    
    main_layout->addWidget(global_group);
    
    // 图案参数组
    QGroupBox* pattern_group = new QGroupBox("图案参数", this);
    QFormLayout* pattern_layout = new QFormLayout(pattern_group);
    
    // 填充间距
    QHBoxLayout* spacing_layout = new QHBoxLayout();
    spacing_spinbox_ = new QDoubleSpinBox(this);
    spacing_spinbox_->setRange(0.0001, 1000.0);
    spacing_spinbox_->setValue(0.1);
    spacing_spinbox_->setDecimals(4);
    spacing_layout->addWidget(spacing_spinbox_);
    
    spacing_unit_combo_ = new QComboBox(this);
    spacing_unit_combo_->addItems(QStringList() << "mm" << "cm" << "m" << "in" << "ft");
    spacing_unit_combo_->setCurrentIndex(0);
    spacing_layout->addWidget(spacing_unit_combo_);
    pattern_layout->addRow("填充间距:", spacing_layout);
    
    // 填充角度
    QHBoxLayout* angle_layout = new QHBoxLayout();
    angle_spinbox_ = new QDoubleSpinBox(this);
    angle_spinbox_->setRange(0.0, 360.0);
    angle_spinbox_->setValue(0.0);
    angle_spinbox_->setDecimals(2);
    angle_layout->addWidget(angle_spinbox_);
    
    angle_unit_combo_ = new QComboBox(this);
    angle_unit_combo_->addItems(QStringList() << "°" << "rad");
    angle_unit_combo_->setCurrentIndex(0);
    angle_layout->addWidget(angle_unit_combo_);
    pattern_layout->addRow("填充角度:", angle_layout);
    
    strategy_combo_ = new QComboBox(this);
    strategy_combo_->addItems(QStringList()
        << "轴向往返填充"
        << "轴向往返填充(圆弧)"
        << "回型填充(内->外)"
        << "回型填充(外->内)"
        << "轮廓偏置回型(忽略空洞)"
        << "Z轴分层填充");
    strategy_combo_->setCurrentText("轴向往返填充");
    pattern_layout->addRow("填充策略:", strategy_combo_);

    direction_combo_ = new QComboBox(this);
    direction_combo_->addItems(QStringList() << "按角度" << "锁定U轴" << "锁定V轴");
    direction_combo_->setCurrentText("按角度");
    pattern_layout->addRow("轴向方向:", direction_combo_);

    contour_mode_combo_ = new QComboBox(this);
    contour_mode_combo_->addItem("只加工轮廓", "contour_only");
    contour_mode_combo_->addItem("不加工轮廓", "none");
    contour_mode_combo_->addItem("加工全部", "all");
    contour_mode_combo_->setCurrentIndex(contour_mode_combo_->findData("all"));
    pattern_layout->addRow("加工轮廓:", contour_mode_combo_);
    
    main_layout->addWidget(pattern_group);

    replan_button_ = new QPushButton("重新规划路径", this);
    replan_button_->setToolTip("按当前图案参数重新执行路径规划");
    main_layout->addWidget(replan_button_);
    connect(replan_button_, &QPushButton::clicked, this, &ParameterPanel::replanRequested);
    
    main_layout->addStretch();
}

void ParameterPanel::setupUnitConversion()
{
    // 连接单位选择变化信号
    connect(speed_unit_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParameterPanel::onSpeedUnitChanged);
    connect(spacing_unit_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParameterPanel::onSpacingUnitChanged);
    connect(power_unit_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParameterPanel::onPowerUnitChanged);
    connect(freq_unit_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParameterPanel::onFreqUnitChanged);
    connect(angle_unit_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParameterPanel::onAngleUnitChanged);
}

// Getter方法（返回标准单位）
double ParameterPanel::getPower() const
{
    return convertPowerToW(power_spinbox_->value(), power_unit_combo_->currentIndex());
}

double ParameterPanel::getFreq() const
{
    return convertFreqToHz(freq_spinbox_->value(), freq_unit_combo_->currentIndex());
}

double ParameterPanel::getSpeed() const
{
    return convertSpeedToMMS(speed_spinbox_->value(), speed_unit_combo_->currentIndex());
}

double ParameterPanel::getSpacing() const
{
    return convertSpacingToMM(spacing_spinbox_->value(), spacing_unit_combo_->currentIndex());
}

double ParameterPanel::getAngle() const
{
    return convertAngleToDeg(angle_spinbox_->value(), angle_unit_combo_->currentIndex());
}

QString ParameterPanel::getDirectionMode() const
{
    return direction_combo_ ? direction_combo_->currentText() : "按角度";
}

QString ParameterPanel::getContourProcessMode() const
{
    if (!contour_mode_combo_) {
        return "all";
    }
    const QString mode = contour_mode_combo_->currentData().toString();
    return mode.isEmpty() ? "all" : mode;
}

bool ParameterPanel::isContourEnabled() const
{
    return getContourProcessMode() != "none";
}

bool ParameterPanel::isZLayerEnabled() const
{
    return strategy_combo_ && strategy_combo_->currentText() == "Z轴分层填充";
}

double ParameterPanel::getLayerHeight() const
{
    // 按需求复用“间距”作为层厚输入。
    return getSpacing();
}

// 单位转换槽函数
void ParameterPanel::onSpeedUnitChanged(int index)
{
    // 保存当前值（转换为标准单位mm/s）
    speed_value_mms_ = convertSpeedToMMS(speed_spinbox_->value(), prev_speed_unit_index_);
    
    // 更新显示值
    double new_value = convertSpeedFromMMS(speed_value_mms_, index);
    speed_spinbox_->blockSignals(true);
    speed_spinbox_->setValue(new_value);
    speed_spinbox_->blockSignals(false);
    prev_speed_unit_index_ = index;
}

void ParameterPanel::onSpacingUnitChanged(int index)
{
    spacing_value_mm_ = convertSpacingToMM(spacing_spinbox_->value(), prev_spacing_unit_index_);
    
    double new_value = convertSpacingFromMM(spacing_value_mm_, index);
    spacing_spinbox_->blockSignals(true);
    spacing_spinbox_->setValue(new_value);
    spacing_spinbox_->blockSignals(false);
    prev_spacing_unit_index_ = index;
}

void ParameterPanel::onPowerUnitChanged(int index)
{
    power_value_w_ = convertPowerToW(power_spinbox_->value(), prev_power_unit_index_);
    
    double new_value = convertPowerFromW(power_value_w_, index);
    power_spinbox_->blockSignals(true);
    power_spinbox_->setValue(new_value);
    power_spinbox_->blockSignals(false);
    prev_power_unit_index_ = index;
}

void ParameterPanel::onFreqUnitChanged(int index)
{
    freq_value_hz_ = convertFreqToHz(freq_spinbox_->value(), prev_freq_unit_index_);
    
    double new_value = convertFreqFromHz(freq_value_hz_, index);
    freq_spinbox_->blockSignals(true);
    freq_spinbox_->setValue(new_value);
    freq_spinbox_->blockSignals(false);
    prev_freq_unit_index_ = index;
}

void ParameterPanel::onAngleUnitChanged(int index)
{
    angle_value_deg_ = convertAngleToDeg(angle_spinbox_->value(), prev_angle_unit_index_);
    
    double new_value = convertAngleFromDeg(angle_value_deg_, index);
    angle_spinbox_->blockSignals(true);
    angle_spinbox_->setValue(new_value);
    angle_spinbox_->blockSignals(false);
    prev_angle_unit_index_ = index;
}

// 单位转换函数实现
double ParameterPanel::convertSpeedToMMS(double value, int unit_index) const
{
    switch (unit_index) {
        case 0: return value;           // mm/s
        case 1: return value * 1000.0;  // m/s -> mm/s
        case 2: return value * 10.0;    // cm/s -> mm/s
        case 3: return value * 25.4;    // in/s -> mm/s
        case 4: return value * 304.8;   // ft/s -> mm/s
        default: return value;
    }
}

double ParameterPanel::convertSpeedFromMMS(double value_mms, int unit_index) const
{
    switch (unit_index) {
        case 0: return value_mms;              // mm/s
        case 1: return value_mms / 1000.0;     // mm/s -> m/s
        case 2: return value_mms / 10.0;       // mm/s -> cm/s
        case 3: return value_mms / 25.4;       // mm/s -> in/s
        case 4: return value_mms / 304.8;      // mm/s -> ft/s
        default: return value_mms;
    }
}

double ParameterPanel::convertSpacingToMM(double value, int unit_index) const
{
    switch (unit_index) {
        case 0: return value;           // mm
        case 1: return value * 10.0;    // cm -> mm
        case 2: return value * 1000.0;  // m -> mm
        case 3: return value * 25.4;    // in -> mm
        case 4: return value * 304.8;   // ft -> mm
        default: return value;
    }
}

double ParameterPanel::convertSpacingFromMM(double value_mm, int unit_index) const
{
    switch (unit_index) {
        case 0: return value_mm;              // mm
        case 1: return value_mm / 10.0;       // mm -> cm
        case 2: return value_mm / 1000.0;     // mm -> m
        case 3: return value_mm / 25.4;       // mm -> in
        case 4: return value_mm / 304.8;      // mm -> ft
        default: return value_mm;
    }
}

double ParameterPanel::convertPowerToW(double value, int unit_index) const
{
    switch (unit_index) {
        case 0: return value;           // W
        case 1: return value * 1000.0;  // kW -> W
        case 2: return value / 1000.0;  // mW -> W
        default: return value;
    }
}

double ParameterPanel::convertPowerFromW(double value_w, int unit_index) const
{
    switch (unit_index) {
        case 0: return value_w;              // W
        case 1: return value_w / 1000.0;     // W -> kW
        case 2: return value_w * 1000.0;     // W -> mW
        default: return value_w;
    }
}

double ParameterPanel::convertFreqToHz(double value, int unit_index) const
{
    switch (unit_index) {
        case 0: return value;           // Hz
        case 1: return value * 1000.0;  // kHz -> Hz
        case 2: return value * 1000000.0; // MHz -> Hz
        default: return value;
    }
}

double ParameterPanel::convertFreqFromHz(double value_hz, int unit_index) const
{
    switch (unit_index) {
        case 0: return value_hz;              // Hz
        case 1: return value_hz / 1000.0;     // Hz -> kHz
        case 2: return value_hz / 1000000.0;  // Hz -> MHz
        default: return value_hz;
    }
}

double ParameterPanel::convertAngleToDeg(double value, int unit_index) const
{
    switch (unit_index) {
        case 0: return value;           // 度
        case 1: return value * 180.0 / 3.14159265358979323846; // 弧度 -> 度
        default: return value;
    }
}

double ParameterPanel::convertAngleFromDeg(double value_deg, int unit_index) const
{
    switch (unit_index) {
        case 0: return value_deg;              // 度
        case 1: return value_deg * 3.14159265358979323846 / 180.0; // 度 -> 弧度
        default: return value_deg;
    }
}
