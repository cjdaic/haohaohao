#include "grpc_client.h"
#include "../core/executor/data_buffer.h"
#include "../core/executor/data_generator.h"
#include <spdlog/spdlog.h>

GrpcClient::GrpcClient(QObject *parent)
    : QObject(parent)
    , is_connected_(false)
    , server_address_("localhost:50051")
{
    // 创建DataBuffer（临时方案，待gRPC完善后替换）
    data_buffer_ = std::make_shared<nbcam::DataBuffer>();
}

GrpcClient::~GrpcClient() {
    disconnect();
}

bool GrpcClient::connect(const std::string& server_address) {
    server_address_ = server_address;
    
    // TODO: 实现gRPC连接
    // 需要protobuf生成的代码
    // auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    // stub_ = nbcam::FiveAxis::NewStub(channel);
    
    // 临时方案：直接使用DataBuffer
    if (data_buffer_) {
        data_buffer_->startTcpThread();
        is_connected_ = true;
        emit connectionStatusChanged(true);
        spdlog::info("gRPC客户端连接成功（临时使用DataBuffer）: {}", server_address);
        return true;
    }
    
    return false;
}

void GrpcClient::disconnect() {
    if (is_connected_) {
        if (data_buffer_) {
            data_buffer_->stopTcpThread();
        }
        is_connected_ = false;
        emit connectionStatusChanged(false);
        spdlog::info("gRPC客户端断开连接");
    }
}

bool GrpcClient::processLine(double speed, int times,
                             double X1, double Y1, double Z1, double A1, double B1,
                             double X2, double Y2, double Z2, double A2, double B2,
                             bool isLast) {
    if (!is_connected_ || !data_buffer_) {
        return false;
    }
    
    try {
        nbcam::DataGenerator::genLineData(speed, times,
                                         X1, Y1, Z1, A1, B1,
                                         X2, Y2, Z2, A2, B2,
                                         *data_buffer_);
        if (isLast) {
            data_buffer_->forceFill();
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("processLine失败: {}", e.what());
        emit errorOccurred(QString::fromStdString(e.what()));
        return false;
    }
}

bool GrpcClient::processCircle(double speed, int times,
                               double X1, double Y1, double X2, double Y2,
                               int M, double angle, double taper, bool filled,
                               double r_min, double r_interval,
                               double z_start, double z_end, double z_interval,
                               int circle_num_repair, int times_repair,
                               bool isLast) {
    if (!is_connected_ || !data_buffer_) {
        return false;
    }
    
    try {
        nbcam::DataGenerator::genCircleData(speed, times,
                                           X1, Y1, X2, Y2,
                                           M, angle, taper, filled,
                                           r_min, r_interval,
                                           z_start, z_end, z_interval,
                                           circle_num_repair, times_repair,
                                           *data_buffer_);
        if (isLast) {
            data_buffer_->forceFill();
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("processCircle失败: {}", e.what());
        emit errorOccurred(QString::fromStdString(e.what()));
        return false;
    }
}

bool GrpcClient::processRectangle(double X0, double Y0, double X1, double Y1,
                                  double taperAMax, double taperBMax,
                                  double feedSpacingX, double feedSpacingY,
                                  double speed, double z_start, double z_end, double z_interval,
                                  int times, double X2, double Y2,
                                  int circle_num_repair, int times_repair,
                                  bool isLast) {
    if (!is_connected_ || !data_buffer_) {
        return false;
    }
    
    try {
        nbcam::DataGenerator::genFilledRectangleData(X0, Y0, X1, Y1,
                                                      taperAMax, taperBMax,
                                                      feedSpacingX, feedSpacingY,
                                                      speed, times,
                                                      z_start, z_end, z_interval,
                                                      X2, Y2,
                                                      circle_num_repair, times_repair,
                                                      *data_buffer_);
        if (isLast) {
            data_buffer_->forceFill();
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("processRectangle失败: {}", e.what());
        emit errorOccurred(QString::fromStdString(e.what()));
        return false;
    }
}

bool GrpcClient::processRectangle3D(double X0, double Y0, double Z0,
                                   double X1, double Y1, double Z1,
                                   double speed, double interval, int times,
                                   bool isLast) {
    if (!is_connected_ || !data_buffer_) {
        return false;
    }
    
    try {
        nbcam::DataGenerator::genFilledRectangleData3D(X0, Y0, Z0,
                                                       X1, Y1, Z1,
                                                       speed, interval, times,
                                                       *data_buffer_);
        if (isLast) {
            data_buffer_->forceFill();
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("processRectangle3D失败: {}", e.what());
        emit errorOccurred(QString::fromStdString(e.what()));
        return false;
    }
}

bool GrpcClient::processEllipse(double speed, int times,
                                double X0, double Y0,
                                double aMax, double bMax, double aMin, double bMin,
                                double feedSpacingX, double feedSpacingY,
                                double taperAMax, double taperBMax,
                                double z_start, double z_end, double z_interval,
                                int circle_num_repair, int times_repair,
                                bool isLast) {
    if (!is_connected_ || !data_buffer_) {
        return false;
    }
    
    try {
        nbcam::DataGenerator::genFilledEllipseData(speed, times,
                                                   X0, Y0,
                                                   aMax, bMax, aMin, bMin,
                                                   feedSpacingX, feedSpacingY,
                                                   taperAMax, taperBMax,
                                                   z_start, z_end, z_interval,
                                                   circle_num_repair, times_repair,
                                                   *data_buffer_);
        if (isLast) {
            data_buffer_->forceFill();
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("processEllipse失败: {}", e.what());
        emit errorOccurred(QString::fromStdString(e.what()));
        return false;
    }
}

bool GrpcClient::setDelay(int jumpSpeed, int laserOnDelay, int laserOffDelay,
                         int markDelay, int jumpDelay, int polygonDelay) {
    if (!is_connected_) {
        return false;
    }
    
    try {
        nbcam::DataGenerator::JUMP_SPEED = jumpSpeed;
        nbcam::DataGenerator::LASER_ON_DELAY = laserOnDelay;
        nbcam::DataGenerator::LASER_OFF_DELAY = laserOffDelay;
        nbcam::DataGenerator::MARK_DELAY = markDelay;
        nbcam::DataGenerator::JUMP_DELAY = jumpDelay;
        nbcam::DataGenerator::POLYGON_DELAY = polygonDelay;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("setDelay失败: {}", e.what());
        emit errorOccurred(QString::fromStdString(e.what()));
        return false;
    }
}

bool GrpcClient::setLaserFreq(int freq) {
    if (!is_connected_ || !data_buffer_) {
        return false;
    }
    
    try {
        nbcam::DataGenerator::setFreq(freq, *data_buffer_);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("setLaserFreq失败: {}", e.what());
        emit errorOccurred(QString::fromStdString(e.what()));
        return false;
    }
}
