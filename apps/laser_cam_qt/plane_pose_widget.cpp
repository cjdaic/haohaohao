#include "plane_pose_widget.h"
#include <QFormLayout>
#include <QSpacerItem>

PlanePoseWidget::PlanePoseWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void PlanePoseWidget::setupUI()
{
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    
    QGroupBox* pose_group = new QGroupBox("工作平面位姿", this);
    QFormLayout* form_layout = new QFormLayout(pose_group);
    
    // 位置控件
    pos_x_spinbox_ = new QDoubleSpinBox(this);
    pos_x_spinbox_->setRange(-10000.0, 10000.0);
    pos_x_spinbox_->setDecimals(3);
    pos_x_spinbox_->setSuffix(" mm");
    pos_x_spinbox_->setValue(0.0);
    form_layout->addRow("位置 X:", pos_x_spinbox_);
    
    pos_y_spinbox_ = new QDoubleSpinBox(this);
    pos_y_spinbox_->setRange(-10000.0, 10000.0);
    pos_y_spinbox_->setDecimals(3);
    pos_y_spinbox_->setSuffix(" mm");
    pos_y_spinbox_->setValue(0.0);
    form_layout->addRow("位置 Y:", pos_y_spinbox_);
    
    pos_z_spinbox_ = new QDoubleSpinBox(this);
    pos_z_spinbox_->setRange(-10000.0, 10000.0);
    pos_z_spinbox_->setDecimals(3);
    pos_z_spinbox_->setSuffix(" mm");
    pos_z_spinbox_->setValue(0.0);
    form_layout->addRow("位置 Z:", pos_z_spinbox_);
    
    form_layout->addItem(new QSpacerItem(0, 10));
    
    // 旋转控件（欧拉角）
    rot_x_spinbox_ = new QDoubleSpinBox(this);
    rot_x_spinbox_->setRange(-360.0, 360.0);
    rot_x_spinbox_->setDecimals(2);
    rot_x_spinbox_->setSuffix(" °");
    rot_x_spinbox_->setValue(0.0);
    form_layout->addRow("旋转 X:", rot_x_spinbox_);
    
    rot_y_spinbox_ = new QDoubleSpinBox(this);
    rot_y_spinbox_->setRange(-360.0, 360.0);
    rot_y_spinbox_->setDecimals(2);
    rot_y_spinbox_->setSuffix(" °");
    rot_y_spinbox_->setValue(0.0);
    form_layout->addRow("旋转 Y:", rot_y_spinbox_);
    
    rot_z_spinbox_ = new QDoubleSpinBox(this);
    rot_z_spinbox_->setRange(-360.0, 360.0);
    rot_z_spinbox_->setDecimals(2);
    rot_z_spinbox_->setSuffix(" °");
    rot_z_spinbox_->setValue(0.0);
    form_layout->addRow("旋转 Z:", rot_z_spinbox_);
    
    pose_group->setLayout(form_layout);
    main_layout->addWidget(pose_group);
    main_layout->addStretch();
    
    // 连接信号
    connect(pos_x_spinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlanePoseWidget::onValueChanged);
    connect(pos_y_spinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlanePoseWidget::onValueChanged);
    connect(pos_z_spinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlanePoseWidget::onValueChanged);
    connect(rot_x_spinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlanePoseWidget::onValueChanged);
    connect(rot_y_spinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlanePoseWidget::onValueChanged);
    connect(rot_z_spinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlanePoseWidget::onValueChanged);
}

void PlanePoseWidget::getPose(double& pos_x, double& pos_y, double& pos_z,
                              double& rot_x, double& rot_y, double& rot_z) const
{
    pos_x = pos_x_spinbox_->value();
    pos_y = pos_y_spinbox_->value();
    pos_z = pos_z_spinbox_->value();
    rot_x = rot_x_spinbox_->value();
    rot_y = rot_y_spinbox_->value();
    rot_z = rot_z_spinbox_->value();
}

void PlanePoseWidget::setPose(double pos_x, double pos_y, double pos_z,
                              double rot_x, double rot_y, double rot_z)
{
    pos_x_spinbox_->setValue(pos_x);
    pos_y_spinbox_->setValue(pos_y);
    pos_z_spinbox_->setValue(pos_z);
    rot_x_spinbox_->setValue(rot_x);
    rot_y_spinbox_->setValue(rot_y);
    rot_z_spinbox_->setValue(rot_z);
}

void PlanePoseWidget::onValueChanged()
{
    emit poseChanged();
}
