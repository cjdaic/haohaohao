#ifndef PARAMETER_PANEL_H
#define PARAMETER_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QString>

class ParameterPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterPanel(QWidget *parent = nullptr);
    ~ParameterPanel() override;

    // Getter方法（返回标准单位的值）
    double getPower() const;  // 返回W
    double getFreq() const;   // 返回Hz
    double getSpeed() const;  // 返回mm/s
    double getSpacing() const; // 返回mm
    double getAngle() const;  // 返回度
    QString getStrategy() const { return strategy_combo_->currentText(); }
    QString getDirectionMode() const;
    QString getContourProcessMode() const;
    bool isContourEnabled() const;
    bool isZLayerEnabled() const;
    bool isLaserOutputEnabled() const;
    void setLaserOutputEnabled(bool enabled);
    double getLayerHeight() const;  // 返回mm

signals:
    void replanRequested();

private slots:
    void onSpeedUnitChanged(int index);
    void onSpacingUnitChanged(int index);
    void onPowerUnitChanged(int index);
    void onFreqUnitChanged(int index);
    void onAngleUnitChanged(int index);

private:
    void setupUI();
    void setupUnitConversion();
    
    // 单位转换函数
    double convertSpeedToMMS(double value, int unit_index) const;
    double convertSpeedFromMMS(double value_mms, int unit_index) const;
    double convertSpacingToMM(double value, int unit_index) const;
    double convertSpacingFromMM(double value_mm, int unit_index) const;
    double convertPowerToW(double value, int unit_index) const;
    double convertPowerFromW(double value_w, int unit_index) const;
    double convertFreqToHz(double value, int unit_index) const;
    double convertFreqFromHz(double value_hz, int unit_index) const;
    double convertAngleToDeg(double value, int unit_index) const;
    double convertAngleFromDeg(double value_deg, int unit_index) const;
    
    // 存储标准单位的值（用于内部计算）
    double speed_value_mms_;  // 速度（mm/s）
    double spacing_value_mm_;  // 间距（mm）
    double power_value_w_;     // 功率（W）
    double freq_value_hz_;     // 频率（Hz）
    double angle_value_deg_;   // 角度（度）
    
    // 存储之前的单位索引
    int prev_speed_unit_index_;
    int prev_spacing_unit_index_;
    int prev_power_unit_index_;
    int prev_freq_unit_index_;
    int prev_angle_unit_index_;
    
    // 全局参数
    QDoubleSpinBox* power_spinbox_;
    QDoubleSpinBox* freq_spinbox_;
    QDoubleSpinBox* speed_spinbox_;
    
    // 图案参数
    QDoubleSpinBox* spacing_spinbox_;
    QDoubleSpinBox* angle_spinbox_;
    QComboBox* strategy_combo_;
    QComboBox* direction_combo_;
    QComboBox* contour_mode_combo_;
    QCheckBox* laser_output_check_;
    QPushButton* replan_button_;
    
    // 单位选择下拉框
    QComboBox* speed_unit_combo_;
    QComboBox* spacing_unit_combo_;
    QComboBox* power_unit_combo_;
    QComboBox* freq_unit_combo_;
    QComboBox* angle_unit_combo_;
};

#endif // PARAMETER_PANEL_H
