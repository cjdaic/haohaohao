#ifndef PLANE_POSE_WIDGET_H
#define PLANE_POSE_WIDGET_H

#include <QWidget>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

class PlanePoseWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlanePoseWidget(QWidget *parent = nullptr);
    ~PlanePoseWidget() = default;

    // 获取平面位姿
    void getPose(double& pos_x, double& pos_y, double& pos_z,
                 double& rot_x, double& rot_y, double& rot_z) const;
    
    // 设置平面位姿
    void setPose(double pos_x, double pos_y, double pos_z,
                 double rot_x, double rot_y, double rot_z);

signals:
    void poseChanged();

private slots:
    void onValueChanged();

private:
    void setupUI();

    // 位置控件
    QDoubleSpinBox* pos_x_spinbox_;
    QDoubleSpinBox* pos_y_spinbox_;
    QDoubleSpinBox* pos_z_spinbox_;
    
    // 旋转控件（欧拉角，单位：度）
    QDoubleSpinBox* rot_x_spinbox_;
    QDoubleSpinBox* rot_y_spinbox_;
    QDoubleSpinBox* rot_z_spinbox_;
};

#endif // PLANE_POSE_WIDGET_H
