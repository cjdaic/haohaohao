#ifndef GRPC_CLIENT_H
#define GRPC_CLIENT_H

#include <QObject>
#include <memory>
#include <string>
#include "../core/executor/data_buffer.h"
#include "../core/executor/data_generator.h"

namespace nbcam {
class DataBuffer;
}

// gRPC客户端（用于与gRPC服务通信）
class GrpcClient : public QObject
{
    Q_OBJECT

public:
    explicit GrpcClient(QObject *parent = nullptr);
    ~GrpcClient();

    // 连接gRPC服务
    bool connect(const std::string& server_address = "localhost:50051");
    void disconnect();
    bool isConnected() const { return is_connected_; }

    // gRPC方法（对应FiveAxis服务）
    bool processLine(double speed, int times,
                    double X1, double Y1, double Z1, double A1, double B1,
                    double X2, double Y2, double Z2, double A2, double B2,
                    bool isLast = false);
    
    bool processCircle(double speed, int times,
                      double X1, double Y1, double X2, double Y2,
                      int M, double angle, double taper, bool filled,
                      double r_min, double r_interval,
                      double z_start, double z_end, double z_interval,
                      int circle_num_repair, int times_repair,
                      bool isLast = false);
    
    bool processRectangle(double X0, double Y0, double X1, double Y1,
                         double taperAMax, double taperBMax,
                         double feedSpacingX, double feedSpacingY,
                         double speed, double z_start, double z_end, double z_interval,
                         int times, double X2, double Y2,
                         int circle_num_repair, int times_repair,
                         bool isLast = false);
    
    bool processRectangle3D(double X0, double Y0, double Z0,
                           double X1, double Y1, double Z1,
                           double speed, double interval, int times,
                           bool isLast = false);
    
    bool processEllipse(double speed, int times,
                       double X0, double Y0,
                       double aMax, double bMax, double aMin, double bMin,
                       double feedSpacingX, double feedSpacingY,
                       double taperAMax, double taperBMax,
                       double z_start, double z_end, double z_interval,
                       int circle_num_repair, int times_repair,
                       bool isLast = false);
    
    bool setDelay(int jumpSpeed, int laserOnDelay, int laserOffDelay,
                 int markDelay, int jumpDelay, int polygonDelay);
    
    bool setLaserFreq(int freq);

signals:
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString& error);

private:
    bool is_connected_;
    std::string server_address_;
    
    // gRPC stub（需要protobuf生成的代码）
    // std::unique_ptr<nbcam::FiveAxis::Stub> stub_;
    
    // 临时使用DataBuffer直接操作（待gRPC完善后替换）
    std::shared_ptr<nbcam::DataBuffer> data_buffer_;
};

#endif // GRPC_CLIENT_H
