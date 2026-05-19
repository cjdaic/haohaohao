#include "mainwindow.h"
#include "application_controller.h"
#include "texture_editor_dialog.h"
#include "texture_list_widget.h"
#include "texture_details_dialog.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "model_view.h"
#include "uv_view.h"
#include "path_preview.h"
#include "parameter_panel.h"
#include "plane_pose_widget.h"
#include "../core/executor/data_buffer.h"
#include "../core/executor/board_executor.h"
#include "../core/executor/rtc6_executor.h"
#include "../core/executor/frame_log_utils.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QActionGroup>
#include <QInputDialog>
#include <QTimer>
#include <QShowEvent>
#include <QSettings>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QEventLoop>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QPixmap>
#include <QPushButton>
#include <QProcess>
#include <QSet>
#include <QToolButton>
#include <QThread>
#include <QTranslator>
#include <QDesktopServices>
#include <QDateTime>
#include <QRegularExpression>
#include <QStyle>
#include <QTransform>
#include <QUrl>
#include <QVector>
#include <QWidgetAction>
#include <array>
#include <limits>
#include <queue>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace {

constexpr QFileDialog::Options kSafeDialogOptions = QFileDialog::DontUseNativeDialog;
constexpr int kCommTimeoutMs = 800;
constexpr double kProcessingRegionMinMm = -32.768;
constexpr double kProcessingRegionMaxMm = 32.768;
constexpr int kDefaultTcpMaxConnectAttempts = 5;

QString openFileSafe(QWidget* parent,
                     const QString& caption,
                     const QString& dir,
                     const QString& filter)
{
    return QFileDialog::getOpenFileName(parent, caption, dir, filter, nullptr, kSafeDialogOptions);
}

QString saveFileSafe(QWidget* parent,
                     const QString& caption,
                     const QString& dir,
                     const QString& filter)
{
    return QFileDialog::getSaveFileName(parent, caption, dir, filter, nullptr, kSafeDialogOptions);
}

nbcam::PathPoint clonePathPointDeep(const nbcam::PathPoint& src)
{
    nbcam::PathPoint dst;
    dst.u = src.u;
    dst.v = src.v;
    dst.x = src.x;
    dst.y = src.y;
    dst.z = src.z;
    dst.a = src.a;
    dst.b = src.b;
    dst.laser = src.laser;
    if (src.params_override) {
        dst.params_override = std::make_unique<nbcam::ProcessParams>(*src.params_override);
    }
    return dst;
}

nbcam::PathSegment clonePathSegmentDeep(const nbcam::PathSegment& src, int id)
{
    nbcam::PathSegment dst;
    dst.id = id;
    dst.type = src.type;
    dst.strategy = src.strategy;
    if (src.params_override) {
        dst.params_override = std::make_unique<nbcam::ProcessParams>(*src.params_override);
    }
    dst.points.reserve(src.points.size());
    for (const auto& pt : src.points) {
        dst.points.push_back(clonePathPointDeep(pt));
    }
    return dst;
}

std::unique_ptr<nbcam::LaserJob> cloneLaserJobDeep(const nbcam::LaserJob& src)
{
    auto dst = std::make_unique<nbcam::LaserJob>();
    dst->meta = src.meta;
    dst->coordinate = src.coordinate;
    dst->parameterization = src.parameterization;
    dst->process_defaults = src.process_defaults;
    dst->segments.reserve(src.segments.size());
    for (const auto& seg : src.segments) {
        dst->segments.push_back(clonePathSegmentDeep(seg, static_cast<int>(dst->segments.size())));
    }
    return dst;
}

void appendJobSegmentsDeep(nbcam::LaserJob& dst, const nbcam::LaserJob& src)
{
    for (const auto& seg : src.segments) {
        dst.segments.push_back(clonePathSegmentDeep(seg, static_cast<int>(dst.segments.size())));
    }
}

struct ContourStats {
    int contour_loops = 0;
    int contour_mark_segments = 0;
    int contour_jump_segments = 0;
    int contour_mark_points = 0;
};

ContourStats collectContourStats(const nbcam::LaserJob* job)
{
    ContourStats stats;
    if (!job) {
        return stats;
    }

    for (const auto& seg : job->segments) {
        if (seg.strategy != nbcam::FillStrategy::CONTOUR) {
            continue;
        }
        if (seg.type == nbcam::SegmentType::MARK) {
            ++stats.contour_mark_segments;
            ++stats.contour_loops;
            stats.contour_mark_points += static_cast<int>(seg.points.size());
        } else if (seg.type == nbcam::SegmentType::JUMP) {
            ++stats.contour_jump_segments;
        }
    }
    return stats;
}

QStringList candidateBaseDirs()
{
    const QString app_dir = QCoreApplication::applicationDirPath();
    QStringList bases;
    bases << QDir::currentPath()
          << app_dir
          << QDir(app_dir).absoluteFilePath("..")
          << QDir(app_dir).absoluteFilePath("../..")
          << QDir(app_dir).absoluteFilePath("../../..")
          << QDir(app_dir).absoluteFilePath("../../../..");
    bases.removeDuplicates();
    return bases;
}

QString bytesToHex(const QByteArray& data)
{
    if (data.isEmpty()) {
        return "none";
    }

    QStringList parts;
    parts.reserve(data.size());
    for (unsigned char value : data) {
        parts.push_back(QString("%1").arg(static_cast<int>(value), 2, 16, QChar('0')).toUpper());
    }
    return parts.join(' ');
}

bool queryMachiningStartOptions(QWidget* parent,
                                const QString& title,
                                bool* out_laser_enabled,
                                bool* out_segment_feedback_enabled)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);

    auto* layout = new QVBoxLayout(&dialog);
    auto* intro = new QLabel("请确认本次开始加工的执行选项。", &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* laser_group = new QGroupBox("激光输出", &dialog);
    auto* laser_layout = new QVBoxLayout(laser_group);
    auto* no_laser_button = new QRadioButton("不开光", laser_group);
    auto* laser_button = new QRadioButton("开光", laser_group);
    no_laser_button->setChecked(true);
    laser_layout->addWidget(no_laser_button);
    laser_layout->addWidget(laser_button);
    auto* laser_hint = new QLabel("不开光时，只会把实际发送到设备的数据帧开光位强制置0，任务中的点位开关光属性保持不变。", laser_group);
    laser_hint->setWordWrap(true);
    laser_layout->addWidget(laser_hint);
    layout->addWidget(laser_group);

    auto* feedback_group = new QGroupBox("发送反馈", &dialog);
    auto* feedback_layout = new QVBoxLayout(feedback_group);
    auto* segment_feedback_box = new QCheckBox("按段级反馈", feedback_group);
    segment_feedback_box->setChecked(false);
    feedback_layout->addWidget(segment_feedback_box);
    auto* feedback_hint = new QLabel("勾选后按现有逐段等待流程发送；不勾选时仅输出 warning 及以上日志，数据连续下发，全部发送完成后再统一反馈加工结果。", feedback_group);
    feedback_hint->setWordWrap(true);
    feedback_layout->addWidget(feedback_hint);
    layout->addWidget(feedback_group);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    if (out_laser_enabled) {
        *out_laser_enabled = laser_button->isChecked();
    }
    if (out_segment_feedback_enabled) {
        *out_segment_feedback_enabled = segment_feedback_box->isChecked();
    }
    return true;
}

QByteArray packFrameBytes(uint16_t arg1 = 0,
                          uint16_t arg2 = 0,
                          uint16_t arg3 = 0,
                          uint16_t arg4 = 0,
                          uint16_t arg5 = 0,
                          uint16_t arg6 = 0,
                          uint16_t arg7 = 0,
                          uint16_t arg8 = 0)
{
    const uint16_t frame[8] = {arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8};
    QByteArray bytes(16, Qt::Uninitialized);
    for (int i = 0; i < 8; ++i) {
        bytes[i * 2] = static_cast<char>(frame[i] & 0xFF);
        bytes[i * 2 + 1] = static_cast<char>((frame[i] >> 8) & 0xFF);
    }
    return bytes;
}

int computeInterpolationSamplesForMachining(uint16_t start_x,
                                            uint16_t start_y,
                                            uint16_t start_z,
                                            uint16_t end_x,
                                            uint16_t end_y,
                                            uint16_t end_z,
                                            double speed_mm_s)
{
    const double safe_speed_mm_s = std::max(1e-6, speed_mm_s);
    constexpr double kTickSec = 0.00001;
    const double dx = static_cast<double>(static_cast<int>(end_x) - static_cast<int>(start_x));
    const double dy = static_cast<double>(static_cast<int>(end_y) - static_cast<int>(start_y));
    const double dz = static_cast<double>(static_cast<int>(end_z) - static_cast<int>(start_z));
    const double length_xy = 0.001 * std::sqrt(dx * dx + dy * dy);
    const double length_z = 0.001 * std::abs(dz);

    int sample_count = 1;
    if (length_xy > 1e-12) {
        sample_count = static_cast<int>((length_xy / (safe_speed_mm_s * kTickSec)) + 1.0);
    } else if (length_z > 1e-12) {
        sample_count = static_cast<int>((length_z / (safe_speed_mm_s * kTickSec)) + 1.0);
    }
    return std::max(1, sample_count);
}

uint16_t interpolateAxisForMachining(uint16_t a, uint16_t b, int sample_index, int sample_count)
{
    if (sample_count <= 0) {
        return b;
    }
    const double ratio = static_cast<double>(sample_index) / static_cast<double>(sample_count);
    const double raw = std::round(static_cast<double>(a) +
                                  (static_cast<double>(b) - static_cast<double>(a)) * ratio);
    return static_cast<uint16_t>(std::clamp(raw, 0.0, 65535.0));
}

int clampNonNegativeMachining(int value)
{
    return std::max(0, value);
}

double resolveSpeedMmSMachining(const nbcam::ProcessParams* params, double fallback_mm_s)
{
    if (params && std::isfinite(params->speed_mm_s) && params->speed_mm_s > 1e-6) {
        return params->speed_mm_s;
    }
    if (std::isfinite(fallback_mm_s) && fallback_mm_s > 1e-6) {
        return fallback_mm_s;
    }
    return 300.0;
}

int resolveLaserOnDelayMachining(const nbcam::ProcessParams* params, int fallback_us)
{
    if (params && params->laser_on_delay_us > 0) {
        return static_cast<int>(std::min<int64_t>(params->laser_on_delay_us,
                                                  static_cast<int64_t>((std::numeric_limits<int>::max)())));
    }
    return clampNonNegativeMachining(fallback_us);
}

int resolveLaserOffDelayMachining(const nbcam::ProcessParams* params, int fallback_us)
{
    if (params && params->laser_off_delay_us > 0) {
        return static_cast<int>(std::min<int64_t>(params->laser_off_delay_us,
                                                  static_cast<int64_t>((std::numeric_limits<int>::max)())));
    }
    return clampNonNegativeMachining(fallback_us);
}

struct MachineEncodedPoint {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t z = 0;
    uint16_t a = 0x8000;
    uint16_t b = 0x8000;
};

bool transformCoordinatesMachining(double& x, double& y, double& z, const std::vector<double>& transform)
{
    if (transform.size() != 16) {
        return false;
    }

    const double original_x = x;
    const double original_y = y;
    const double original_z = z;

    const double tx = transform[0] * original_x + transform[1] * original_y + transform[2] * original_z + transform[3];
    const double ty = transform[4] * original_x + transform[5] * original_y + transform[6] * original_z + transform[7];
    const double tz = transform[8] * original_x + transform[9] * original_y + transform[10] * original_z + transform[11];
    const double tw = transform[12] * original_x + transform[13] * original_y + transform[14] * original_z + transform[15];

    if (!std::isfinite(tx) || !std::isfinite(ty) || !std::isfinite(tz) || !std::isfinite(tw)) {
        return false;
    }

    if (std::abs(tw) > 1e-9 && std::abs(tw - 1.0) > 1e-9) {
        x = tx / tw;
        y = ty / tw;
        z = tz / tw;
    } else {
        x = tx;
        y = ty;
        z = tz;
    }

    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

bool encodeAxisMachining(double mm,
                         const nbcam::BoardExecutor::AxisConfig& axis_config,
                         const char* axis_name,
                         uint16_t* out_value,
                         QString* out_error)
{
    if (!std::isfinite(mm)) {
        if (out_error) {
            *out_error = QString("%1轴坐标不是有限值").arg(axis_name);
        }
        return false;
    }
    if (axis_config.limit_min_mm > axis_config.limit_max_mm) {
        if (out_error) {
            *out_error = QString("%1轴软限位设置非法").arg(axis_name);
        }
        return false;
    }
    if (mm < axis_config.limit_min_mm || mm > axis_config.limit_max_mm) {
        if (out_error) {
            *out_error = QString("%1轴超出软限位: mm=%2, limit=[%3, %4]")
                             .arg(axis_name)
                             .arg(mm, 0, 'f', 6)
                             .arg(axis_config.limit_min_mm, 0, 'f', 6)
                             .arg(axis_config.limit_max_mm, 0, 'f', 6);
        }
        return false;
    }

    constexpr double kAxisEncodeBias = 32768.0;
    constexpr double kAxisEncodeScale = 1000.0;
    const double raw = std::llround(mm * kAxisEncodeScale + kAxisEncodeBias);
    if (raw < 0.0 || raw > 65535.0) {
        if (out_error) {
            *out_error = QString("%1轴编码溢出: mm=%2, raw=%3").arg(axis_name).arg(mm).arg(raw);
        }
        return false;
    }

    if (out_value) {
        *out_value = static_cast<uint16_t>(raw);
    }
    return true;
}

bool encodePointForMachining(const nbcam::LaserJob& job,
                             const nbcam::PathPoint& point,
                             const nbcam::BoardExecutor::MachineConfig& machine_config,
                             size_t segment_index,
                             size_t point_index,
                             MachineEncodedPoint* out_point,
                             QString* out_error)
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
        if (out_error) {
            *out_error = QString("segment=%1, point=%2: 存在非有限坐标点")
                             .arg(static_cast<qulonglong>(segment_index))
                             .arg(static_cast<qulonglong>(point_index));
        }
        return false;
    }

    double x = point.x;
    double y = point.y;
    double z = point.z;
    if (machine_config.apply_job_transform && !job.coordinate.transform_model_to_machine.empty()) {
        if (!transformCoordinatesMachining(x, y, z, job.coordinate.transform_model_to_machine)) {
            if (out_error) {
                *out_error = QString("segment=%1, point=%2: 坐标变换失败")
                                 .arg(static_cast<qulonglong>(segment_index))
                                 .arg(static_cast<qulonglong>(point_index));
            }
            return false;
        }
    }

    x += machine_config.x_axis.offset_mm;
    y += machine_config.y_axis.offset_mm;
    z += machine_config.z_axis.offset_mm;

    MachineEncodedPoint encoded;
    QString axis_error;
    if (!encodeAxisMachining(x, machine_config.x_axis, "X", &encoded.x, &axis_error) ||
        !encodeAxisMachining(y, machine_config.y_axis, "Y", &encoded.y, &axis_error) ||
        !encodeAxisMachining(z, machine_config.z_axis, "Z", &encoded.z, &axis_error)) {
        if (out_error) {
            *out_error = QString("segment=%1, point=%2: %3")
                             .arg(static_cast<qulonglong>(segment_index))
                             .arg(static_cast<qulonglong>(point_index))
                             .arg(axis_error);
        }
        return false;
    }
    if (machine_config.axis_mode == nbcam::BoardExecutor::AxisMode::FiveAxis) {
        const double a = point.a + machine_config.a_axis.offset_mm;
        const double b = point.b + machine_config.b_axis.offset_mm;
        if (!encodeAxisMachining(a, machine_config.a_axis, "A", &encoded.a, &axis_error) ||
            !encodeAxisMachining(b, machine_config.b_axis, "B", &encoded.b, &axis_error)) {
            if (out_error) {
                *out_error = QString("segment=%1, point=%2: %3")
                                 .arg(static_cast<qulonglong>(segment_index))
                                 .arg(static_cast<qulonglong>(point_index))
                                 .arg(axis_error);
            }
            return false;
        }
    }

    if (out_point) {
        *out_point = encoded;
    }
    return true;
}

QString localeFromQtTranslationFile(const QString& file_name)
{
    if (file_name.startsWith("qtbase_") && file_name.endsWith(".qm")) {
        return file_name.mid(7, file_name.size() - 10);
    }
    if (file_name.startsWith("qt_") && file_name.endsWith(".qm")) {
        return file_name.mid(3, file_name.size() - 6);
    }
    return {};
}

bool parseHostPort(const QString& endpoint, QString& out_host, int& out_port, QString* out_error)
{
    QString normalized = endpoint.trimmed();
    const int schema_index = normalized.indexOf("://");
    if (schema_index >= 0) {
        normalized = normalized.mid(schema_index + 3);
    }

    const int slash_index = normalized.indexOf('/');
    if (slash_index >= 0) {
        normalized = normalized.left(slash_index);
    }

    const int colon_index = normalized.lastIndexOf(':');
    if (colon_index <= 0 || colon_index >= normalized.size() - 1) {
        if (out_error) {
            *out_error = QString("端点格式无效: %1（需要 host:port）").arg(endpoint);
        }
        return false;
    }

    bool ok = false;
    const int parsed_port = normalized.mid(colon_index + 1).toInt(&ok);
    if (!ok || parsed_port <= 0 || parsed_port > 65535) {
        if (out_error) {
            *out_error = QString("端口无效: %1").arg(normalized.mid(colon_index + 1));
        }
        return false;
    }

    const QString host = normalized.left(colon_index).trimmed();
    if (host.isEmpty()) {
        if (out_error) {
            *out_error = "主机地址为空";
        }
        return false;
    }

    out_host = host;
    out_port = parsed_port;
    return true;
}

#ifdef _WIN32

QString winSockErrorString(int code)
{
    char* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD len = FormatMessageA(flags, nullptr, static_cast<DWORD>(code), 0, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
    QString msg = QString("code=%1").arg(code);
    if (len > 0 && buffer) {
        msg = QString("code=%1, msg=%2").arg(code).arg(QString::fromLocal8Bit(buffer).trimmed());
        LocalFree(buffer);
    }
    return msg;
}

bool socketConnectWithTimeout(SOCKET sock, const sockaddr* addr, int addr_len, int timeout_ms, QString* out_feedback)
{
    u_long non_blocking = 1;
    if (ioctlsocket(sock, FIONBIO, &non_blocking) != 0) {
        if (out_feedback) {
            *out_feedback = QString("设置非阻塞失败: %1").arg(winSockErrorString(WSAGetLastError()));
        }
        return false;
    }

    int rc = connect(sock, addr, addr_len);
    if (rc != 0) {
        const int connect_error = WSAGetLastError();
        if (connect_error != WSAEWOULDBLOCK && connect_error != WSAEINPROGRESS && connect_error != WSAEINVAL) {
            if (out_feedback) {
                *out_feedback = QString("connect失败: %1").arg(winSockErrorString(connect_error));
            }
            return false;
        }
    }

    fd_set write_set;
    fd_set err_set;
    FD_ZERO(&write_set);
    FD_ZERO(&err_set);
    FD_SET(sock, &write_set);
    FD_SET(sock, &err_set);
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    const int select_rc = select(0, nullptr, &write_set, &err_set, &tv);
    if (select_rc <= 0) {
        if (out_feedback) {
            if (select_rc == 0) {
                *out_feedback = QString("connect超时(%1ms)").arg(timeout_ms);
            } else {
                *out_feedback = QString("select失败: %1").arg(winSockErrorString(WSAGetLastError()));
            }
        }
        return false;
    }

    int so_error = 0;
    int so_len = sizeof(so_error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &so_len) != 0 || so_error != 0) {
        if (out_feedback) {
            const int code = (so_error != 0) ? so_error : WSAGetLastError();
            *out_feedback = QString("SO_ERROR异常: %1").arg(winSockErrorString(code));
        }
        return false;
    }

    non_blocking = 0;
    ioctlsocket(sock, FIONBIO, &non_blocking);
    return true;
}

bool probeTcpEndpoint(const QString& host, int port, int timeout_ms, QString* out_feedback)
{
    WSADATA wsa_data{};
    const int startup_rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (startup_rc != 0) {
        if (out_feedback) {
            *out_feedback = QString("WSAStartup失败: %1").arg(startup_rc);
        }
        return false;
    }

    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const std::string host_utf8 = host.toStdString();
    const std::string port_utf8 = std::to_string(port);
    const int gai_rc = getaddrinfo(host_utf8.c_str(), port_utf8.c_str(), &hints, &result);
    if (gai_rc != 0 || !result) {
        if (out_feedback) {
            *out_feedback = QString("地址解析失败: %1").arg(gai_strerrorA(gai_rc));
        }
        WSACleanup();
        return false;
    }

    bool connected = false;
    QString feedback = "未匹配到可用地址";
    for (addrinfo* it = result; it != nullptr; it = it->ai_next) {
        SOCKET sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock == INVALID_SOCKET) {
            feedback = QString("socket创建失败: %1").arg(winSockErrorString(WSAGetLastError()));
            continue;
        }

        QString connect_feedback;
        if (socketConnectWithTimeout(sock, it->ai_addr, static_cast<int>(it->ai_addrlen), timeout_ms, &connect_feedback)) {
            connected = true;
            feedback = QString("连接成功，耗时阈值=%1ms").arg(timeout_ms);
            closesocket(sock);
            break;
        }

        feedback = connect_feedback;
        closesocket(sock);
    }

    freeaddrinfo(result);
    WSACleanup();
    if (out_feedback) {
        *out_feedback = feedback;
    }
    return connected;
}

bool probeTcpSendAndRecv(const QString& host, int port, const std::array<uint8_t, 16>& payload, int timeout_ms, QString* out_feedback)
{
    WSADATA wsa_data{};
    const int startup_rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (startup_rc != 0) {
        if (out_feedback) {
            *out_feedback = QString("WSAStartup失败: %1").arg(startup_rc);
        }
        return false;
    }

    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const std::string host_utf8 = host.toStdString();
    const std::string port_utf8 = std::to_string(port);
    const int gai_rc = getaddrinfo(host_utf8.c_str(), port_utf8.c_str(), &hints, &result);
    if (gai_rc != 0 || !result) {
        if (out_feedback) {
            *out_feedback = QString("地址解析失败: %1").arg(gai_strerrorA(gai_rc));
        }
        WSACleanup();
        return false;
    }

    bool success = false;
    QString feedback = "TCP发送失败";
    for (addrinfo* it = result; it != nullptr; it = it->ai_next) {
        SOCKET sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock == INVALID_SOCKET) {
            feedback = QString("socket创建失败: %1").arg(winSockErrorString(WSAGetLastError()));
            continue;
        }

        QString connect_feedback;
        if (!socketConnectWithTimeout(sock, it->ai_addr, static_cast<int>(it->ai_addrlen), timeout_ms, &connect_feedback)) {
            feedback = connect_feedback;
            closesocket(sock);
            continue;
        }

        DWORD timeout_value = static_cast<DWORD>(timeout_ms);
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_value), sizeof(timeout_value));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_value), sizeof(timeout_value));

        int sent_total = 0;
        while (sent_total < static_cast<int>(payload.size())) {
            const int sent = send(sock,
                                  reinterpret_cast<const char*>(payload.data()) + sent_total,
                                  static_cast<int>(payload.size()) - sent_total,
                                  0);
            if (sent <= 0) {
                feedback = QString("send失败: %1").arg(winSockErrorString(WSAGetLastError()));
                break;
            }
            sent_total += sent;
        }

        if (sent_total == static_cast<int>(payload.size())) {
            char recv_buf[128] = {0};
            const int recv_size = recv(sock, recv_buf, static_cast<int>(sizeof(recv_buf)), 0);
            QByteArray recv_data;
            if (recv_size > 0) {
                recv_data = QByteArray(recv_buf, recv_size);
            }

            feedback = QString("发送%1字节，反馈=%2")
                           .arg(sent_total)
                           .arg(bytesToHex(recv_data));
            success = true;
        }

        closesocket(sock);
        if (success) {
            break;
        }
    }

    freeaddrinfo(result);
    WSACleanup();
    if (out_feedback) {
        *out_feedback = feedback;
    }
    return success;
}

#endif

}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , main_splitter_(nullptr)
    , model_view_(nullptr)
    , uv_view_(nullptr)
    , path_preview_(nullptr)
    , path_preview_button_(nullptr)
    , path_preview_dialog_(nullptr)
    , parameter_panel_(nullptr)
    , plane_pose_widget_(nullptr)
    , controller_(nullptr)
    , file_menu_(nullptr)
    , view_menu_(nullptr)
    , tools_menu_(nullptr)
    , connection_menu_(nullptr)
    , device_board_menu_(nullptr)
    , manual_menu_(nullptr)
    , measure_menu_(nullptr)
    , log_menu_(nullptr)
    , language_menu_(nullptr)
    , help_menu_(nullptr)
    , left_splitter_(nullptr)
    , texture_list_widget_(nullptr)
    , open_model_action_(nullptr)
    , execute_job_action_(nullptr)
    , parameterize_action_(nullptr)
    , generate_pattern_action_(nullptr)
    , import_svg_action_(nullptr)
    , exit_action_(nullptr)
    , about_action_(nullptr)
    , parameterization_group_(nullptr)
    , param_lscm_action_(nullptr)
    , param_arap_action_(nullptr)
    , wireframe_3d_action_(nullptr)
    , wireframe_uv_action_(nullptr)
    , bounding_box_action_(nullptr)
    , axes_action_(nullptr)
    , manual_operation_dialog_(nullptr)
    , language_group_(nullptr)
    , qt_translator_(nullptr)
    , machining_control_widget_(nullptr)
    , machining_start_button_(nullptr)
    , machining_pause_button_(nullptr)
    , machining_stop_button_(nullptr)
    , machining_refresh_button_(nullptr)
    , machining_simulate_button_(nullptr)
    , machining_timer_(nullptr)
    , machining_progress_timer_(nullptr)
    , texture_editor_dialog_(nullptr)
    , texture_details_dialog_(nullptr)
    , plan_path_action_(nullptr)
{
    // 创建控制器
    controller_ = new ApplicationController(this);
    machining_timer_ = new QTimer(this);
    machining_timer_->setSingleShot(true);
    connect(machining_timer_, &QTimer::timeout, this, &MainWindow::processMachiningStep);
    machining_progress_timer_ = new QTimer(this);
    machining_progress_timer_->setSingleShot(true);
    connect(machining_progress_timer_, &QTimer::timeout, this, &MainWindow::updateMachiningProgressStep);
    machining_data_buffer_ = std::make_unique<nbcam::DataBuffer>();
    
    setupUI();
    createMenus();
    createStatusBar();
    connectSignals();
    
    // 连接平面位姿控件信号
    if (plane_pose_widget_ && model_view_) {
        connect(plane_pose_widget_, &PlanePoseWidget::poseChanged, this, [this]() {
            if (model_view_ && plane_pose_widget_) {
                double pos_x, pos_y, pos_z, rot_x, rot_y, rot_z;
                plane_pose_widget_->getPose(pos_x, pos_y, pos_z, rot_x, rot_y, rot_z);
                model_view_->setWorkingPlanePose(pos_x, pos_y, pos_z, rot_x, rot_y, rot_z);
            }
        });
    }
    
    // 连接纹理列表信号
    if (texture_list_widget_) {
        connect(texture_list_widget_, &TextureListWidget::textureSelected, this, [this](int patch_id) {
            if (model_view_) {
                model_view_->selectPatch(patch_id);
            }
        });
        connect(texture_list_widget_, &TextureListWidget::addTextureRequested, this, &MainWindow::applyTextureToPatch);
        connect(texture_list_widget_, &TextureListWidget::removeTextureRequested, this, &MainWindow::removeTextureFromPatch);
        connect(texture_list_widget_, &TextureListWidget::removePathRequested, this, &MainWindow::removeSavedPathFromPatch);
        connect(texture_list_widget_, &TextureListWidget::editTextureRequested, this, &MainWindow::editTextureForPatch);
        connect(texture_list_widget_, &TextureListWidget::showDetailsRequested, this, &MainWindow::showTextureDetails);
    }
    
    // 连接UV校验信号
    if (uv_view_ && model_view_) {
        connect(uv_view_, &UVView::uvVertexClicked, this, [this](size_t vertex_index, double u, double v) {
            if (model_view_) {
                model_view_->highlightVertex(vertex_index);
            }
            // 在状态栏显示信息
            QString msg = QString("UV顶点 %1: (%.6f, %.6f)").arg(vertex_index).arg(u, 0, 'f', 6).arg(v, 0, 'f', 6);
            statusBar()->showMessage(msg, 3000);
        });
    }

    if (path_preview_ && model_view_) {
        connect(path_preview_, &PathPreview::highlightedSegmentsChanged, this, [this](const QVector<int>& segment_ids) {
            if (!model_view_) {
                return;
            }
            std::vector<int> ids;
            ids.reserve(static_cast<size_t>(segment_ids.size()));
            for (int id : segment_ids) {
                ids.push_back(id);
            }
            model_view_->setHighlightedPathSegments(ids);
            if (uv_view_) {
                uv_view_->setHighlightedPathSegments(ids);
            }
        });
        connect(path_preview_, &PathPreview::focusSegmentRequested, this, [this](int segment_id) {
            if (model_view_) {
                model_view_->focusPathSegment(segment_id);
            }
        });
        connect(path_preview_, &PathPreview::savePathRequested,
                this, &MainWindow::saveCurrentPathFromPreview);
    }
    
    setWindowTitle("NBCAM - v0.0.1");
    resize(1600, 900);
    updateMachiningUiState();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    // 创建主分割器
    main_splitter_ = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(main_splitter_);
    
    // 最左侧：纹理列表（占1/8）
    texture_list_widget_ = new TextureListWidget(main_splitter_);
    main_splitter_->addWidget(texture_list_widget_);
    
    // 中间：3D视图和UV视图（占6/8）
    QSplitter* left_splitter = new QSplitter(Qt::Vertical, main_splitter_);
    
    model_view_ = new ModelView(left_splitter);
    model_view_->setProcessingRegionVisible(true);

    uv_view_ = new UVView(left_splitter);
    
    left_splitter->addWidget(model_view_);
    left_splitter->addWidget(uv_view_);
    left_splitter->setStretchFactor(0, 2);
    left_splitter->setStretchFactor(1, 1);
    
    // 保存left_splitter的引用，用于控制UV视图的显示/隐藏
    left_splitter_ = left_splitter;
    
    // 右侧：参数面板、平面位姿、路径预览按钮（占1/8）
    QSplitter* right_splitter = new QSplitter(Qt::Vertical, main_splitter_);
    
    parameter_panel_ = new ParameterPanel(right_splitter);
    
    // 创建平面位姿控件
    plane_pose_widget_ = new PlanePoseWidget(right_splitter);

    QWidget* path_preview_button_container = new QWidget(right_splitter);
    QVBoxLayout* path_preview_button_layout = new QVBoxLayout(path_preview_button_container);
    path_preview_button_layout->setContentsMargins(8, 8, 8, 8);
    path_preview_button_ = new QPushButton("路径预览", path_preview_button_container);
    path_preview_button_->setMinimumHeight(36);
    path_preview_button_layout->addWidget(path_preview_button_);
    path_preview_button_layout->addStretch();
    path_preview_button_container->setMinimumHeight(58);
    path_preview_button_container->setMaximumHeight(100);

    path_preview_dialog_ = new QDialog(this);
    path_preview_dialog_->setWindowTitle("路径预览");
    path_preview_dialog_->setModal(false);
    path_preview_dialog_->resize(760, 640);
    QVBoxLayout* path_preview_dialog_layout = new QVBoxLayout(path_preview_dialog_);
    path_preview_dialog_layout->setContentsMargins(8, 8, 8, 8);
    path_preview_ = new PathPreview(path_preview_dialog_);
    path_preview_dialog_layout->addWidget(path_preview_);
    connect(path_preview_button_, &QPushButton::clicked, this, &MainWindow::openPathPreviewDialog);
    
    right_splitter->addWidget(parameter_panel_);
    right_splitter->addWidget(plane_pose_widget_);
    right_splitter->addWidget(path_preview_button_container);
    right_splitter->setStretchFactor(0, 2);
    right_splitter->setStretchFactor(1, 1);
    right_splitter->setStretchFactor(2, 0);
    
    main_splitter_->addWidget(left_splitter);
    main_splitter_->addWidget(right_splitter);
    
    // 设置比例：纹理列表1/8，中间视图6/8，右侧面板1/8
    main_splitter_->setStretchFactor(0, 1);   // 纹理列表
    main_splitter_->setStretchFactor(1, 6);   // 中间视图
    main_splitter_->setStretchFactor(2, 1);   // 右侧面板
    
    // 设置最小宽度
    texture_list_widget_->setMinimumWidth(150);
    right_splitter->setMinimumWidth(200);
    
    // 注意：初始大小将在showEvent中设置，因为此时窗口还没有显示，宽度可能不准确
}

void MainWindow::createMenus()
{
    // 文件菜单
    file_menu_ = menuBar()->addMenu("文件(&F)");
    
    open_model_action_ = new QAction("打开模型(&O)...", this);
    open_model_action_->setShortcut(QKeySequence::Open);
    connect(open_model_action_, &QAction::triggered, this, &MainWindow::openModel);
    file_menu_->addAction(open_model_action_);

    QAction* export_model_action = new QAction("导出模型(&E)...", this);
    connect(export_model_action, &QAction::triggered, this, &MainWindow::exportModel);
    file_menu_->addAction(export_model_action);
    
    file_menu_->addSeparator();
    
    execute_job_action_ = new QAction("执行任务(&R)", this);
    execute_job_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    execute_job_action_->setEnabled(false);
    connect(execute_job_action_, &QAction::triggered, this, &MainWindow::executeJob);
    file_menu_->addAction(execute_job_action_);
    
    file_menu_->addSeparator();
    
    exit_action_ = new QAction("退出(&X)", this);
    exit_action_->setShortcut(QKeySequence::Quit);
    connect(exit_action_, &QAction::triggered, this, &QWidget::close);
    file_menu_->addAction(exit_action_);
    
    // 纹理菜单已移除，纹理编辑器通过右键菜单访问
    
    // 视图菜单
    view_menu_ = menuBar()->addMenu("视图(&V)");
    
    wireframe_3d_action_ = new QAction("3D线框(&V)", this);
    wireframe_3d_action_->setCheckable(true);
    wireframe_3d_action_->setChecked(false);
    wireframe_3d_action_->setShortcut(QKeySequence(Qt::Key_V));
    connect(wireframe_3d_action_, &QAction::toggled, this, &MainWindow::toggleWireframe3D);
    view_menu_->addAction(wireframe_3d_action_);
    
    wireframe_uv_action_ = new QAction("UV线框(&U)", this);
    wireframe_uv_action_->setCheckable(true);
    wireframe_uv_action_->setChecked(false);
    connect(wireframe_uv_action_, &QAction::toggled, this, &MainWindow::toggleWireframeUV);
    view_menu_->addAction(wireframe_uv_action_);
    
    view_menu_->addSeparator();
    
    bounding_box_action_ = new QAction("显示包围盒(&B)", this);
    bounding_box_action_->setCheckable(true);
    bounding_box_action_->setChecked(false);
    connect(bounding_box_action_, &QAction::toggled, this, &MainWindow::toggleBoundingBox);
    view_menu_->addAction(bounding_box_action_);

    axes_action_ = new QAction("显示坐标系(&A)", this);
    axes_action_->setCheckable(true);
    axes_action_->setChecked(true);
    connect(axes_action_, &QAction::toggled, this, &MainWindow::toggleAxes);
    view_menu_->addAction(axes_action_);
    
    view_menu_->addSeparator();
    
    QAction* show_uv_view_action_ = new QAction("显示UV视图(&U)", this);
    show_uv_view_action_->setCheckable(true);
    show_uv_view_action_->setChecked(true);  // 默认显示
    connect(show_uv_view_action_, &QAction::toggled, this, &MainWindow::toggleUVView);
    view_menu_->addAction(show_uv_view_action_);
    
    QAction* uv_validation_action_ = new QAction("UV校验模式(&V)", this);
    uv_validation_action_->setCheckable(true);
    uv_validation_action_->setChecked(false);  // 默认关闭
    connect(uv_validation_action_, &QAction::toggled, this, &MainWindow::toggleUVValidation);
    view_menu_->addAction(uv_validation_action_);

    QMenu* color_mode_menu = view_menu_->addMenu("着色模式");
    QActionGroup* color_mode_group = new QActionGroup(this);
    color_mode_group->setExclusive(true);

    QAction* color_solid_action = new QAction("默认着色", this);
    color_solid_action->setCheckable(true);
    color_solid_action->setChecked(true);
    color_mode_group->addAction(color_solid_action);
    color_mode_menu->addAction(color_solid_action);
    connect(color_solid_action, &QAction::triggered, this, [this]() {
        if (model_view_) {
            model_view_->setSurfaceColorModeSolid();
        }
    });

    QAction* color_normal_action = new QAction("法向量着色", this);
    color_normal_action->setCheckable(true);
    color_mode_group->addAction(color_normal_action);
    color_mode_menu->addAction(color_normal_action);
    connect(color_normal_action, &QAction::triggered, this, [this]() {
        if (model_view_) {
            model_view_->setSurfaceColorModeNormal();
        }
    });

    QAction* color_curvature_action = new QAction("曲率着色", this);
    color_curvature_action->setCheckable(true);
    color_mode_group->addAction(color_curvature_action);
    color_mode_menu->addAction(color_curvature_action);
    connect(color_curvature_action, &QAction::triggered, this, [this]() {
        if (model_view_) {
            model_view_->setSurfaceColorModeCurvature();
        }
    });
    
    // 工具菜单
    tools_menu_ = menuBar()->addMenu("工具(&T)");
    
    
    import_svg_action_ = new QAction("导入SVG图案(&I)...", this);
    connect(import_svg_action_, &QAction::triggered, this, &MainWindow::importSVG);
    tools_menu_->addAction(import_svg_action_);
    
    tools_menu_->addSeparator();
    
    QAction* cluster_patches_action_ = new QAction("面片聚类(&C)...", this);
    connect(cluster_patches_action_, &QAction::triggered, this, &MainWindow::clusterPatches);
    tools_menu_->addAction(cluster_patches_action_);
    
    QAction* parameterize_patch_action_ = new QAction("参数化选中Patch(&P)", this);
    connect(parameterize_patch_action_, &QAction::triggered, this, &MainWindow::parameterizeSelectedPatch);
    tools_menu_->addAction(parameterize_patch_action_);
    
    // SVG路径规划功能已注释    // QAction* import_svg_to_patch_action_ = new QAction("导入SVG到Patch(&S)...", this);
    // connect(import_svg_to_patch_action_, &QAction::triggered, this, &MainWindow::importSVGToPatch);
    // tools_menu_->addAction(import_svg_to_patch_action_);
    
    QAction* apply_texture_to_patch_action_ = new QAction("应用SVG纹理到Patch(&T)...", this);
    connect(apply_texture_to_patch_action_, &QAction::triggered, this, &MainWindow::applyTextureToPatch);
    tools_menu_->addAction(apply_texture_to_patch_action_);

    QAction* adaptive_subdivide_action = new QAction("自适应均匀细分(&A)...", this);
    connect(adaptive_subdivide_action, &QAction::triggered, this, &MainWindow::adaptiveSubdivideAndExportStl);
    tools_menu_->addAction(adaptive_subdivide_action);

    QAction* model_offset_action = new QAction("模型偏置与加工区域(&O)...", this);
    connect(model_offset_action, &QAction::triggered, this, &MainWindow::openModelOffsetDialog);
    tools_menu_->addAction(model_offset_action);

    QMenu* path_planning_menu = tools_menu_->addMenu("路径规划(&P)");
    plan_path_action_ = new QAction("执行路径规划(&R)", this);
    plan_path_action_->setStatusTip("在已贴图的SVG区域按当前参数执行路径规划并在模型上绘制");
    connect(plan_path_action_, &QAction::triggered, this, &MainWindow::onPlanPath);
    path_planning_menu->addAction(plan_path_action_);
    
    tools_menu_->addSeparator();
    
    QAction* clear_patch_data_action_ = new QAction("清理面片数据(&C)", this);
    connect(clear_patch_data_action_, &QAction::triggered, this, &MainWindow::clearPatchData);
    tools_menu_->addAction(clear_patch_data_action_);

    tools_menu_->addSeparator();

    QAction* validate_example_01_action = new QAction("验证示例流程(0.1mm)", this);
    connect(validate_example_01_action, &QAction::triggered, this, [this]() { runValidationExample(0.1); });
    tools_menu_->addAction(validate_example_01_action);

    QAction* validate_example_02_action = new QAction("验证示例流程2(圆弧,HUST,0.01mm)", this);
    connect(validate_example_02_action, &QAction::triggered, this, [this]() {
        ValidationExampleOptions options;
        options.svg_relative_path = "docs/HUST.svg";
        options.strategy_override = "轴向往返填充(圆弧)";
        options.adaptive_target_triangles = 0;
        options.export_connection_frame_log = true;
        options.run_comm_probe_for_log = true;
        options.arc_center_x = 20.0;
        options.arc_center_z = -15.0;
        options.arc_radius = 20.0;
        runValidationExample(0.01, options);
    });
    tools_menu_->addAction(validate_example_02_action);

    QAction* validate_example_03_action = new QAction("验证示例流程3(HUST,0.01mm)", this);
    connect(validate_example_03_action, &QAction::triggered, this, [this]() {
        ValidationExampleOptions options;
        options.svg_relative_path = "docs/HUST.svg";
        runValidationExample(0.01, options);
    });
    tools_menu_->addAction(validate_example_03_action);

    // 连接菜单
    connection_menu_ = menuBar()->addMenu("连接(&C)");

    QAction* test_grpc_action = new QAction("测试gRPC通信", this);
    connect(test_grpc_action, &QAction::triggered, this, &MainWindow::testGrpcCommunication);
    connection_menu_->addAction(test_grpc_action);

    QAction* test_tcp_action = new QAction("测试TCP通信", this);
    connect(test_tcp_action, &QAction::triggered, this, &MainWindow::testTcpCommunication);
    connection_menu_->addAction(test_tcp_action);

    QAction* test_serial_action = new QAction("测试串口通信", this);
    connect(test_serial_action, &QAction::triggered, this, &MainWindow::testSerialCommunication);
    connection_menu_->addAction(test_serial_action);

    connection_menu_->addSeparator();

    QAction* connect_action = new QAction("连接", this);
    connect(connect_action, &QAction::triggered, this, &MainWindow::connectCommunication);
    connection_menu_->addAction(connect_action);

    device_board_menu_ = menuBar()->addMenu("设备与板卡(&B)");

    QAction* device_settings_action = new QAction("设备设置", this);
    connect(device_settings_action, &QAction::triggered, this, [this]() {
        configureDeviceSettings();
    });
    device_board_menu_->addAction(device_settings_action);

    QAction* board_settings_action = new QAction("板卡设置", this);
    connect(board_settings_action, &QAction::triggered, this, [this]() {
        configureBoardSettings();
    });
    device_board_menu_->addAction(board_settings_action);

    QAction* laser_settings_action = new QAction("激光器设置", this);
    connect(laser_settings_action, &QAction::triggered, this, [this]() {
        configureLaserSettings();
    });
    device_board_menu_->addAction(laser_settings_action);

    // 手动操作菜单（占位）
    manual_menu_ = menuBar()->addMenu("手动操作(&O)");
    QAction* open_manual_panel_action = new QAction("打开手动操作面板", this);
    connect(open_manual_panel_action, &QAction::triggered, this, &MainWindow::openManualOperationPanel);
    manual_menu_->addAction(open_manual_panel_action);

    // 测量菜单
    measure_menu_ = menuBar()->addMenu("测量(&M)");

    QAction* measure_area_action = new QAction("计算面积", this);
    connect(measure_area_action, &QAction::triggered, this, &MainWindow::measureArea);
    measure_menu_->addAction(measure_area_action);

    QAction* measure_point_action = new QAction("计算曲面点坐标", this);
    connect(measure_point_action, &QAction::triggered, this, &MainWindow::measureSurfacePointCoordinate);
    measure_menu_->addAction(measure_point_action);

    QAction* measure_line_action = new QAction("计算两点直线距离", this);
    connect(measure_line_action, &QAction::triggered, this, &MainWindow::measureLineDistance);
    measure_menu_->addAction(measure_line_action);

    QAction* measure_geo_action = new QAction("计算两点曲面测地线距离", this);
    connect(measure_geo_action, &QAction::triggered, this, &MainWindow::measureGeodesicDistance);
    measure_menu_->addAction(measure_geo_action);

    // 日志菜单
    log_menu_ = menuBar()->addMenu("日志(&L)");
    QAction* open_log_action = new QAction("打开日志", this);
    connect(open_log_action, &QAction::triggered, this, &MainWindow::openCurrentLog);
    log_menu_->addAction(open_log_action);

    log_menu_->addSeparator();

    QAction* clear_current_log_action = new QAction("清理当前日志", this);
    connect(clear_current_log_action, &QAction::triggered, this, &MainWindow::clearCurrentLog);
    log_menu_->addAction(clear_current_log_action);

    QAction* clear_all_logs_action = new QAction("清理所有日志", this);
    connect(clear_all_logs_action, &QAction::triggered, this, &MainWindow::clearAllLogs);
    log_menu_->addAction(clear_all_logs_action);

    createLanguageMenu();
    createMachiningControlWidget();
    
    // 帮助菜单
    help_menu_ = menuBar()->addMenu("帮助(&H)");

    QAction* cli_mode_action = new QAction("命令行模式(&C)...", this);
    cli_mode_action->setStatusTip("启动NBCAM命令行模式，通过指令执行模型加载、SVG导入和路径规划");
    connect(cli_mode_action, &QAction::triggered, this, &MainWindow::openCommandLineMode);
    help_menu_->addAction(cli_mode_action);

    help_menu_->addSeparator();
    
    about_action_ = new QAction("关于(&A)...", this);
    connect(about_action_, &QAction::triggered, this, &MainWindow::about);
    help_menu_->addAction(about_action_);
}

void MainWindow::createMachiningControlWidget()
{
    if (machining_control_widget_) {
        return;
    }

    machining_control_widget_ = new QWidget(menuBar());
    QHBoxLayout* layout = new QHBoxLayout(machining_control_widget_);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(2);

    auto makeButton = [this, layout](const QString& icon_path,
                                     const QString& tool_tip,
                                     bool checkable) {
        QToolButton* button = new QToolButton(machining_control_widget_);
        button->setAutoRaise(true);
        button->setIcon(QIcon(icon_path));
        button->setIconSize(QSize(18, 18));
        button->setToolTip(tool_tip);
        button->setCheckable(checkable);
        layout->addWidget(button);
        return button;
    };

    machining_start_button_ = makeButton(":/view_icons/start.svg", "开始加工", false);
    machining_pause_button_ = makeButton(":/view_icons/pause.svg", "暂停", false);
    machining_stop_button_ = makeButton(":/view_icons/stop.svg", "急停", false);
    machining_refresh_button_ = makeButton(":/view_icons/refresh.svg", "刷新至起始数据帧并回归起点", false);
    machining_simulate_button_ = makeButton(":/view_icons/simulate.svg", "模拟加工", true);

    connect(machining_start_button_, &QToolButton::clicked, this, &MainWindow::onStartMachining);
    connect(machining_pause_button_, &QToolButton::clicked, this, &MainWindow::onPauseMachining);
    connect(machining_stop_button_, &QToolButton::clicked, this, &MainWindow::onEmergencyStop);
    connect(machining_refresh_button_, &QToolButton::clicked, this, &MainWindow::onRefreshMachining);
    connect(machining_simulate_button_, &QToolButton::toggled, this, &MainWindow::onSimulationToggled);

    menuBar()->setCornerWidget(machining_control_widget_, Qt::TopRightCorner);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage("就绪");
}

void MainWindow::applyHighlightedSegmentIds(const QSet<int>& segment_ids)
{
    if (path_preview_) {
        path_preview_->setSelectedSegmentIds(segment_ids);
    }

    std::vector<int> ids;
    ids.reserve(static_cast<size_t>(segment_ids.size()));
    for (int id : segment_ids) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());

    if (model_view_) {
        model_view_->setHighlightedPathSegments(ids);
    }
    if (uv_view_) {
        uv_view_->setHighlightedPathSegments(ids);
    }
}

void MainWindow::applyMachiningProgressHighlight(int current_segment_id)
{
    if (path_preview_) {
        QSet<int> current_selection;
        if (current_segment_id >= 0) {
            current_selection.insert(current_segment_id);
        }
        path_preview_->setSelectedSegmentIds(current_selection, false);
    }

    if (current_segment_id >= 0) {
        if (model_view_) {
            model_view_->addHighlightedPathSegment(current_segment_id);
        }
        if (uv_view_) {
            uv_view_->addHighlightedPathSegment(current_segment_id);
        }
    }
}

void MainWindow::resetMachiningProgress()
{
    machining_batch_index_ = 0;
    machining_completed_segment_ids_.clear();
    emergency_stop_requested_ = false;
    machining_refresh_available_ = false;
    machining_execution_clock_started_ = false;
    machining_execution_start_time_ = std::chrono::steady_clock::time_point{};
    machining_next_segment_to_mark_ = 0;
    applyHighlightedSegmentIds(QSet<int>{});
}

void MainWindow::stopMachiningSession(bool reset_to_start,
                                      bool disconnect_socket,
                                      bool keep_simulate_toggle)
{
    if (machining_timer_) {
        machining_timer_->stop();
    }
    if (machining_progress_timer_) {
        machining_progress_timer_->stop();
    }

    machining_run_state_ = MachiningRunState::Idle;

    if (disconnect_socket) {
        if (machining_data_buffer_) {
            machining_data_buffer_->stopTcpThread();
        }
        machining_data_buffer_ = std::make_unique<nbcam::DataBuffer>();
    }

    if (machining_log_level_overridden_) {
        spdlog::set_level(machining_previous_log_level_);
        spdlog::flush_on(machining_previous_log_level_);
        machining_log_level_overridden_ = false;
    }

    if (reset_to_start) {
        resetMachiningProgress();
    }

    if (!keep_simulate_toggle && machining_simulate_button_ && machining_simulate_button_->isChecked()) {
        ignore_simulation_toggle_signal_ = true;
        machining_simulate_button_->setChecked(false);
        ignore_simulation_toggle_signal_ = false;
        is_simulation_mode_ = false;
    }

    updateMachiningUiState();
}

bool MainWindow::validateActiveJobForMachining(QString* out_error) const
{
    if (!controller_) {
        if (out_error) {
            *out_error = "控制器未初始化。";
        }
        return false;
    }

    nbcam::LaserJob* const job = controller_->getCurrentJob();
    if (!job || !job->isValid()) {
        if (out_error) {
            *out_error = "当前没有处理好的数据帧。请先完成路径规划。";
        }
        return false;
    }

    if (job->segments.empty()) {
        if (out_error) {
            *out_error = "当前任务没有可发送的路径段。";
        }
        return false;
    }

    return true;
}

bool MainWindow::validateTcpTargetForMachining(QString* out_error) const
{
    if (comm_config_.executor_backend == ExecutorBackend::RTC6) {
        if (out_error) {
            *out_error = "当前设备配置为 RTC-6，不支持 TCP 数据帧下发。";
        }
        return false;
    }

    if (comm_config_.tcp_host.trimmed().isEmpty() || comm_config_.tcp_port <= 0 || comm_config_.tcp_port > 65535) {
        if (out_error) {
            *out_error = QString("TCP 目标无效: %1:%2").arg(comm_config_.tcp_host).arg(comm_config_.tcp_port);
        }
        return false;
    }
    return true;
}

bool MainWindow::prepareMachiningPlan(QString* out_error)
{
    machining_plan_.batches.clear();
    machining_plan_.all_segment_ids.clear();
    machining_plan_.segment_end_times_us.clear();

    nbcam::LaserJob* const job = controller_ ? controller_->getCurrentJob() : nullptr;
    if (!job || !job->isValid()) {
        if (out_error) {
            *out_error = "当前没有处理好的数据帧。";
        }
        return false;
    }

    nbcam::BoardExecutor executor;
    nbcam::BoardExecutor::MachineConfig machine_config;
    machine_config.transport.host = comm_config_.tcp_host.toStdString();
    machine_config.transport.port = static_cast<uint16_t>(comm_config_.tcp_port);
    machine_config.transport.connect_retry_ms = comm_config_.tcp_connect_retry_ms;
    machine_config.transport.max_connect_attempts = std::max(1, comm_config_.tcp_max_connect_attempts);
    machine_config.transport.io_timeout_ms = comm_config_.tcp_io_timeout_ms;
    machine_config.apply_job_transform = comm_config_.apply_job_transform;
    machine_config.dry_run_only = true;
    machine_config.live_laser_enabled = false;
    machine_config.axis_mode = (comm_config_.axis_mode == AxisMode::FiveAxis)
        ? nbcam::BoardExecutor::AxisMode::FiveAxis
        : nbcam::BoardExecutor::AxisMode::ThreeAxis;
    machine_config.x_axis.offset_mm = comm_config_.x_offset_mm;
    machine_config.x_axis.limit_min_mm = comm_config_.x_min_mm;
    machine_config.x_axis.limit_max_mm = comm_config_.x_max_mm;
    machine_config.y_axis.offset_mm = comm_config_.y_offset_mm;
    machine_config.y_axis.limit_min_mm = comm_config_.y_min_mm;
    machine_config.y_axis.limit_max_mm = comm_config_.y_max_mm;
    machine_config.z_axis.offset_mm = comm_config_.z_offset_mm;
    machine_config.z_axis.limit_min_mm = comm_config_.z_min_mm;
    machine_config.z_axis.limit_max_mm = comm_config_.z_max_mm;
    machine_config.a_axis.offset_mm = comm_config_.a_offset_mm;
    machine_config.a_axis.limit_min_mm = comm_config_.a_min_mm;
    machine_config.a_axis.limit_max_mm = comm_config_.a_max_mm;
    machine_config.b_axis.offset_mm = comm_config_.b_offset_mm;
    machine_config.b_axis.limit_min_mm = comm_config_.b_min_mm;
    machine_config.b_axis.limit_max_mm = comm_config_.b_max_mm;
    machine_config.delay.jump_speed_mm_s = comm_config_.jump_speed_mm_s;
    machine_config.delay.laser_on_delay_us = comm_config_.laser_on_delay_us;
    machine_config.delay.laser_off_delay_us = comm_config_.laser_off_delay_us;
    machine_config.delay.mark_delay_us = comm_config_.mark_delay_us;
    machine_config.delay.jump_delay_us = comm_config_.jump_delay_us;
    machine_config.delay.polygon_delay_us = comm_config_.polygon_delay_us;
    executor.setMachineConfig(machine_config);

    std::string validation_error;
    nbcam::BoardExecutor::PreflightReport report;
    if (!executor.validateJob(*job, &validation_error, &report)) {
        if (out_error) {
            *out_error = QString::fromStdString(validation_error);
        }
        return false;
    }

    const auto appendBatch = [this](const QByteArray& payload,
                                    int segment_id,
                                    int duration_us,
                                    bool mark_output,
                                    int frame_count) {
        if (payload.isEmpty() || frame_count <= 0) {
            return;
        }
        MachiningBatch batch;
        batch.payload = payload;
        batch.segment_id = segment_id;
        batch.duration_us = is_simulation_mode_ ? 1 : std::max(1, duration_us);
        batch.mark_output = mark_output;
        batch.frame_count = frame_count;
        machining_plan_.batches.push_back(batch);
        if (segment_id >= 0) {
            machining_plan_.all_segment_ids.insert(segment_id);
        }
    };

    const auto appendControlBatch = [&appendBatch](const QByteArray& payload) {
        const int frame_count = std::max(1, static_cast<int>(payload.size() / 16));
        appendBatch(payload, -1, frame_count * 10, false, frame_count);
    };

    const auto appendMachineConfigBatch = [&appendControlBatch](const QByteArray& payload) {
        if (!payload.isEmpty()) {
            appendControlBatch(payload);
        }
    };

    auto buildSegmentBatch = [this](const nbcam::LaserJob& source_job,
                                    const nbcam::PathSegment& segment,
                                    const nbcam::BoardExecutor::MachineConfig& source_machine_config,
                                    QString* build_error,
                                    int* out_frame_count,
                                    int* out_duration_us,
                                    bool* out_mark_output) -> QByteArray {
        QByteArray payload;
        int frame_count = 0;
        bool saw_mark_output = false;

        const auto appendFrameBytes = [&payload, &frame_count, &saw_mark_output](const QByteArray& frame_bytes,
                                                                                  bool mark_output) {
            payload.append(frame_bytes);
            frame_count += std::max(1, static_cast<int>(frame_bytes.size() / 16));
            saw_mark_output = saw_mark_output || mark_output;
        };

        auto appendDelayFrames = [this, &appendFrameBytes](const MachineEncodedPoint& point,
                                                           int delay_us,
                                                           int delay_on_us) {
            constexpr int kTickUs = 10;
            const int total_ticks = std::max(0, delay_us / kTickUs);
            const int mark_ticks = std::min(total_ticks, std::max(0, delay_on_us) / kTickUs);
            for (int i = 0; i < total_ticks; ++i) {
                const bool mark = machining_laser_output_enabled_ && (i < mark_ticks);
                appendFrameBytes(packFrameBytes(point.b,
                                                point.a,
                                                point.z,
                                                point.y,
                                                point.x,
                                                mark ? 0x00FF : 0x0000,
                                                0,
                                                0),
                                 mark);
            }
        };

        auto appendInterpolatedFrames = [this, &appendFrameBytes](const MachineEncodedPoint& start,
                                                                  const MachineEncodedPoint& end,
                                                                  double speed_mm_s,
                                                                  bool mark_segment,
                                                                  int laser_on_delay_us,
                                                                  int* mark_elapsed_us) {
            const int sample_count = computeInterpolationSamplesForMachining(start.x,
                                                                             start.y,
                                                                             start.z,
                                                                             end.x,
                                                                             end.y,
                                                                             end.z,
                                                                             speed_mm_s);
            int local_mark_elapsed_us = mark_elapsed_us ? *mark_elapsed_us : 0;
            for (int i = 1; i <= sample_count; ++i) {
                const uint16_t x = interpolateAxisForMachining(start.x, end.x, i, sample_count);
                const uint16_t y = interpolateAxisForMachining(start.y, end.y, i, sample_count);
                const uint16_t z = interpolateAxisForMachining(start.z, end.z, i, sample_count);
                const uint16_t a = interpolateAxisForMachining(start.a, end.a, i, sample_count);
                const uint16_t b = interpolateAxisForMachining(start.b, end.b, i, sample_count);
                bool mark = false;
                if (mark_segment) {
                    mark = machining_laser_output_enabled_ && (local_mark_elapsed_us >= laser_on_delay_us);
                    local_mark_elapsed_us += 10;
                }
                appendFrameBytes(packFrameBytes(b,
                                                a,
                                                z,
                                                y,
                                                x,
                                                mark ? 0x00FF : 0x0000,
                                                0,
                                                0),
                                 mark);
            }
            if (mark_elapsed_us) {
                *mark_elapsed_us = local_mark_elapsed_us;
            }
        };

        const nbcam::ProcessParams* default_params = segment.params_override ? segment.params_override.get()
                                                                             : &source_job.process_defaults;
        MachineEncodedPoint prev_encoded{};
        bool has_prev = false;
        int mark_elapsed_us = 0;

        for (size_t point_index = 0; point_index < segment.points.size(); ++point_index) {
            const auto& point = segment.points[point_index];
            MachineEncodedPoint current_encoded{};
            QString encode_error;
            if (!encodePointForMachining(source_job,
                                         point,
                                         source_machine_config,
                                         static_cast<size_t>(segment.id),
                                         point_index,
                                         &current_encoded,
                                         &encode_error)) {
                if (build_error) {
                    *build_error = encode_error;
                }
                return {};
            }

            const nbcam::ProcessParams* point_params = point.params_override ? point.params_override.get() : default_params;
            const bool point_is_jump = (segment.type == nbcam::SegmentType::JUMP) || (point.laser == 0);
            const int laser_on_delay_us = resolveLaserOnDelayMachining(point_params, comm_config_.laser_on_delay_us);

            if (!has_prev) {
                appendFrameBytes(packFrameBytes(current_encoded.b,
                                                current_encoded.a,
                                                current_encoded.z,
                                                current_encoded.y,
                                                current_encoded.x,
                                                0x0000,
                                                0,
                                                0),
                                 false);
                prev_encoded = current_encoded;
                has_prev = true;
                mark_elapsed_us = 0;
                continue;
            }

            if (point_is_jump) {
                const double jump_speed = std::max(1.0, static_cast<double>(comm_config_.jump_speed_mm_s));
                appendInterpolatedFrames(prev_encoded, current_encoded, jump_speed, false, 0, nullptr);
            } else {
                const double mark_speed = resolveSpeedMmSMachining(point_params, default_params->speed_mm_s);
                appendInterpolatedFrames(prev_encoded, current_encoded, mark_speed, true, laser_on_delay_us, &mark_elapsed_us);
            }

            prev_encoded = current_encoded;
        }

        if (has_prev) {
            if (segment.type == nbcam::SegmentType::JUMP) {
                appendDelayFrames(prev_encoded, comm_config_.jump_delay_us, 0);
            } else {
                const nbcam::ProcessParams* tail_params =
                    segment.points.back().params_override ? segment.points.back().params_override.get() : default_params;
                const int laser_off_delay_us = resolveLaserOffDelayMachining(tail_params, comm_config_.laser_off_delay_us);
                appendDelayFrames(prev_encoded, comm_config_.mark_delay_us, laser_off_delay_us);
            }
        }
        if (out_frame_count) {
            *out_frame_count = frame_count;
        }
        if (out_duration_us) {
            *out_duration_us = frame_count * 10;
        }
        if (out_mark_output) {
            *out_mark_output = saw_mark_output;
        }
        return payload;
    };

    const QByteArray legacy_warmup_payload =
        packFrameBytes(0, 0, 0, 0, 0, 0xFF00, 0, 0) +
        packFrameBytes(0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x0000, 0, 0) +
        packFrameBytes(0, 0, 0, 0, 0, 0x1100, 0, 0);

    appendControlBatch(legacy_warmup_payload);
    appendControlBatch(legacy_warmup_payload);
    appendControlBatch(packFrameBytes(0, 0, 0, 0, 0, 0xFF00, 0, 0));

    for (const auto& segment : job->segments) {
        int frame_count = 0;
        int duration_us = 0;
        bool mark_output = false;
        QString build_error;
        const QByteArray payload = buildSegmentBatch(*job,
                                                     segment,
                                                     machine_config,
                                                     &build_error,
                                                     &frame_count,
                                                     &duration_us,
                                                     &mark_output);
        if (!build_error.isEmpty()) {
            if (out_error) {
                *out_error = build_error;
            }
            machining_plan_.batches.clear();
            machining_plan_.all_segment_ids.clear();
            return false;
        }
        appendBatch(payload, segment.id, duration_us, mark_output, frame_count);
    }

    appendControlBatch(packFrameBytes(0, 0, 0, 0, 0, 0x1100, 0, 0));

    if (machining_plan_.batches.isEmpty()) {
        if (out_error) {
            *out_error = "没有生成可发送的数据帧。";
        }
        return false;
    }

    qint64 elapsed_us = 0;
    for (const auto& batch : machining_plan_.batches) {
        elapsed_us += static_cast<qint64>(std::max(1, batch.duration_us));
        if (batch.segment_id >= 0) {
            machining_plan_.segment_end_times_us.push_back({batch.segment_id, elapsed_us});
        }
    }

    int total_frames = 0;
    for (const auto& batch : machining_plan_.batches) {
        total_frames += batch.frame_count;
    }

    spdlog::info("MachiningPlan prepared: batches={}, frames={}, segments={}, mode={}, laser_output={}, segment_feedback={}, simulated_frame_duration={}us, tcp={}:{}, delays(on/off/mark/jump/polygon)={}/{}/{}/{}/{}us, jump_speed={}mm/s",
                 machining_plan_.batches.size(),
                 total_frames,
                 machining_plan_.all_segment_ids.size(),
                 is_simulation_mode_ ? "simulate" : "live",
                 machining_laser_output_enabled_ ? "on" : "off",
                 machining_segment_feedback_enabled_ ? "on" : "off",
                 is_simulation_mode_ ? 1 : 10,
                 comm_config_.tcp_host.toStdString(),
                 comm_config_.tcp_port,
                 comm_config_.laser_on_delay_us,
                 comm_config_.laser_off_delay_us,
                 comm_config_.mark_delay_us,
                 comm_config_.jump_delay_us,
                 comm_config_.polygon_delay_us,
                 comm_config_.jump_speed_mm_s);

    return true;
}

bool MainWindow::ensureMachiningTransportConnected(QString* out_error)
{
    if (!machining_data_buffer_) {
        if (out_error) {
            *out_error = "双缓冲发送器未初始化。";
        }
        return false;
    }

    if (machining_data_buffer_->isTcpConnected()) {
        return true;
    }

    nbcam::TcpSocket::Config transport_config;
    transport_config.host = comm_config_.tcp_host.toStdString();
    transport_config.port = static_cast<uint16_t>(comm_config_.tcp_port);
    transport_config.connect_retry_ms = comm_config_.tcp_connect_retry_ms;
    transport_config.max_connect_attempts = std::max(1, comm_config_.tcp_max_connect_attempts);
    transport_config.io_timeout_ms = comm_config_.tcp_io_timeout_ms;
    machining_data_buffer_->setTransportConfig(transport_config);
    machining_data_buffer_->startTcpThread();

    const int max_attempts = std::max(1, comm_config_.tcp_max_connect_attempts);
    const int retry_ms = std::max(1, comm_config_.tcp_connect_retry_ms);
    const int io_timeout_ms = std::max(1, comm_config_.tcp_io_timeout_ms);
    const int wait_budget_ms = std::max(io_timeout_ms, max_attempts * (retry_ms + io_timeout_ms) + retry_ms);
    const auto deadline = QDeadlineTimer(wait_budget_ms);
    while (!deadline.hasExpired()) {
        if (machining_data_buffer_->isTcpConnected()) {
            return true;
        }
        if (!machining_data_buffer_->isTcpThreadRunning()) {
            break;
        }
        QThread::msleep(10);
    }

    if (out_error) {
        const QString feedback = machining_data_buffer_
            ? QString::fromStdString(machining_data_buffer_->getLastTransportFeedback())
            : QString();
        *out_error = QString("双缓冲TCP连接失败，已重试%1次: %2:%3%4")
                         .arg(max_attempts)
                         .arg(comm_config_.tcp_host)
                         .arg(comm_config_.tcp_port)
                         .arg(feedback.isEmpty() ? QString() : QString("，反馈=%1").arg(feedback));
    }
    return false;
}

bool MainWindow::sendCurrentMachiningFrame(QString* out_error)
{
    if (machining_batch_index_ < 0 || machining_batch_index_ >= machining_plan_.batches.size()) {
        if (out_error) {
            *out_error = "当前 segment 批次索引越界。";
        }
        return false;
    }

    if (!ensureMachiningTransportConnected(out_error)) {
        return false;
    }

    const MachiningBatch& batch = machining_plan_.batches.at(machining_batch_index_);
    machining_data_buffer_->appendBytes(batch.payload.constData(), static_cast<size_t>(batch.payload.size()));
    machining_data_buffer_->forceFill();
    if (!machining_segment_feedback_enabled_) {
        return true;
    }
    if (!machining_data_buffer_->waitUntilIdle(std::chrono::milliseconds(std::max(1000, comm_config_.tcp_io_timeout_ms * 10)))) {
        if (out_error) {
            const QString feedback = QString::fromStdString(machining_data_buffer_->getLastTransportFeedback());
            const QString state = QString::fromStdString(machining_data_buffer_->describeState());
            *out_error = QString("segment 数据已写入缓冲，但发送线程未在超时时间内排空。状态=%1%2")
                             .arg(state)
                             .arg(feedback.isEmpty() ? QString() : QString("，反馈=%1").arg(feedback));
        }
        return false;
    }
    return true;
}

bool MainWindow::sendMachiningPlanContinuous(QString* out_error)
{
    if (!ensureMachiningTransportConnected(out_error)) {
        return false;
    }
    if (!machining_data_buffer_) {
        if (out_error) {
            *out_error = "双缓冲发送器未初始化。";
        }
        return false;
    }

    qint64 total_bytes = 0;
    qint64 total_frames = 0;
    for (const MachiningBatch& batch : machining_plan_.batches) {
        if (emergency_stop_requested_ || machining_run_state_ != MachiningRunState::Running) {
            if (out_error) {
                *out_error = "加工已被用户中断。";
            }
            return false;
        }
        if (batch.payload.isEmpty()) {
            continue;
        }
        machining_data_buffer_->appendBytes(batch.payload.constData(), static_cast<size_t>(batch.payload.size()));
        total_bytes += batch.payload.size();
        total_frames += batch.frame_count;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    }
    machining_data_buffer_->forceFill();

    spdlog::info("Machining continuous send queued: batches={}, bytes={}, frames={}, segment_feedback=off",
                 machining_plan_.batches.size(),
                 total_bytes,
                 total_frames);
    return true;
}

bool MainWindow::sendMachiningResetToStartFrame(QString* out_error)
{
    nbcam::LaserJob* const job = controller_ ? controller_->getCurrentJob() : nullptr;
    if (!job || !job->isValid()) {
        if (out_error) {
            *out_error = "当前没有可复位的数据帧。";
        }
        return false;
    }

    const nbcam::PathSegment* start_segment = nullptr;
    const nbcam::PathPoint* start_point = nullptr;
    size_t start_segment_index = 0;
    for (size_t segment_index = 0; segment_index < job->segments.size(); ++segment_index) {
        const auto& segment = job->segments[segment_index];
        if (!segment.points.empty()) {
            start_segment = &segment;
            start_point = &segment.points.front();
            start_segment_index = segment_index;
            break;
        }
    }

    if (!start_segment || !start_point) {
        if (out_error) {
            *out_error = "当前任务没有可回归的起始路径点。";
        }
        return false;
    }

    nbcam::BoardExecutor::MachineConfig machine_config;
    machine_config.transport.host = comm_config_.tcp_host.toStdString();
    machine_config.transport.port = static_cast<uint16_t>(comm_config_.tcp_port);
    machine_config.transport.connect_retry_ms = comm_config_.tcp_connect_retry_ms;
    machine_config.transport.max_connect_attempts = std::max(1, comm_config_.tcp_max_connect_attempts);
    machine_config.transport.io_timeout_ms = comm_config_.tcp_io_timeout_ms;
    machine_config.apply_job_transform = comm_config_.apply_job_transform;
    machine_config.dry_run_only = true;
    machine_config.live_laser_enabled = false;
    machine_config.axis_mode = (comm_config_.axis_mode == AxisMode::FiveAxis)
        ? nbcam::BoardExecutor::AxisMode::FiveAxis
        : nbcam::BoardExecutor::AxisMode::ThreeAxis;
    machine_config.x_axis.offset_mm = comm_config_.x_offset_mm;
    machine_config.x_axis.limit_min_mm = comm_config_.x_min_mm;
    machine_config.x_axis.limit_max_mm = comm_config_.x_max_mm;
    machine_config.y_axis.offset_mm = comm_config_.y_offset_mm;
    machine_config.y_axis.limit_min_mm = comm_config_.y_min_mm;
    machine_config.y_axis.limit_max_mm = comm_config_.y_max_mm;
    machine_config.z_axis.offset_mm = comm_config_.z_offset_mm;
    machine_config.z_axis.limit_min_mm = comm_config_.z_min_mm;
    machine_config.z_axis.limit_max_mm = comm_config_.z_max_mm;
    machine_config.a_axis.offset_mm = comm_config_.a_offset_mm;
    machine_config.a_axis.limit_min_mm = comm_config_.a_min_mm;
    machine_config.a_axis.limit_max_mm = comm_config_.a_max_mm;
    machine_config.b_axis.offset_mm = comm_config_.b_offset_mm;
    machine_config.b_axis.limit_min_mm = comm_config_.b_min_mm;
    machine_config.b_axis.limit_max_mm = comm_config_.b_max_mm;

    MachineEncodedPoint encoded_start{};
    QString encode_error;
    if (!encodePointForMachining(*job,
                                 *start_point,
                                 machine_config,
                                 start_segment_index,
                                 0,
                                 &encoded_start,
                                 &encode_error)) {
        if (out_error) {
            *out_error = encode_error;
        }
        return false;
    }

    if (!ensureMachiningTransportConnected(out_error)) {
        return false;
    }

    const QByteArray payload =
        packFrameBytes(0, 0, 0, 0, 0, 0xFF00, 0, 0) +
        packFrameBytes(encoded_start.b,
                       encoded_start.a,
                       encoded_start.z,
                       encoded_start.y,
                       encoded_start.x,
                       0x0000,
                       0,
                       0) +
        packFrameBytes(0, 0, 0, 0, 0, 0x1100, 0, 0);

    machining_data_buffer_->appendBytes(payload.constData(), static_cast<size_t>(payload.size()));
    machining_data_buffer_->forceFill();
    if (!machining_data_buffer_->waitUntilIdle(std::chrono::milliseconds(std::max(1000, comm_config_.tcp_io_timeout_ms * 10)))) {
        if (out_error) {
            const QString feedback = QString::fromStdString(machining_data_buffer_->getLastTransportFeedback());
            const QString state = QString::fromStdString(machining_data_buffer_->describeState());
            *out_error = QString("回起点数据帧已写入本地缓冲，但发送线程未在超时时间内排空。状态=%1%2")
                             .arg(state)
                             .arg(feedback.isEmpty() ? QString() : QString("，反馈=%1").arg(feedback));
        }
        return false;
    }

    spdlog::info("Machining refresh sent reset-to-start frame: segment_id={}, point=({},{},{},{},{})",
                 start_segment->id,
                 encoded_start.x,
                 encoded_start.y,
                 encoded_start.z,
                 encoded_start.a,
                 encoded_start.b);
    return true;
}

void MainWindow::updateMachiningUiState()
{
    if (machining_start_button_) {
        const bool has_job = controller_ && controller_->getCurrentJob() && controller_->getCurrentJob()->isValid();
        machining_start_button_->setEnabled(has_job);
    }
    if (machining_pause_button_) {
        machining_pause_button_->setEnabled(machining_run_state_ == MachiningRunState::Running);
    }
    if (machining_stop_button_) {
        machining_stop_button_->setEnabled(machining_run_state_ != MachiningRunState::Idle || machining_batch_index_ > 0);
    }
    if (machining_refresh_button_) {
        const bool can_refresh =
            machining_run_state_ == MachiningRunState::Paused ||
            (machining_run_state_ == MachiningRunState::Idle && machining_refresh_available_);
        machining_refresh_button_->setEnabled(can_refresh);
    }
    if (machining_simulate_button_) {
        machining_simulate_button_->setEnabled(machining_run_state_ == MachiningRunState::Idle);
    }
}

void MainWindow::onStartMachining()
{
    QString error;
    if (!validateActiveJobForMachining(&error)) {
        QMessageBox::warning(this, "开始加工", error);
        return;
    }

    if (!is_simulation_mode_ && !currentModelFitsProcessingRegion(&error)) {
        QMessageBox::warning(this,
                             "开始加工",
                             QString("当前模型仍超出加工区域，禁止真实下发。\n\n%1").arg(error));
        return;
    }

    if (!is_simulation_mode_ && !validateTcpTargetForMachining(&error)) {
        QMessageBox::warning(this, "开始加工", error);
        return;
    }

    if (machining_run_state_ == MachiningRunState::Paused) {
        machining_run_state_ = MachiningRunState::Running;
        machining_refresh_available_ = false;
        spdlog::info("Machining resume: mode={}, laser_output={}, next_batch={}/{}",
                     is_simulation_mode_ ? "simulate" : "live",
                     machining_laser_output_enabled_ ? "on" : "off",
                     machining_batch_index_ + 1,
                     machining_plan_.batches.size());
        updateMachiningUiState();
        processMachiningStep();
        return;
    }

    bool laser_output_enabled = false;
    bool segment_feedback_enabled = true;
    if (!is_simulation_mode_) {
        if (!queryMachiningStartOptions(this, "开始加工", &laser_output_enabled, &segment_feedback_enabled)) {
            statusBar()->showMessage("已取消开始加工", 3000);
            return;
        }
    }
    machining_laser_output_enabled_ = laser_output_enabled;
    machining_segment_feedback_enabled_ = segment_feedback_enabled;

    stopMachiningSession(true, true, true);
    if (!prepareMachiningPlan(&error)) {
        QMessageBox::warning(this, "开始加工", error);
        return;
    }

    if (!is_simulation_mode_ && !ensureMachiningTransportConnected(&error)) {
        QMessageBox::warning(this, "开始加工", error);
        return;
    }

    machining_run_state_ = MachiningRunState::Running;
    emergency_stop_requested_ = false;
    machining_refresh_available_ = false;
    if (!is_simulation_mode_ && !machining_segment_feedback_enabled_) {
        machining_previous_log_level_ = spdlog::get_level();
        machining_log_level_overridden_ = true;
        spdlog::set_level(spdlog::level::warn);
        spdlog::flush_on(spdlog::level::warn);
    }
    spdlog::info("Machining start: mode={}, laser_output={}, segment_feedback={}, frames={}, segments={}",
                 is_simulation_mode_ ? "simulate" : "live",
                 machining_laser_output_enabled_ ? "on" : "off",
                 machining_segment_feedback_enabled_ ? "on" : "off",
                 machining_plan_.batches.size(),
                 machining_plan_.all_segment_ids.size());
    updateMachiningUiState();
    if (!is_simulation_mode_ && !machining_segment_feedback_enabled_) {
        machining_execution_start_time_ = std::chrono::steady_clock::now();
        machining_execution_clock_started_ = true;
        machining_next_segment_to_mark_ = 0;
        if (!sendMachiningPlanContinuous(&error)) {
            stopMachiningSession(true, true, true);
            QMessageBox::warning(this, "开始加工", error);
            return;
        }
        updateMachiningProgressStep();
        return;
    }

    processMachiningStep();
}

void MainWindow::onPauseMachining()
{
    if (machining_run_state_ != MachiningRunState::Running) {
        return;
    }
    machining_run_state_ = MachiningRunState::Paused;
    machining_refresh_available_ = true;
    if (machining_timer_) {
        machining_timer_->stop();
    }
    spdlog::info("Machining pause: mode={}, current_batch={}/{}",
                 is_simulation_mode_ ? "simulate" : "live",
                 machining_batch_index_,
                 machining_plan_.batches.size());
    updateMachiningUiState();
    statusBar()->showMessage("加工已暂停", 3000);
}

void MainWindow::onEmergencyStop()
{
    emergency_stop_requested_ = true;
    spdlog::warn("Machining emergency stop requested: mode={}, current_batch={}/{}",
                 is_simulation_mode_ ? "simulate" : "live",
                 machining_batch_index_,
                 machining_plan_.batches.size());
    if (machining_data_buffer_) {
        // 预留激光器关光/关快门命令位置。
        machining_data_buffer_->stopTcpThread();
    }

    stopMachiningSession(true, true, true);
    machining_refresh_available_ = true;
    updateMachiningUiState();
    statusBar()->showMessage("已急停，数据帧发送已停止", 5000);
}

void MainWindow::onRefreshMachining()
{
    const bool can_refresh =
        machining_run_state_ == MachiningRunState::Paused ||
        (machining_run_state_ == MachiningRunState::Idle && machining_refresh_available_);
    if (!can_refresh) {
        return;
    }

    if (machining_timer_) {
        machining_timer_->stop();
    }

    if (machining_run_state_ == MachiningRunState::Paused) {
        machining_run_state_ = MachiningRunState::Idle;
    }

    emergency_stop_requested_ = false;

    QString error;
    if (!is_simulation_mode_) {
        if (!validateTcpTargetForMachining(&error)) {
            QMessageBox::warning(this, "刷新", error);
            updateMachiningUiState();
            return;
        }

        if (machining_data_buffer_) {
            machining_data_buffer_->stopTcpThread();
        }
        machining_data_buffer_ = std::make_unique<nbcam::DataBuffer>();

        if (!sendMachiningResetToStartFrame(&error)) {
            if (machining_data_buffer_) {
                machining_data_buffer_->stopTcpThread();
            }
            QMessageBox::warning(this, "刷新", error);
            updateMachiningUiState();
            return;
        }
    }

    if (machining_data_buffer_) {
        machining_data_buffer_->stopTcpThread();
    }
    resetMachiningProgress();
    updateMachiningUiState();
    statusBar()->showMessage(is_simulation_mode_ ? "已刷新至起始数据帧" : "已刷新至起始数据帧，振镜已回归起点", 5000);
}

void MainWindow::onSimulationToggled(bool enabled)
{
    if (ignore_simulation_toggle_signal_) {
        return;
    }
    is_simulation_mode_ = enabled;
    spdlog::info("Simulation mode toggled: enabled={}", enabled);
    statusBar()->showMessage(enabled ? "模拟模式已开启" : "模拟模式已关闭", 3000);
}

void MainWindow::processMachiningStep()
{
    if (machining_run_state_ != MachiningRunState::Running) {
        return;
    }

    if (emergency_stop_requested_) {
        stopMachiningSession(true, true, true);
        return;
    }

    if (machining_batch_index_ >= machining_plan_.batches.size()) {
        if (!is_simulation_mode_ && !machining_segment_feedback_enabled_) {
            QString drain_error;
            if (!ensureMachiningTransportConnected(&drain_error) ||
                !machining_data_buffer_ ||
                !machining_data_buffer_->waitUntilIdle(std::chrono::milliseconds(std::max(5000, comm_config_.tcp_io_timeout_ms * 50)))) {
                stopMachiningSession(true, true, true);
                const QString feedback = machining_data_buffer_
                    ? QString::fromStdString(machining_data_buffer_->getLastTransportFeedback())
                    : QString();
                const QString reason = drain_error.isEmpty()
                    ? QString("加工数据已全部写入本地缓冲，但发送线程未在最终等待时间内排空。%1")
                          .arg(feedback.isEmpty() ? QString("状态=%1").arg(QString::fromStdString(machining_data_buffer_->describeState()))
                                                  : QString("状态=%1，反馈=%2")
                                                        .arg(QString::fromStdString(machining_data_buffer_->describeState()))
                                                        .arg(feedback))
                    : drain_error;
                spdlog::error("Machining failed while draining queued batches: {}", reason.toStdString());
                QMessageBox::warning(this, "加工失败", reason);
                return;
            }
        }
        stopMachiningSession(false, true, true);
        int total_frames = 0;
        for (const auto& batch : machining_plan_.batches) {
            total_frames += batch.frame_count;
        }
        spdlog::info("Machining finished: mode={}, segment_feedback={}, total_batches={}, total_frames={}, completed_segments={}",
                     is_simulation_mode_ ? "simulate" : "live",
                     machining_segment_feedback_enabled_ ? "on" : "off",
                     machining_plan_.batches.size(),
                     total_frames,
                     machining_completed_segment_ids_.size());
        statusBar()->showMessage(is_simulation_mode_ ? "模拟加工完成" : "加工数据帧下发完成", 5000);
        return;
    }

    if (!is_simulation_mode_ && !machining_segment_feedback_enabled_) {
        return;
    }

    QString error;
    if (!is_simulation_mode_ && !sendCurrentMachiningFrame(&error)) {
        stopMachiningSession(true, true, true);
        spdlog::error("Machining failed while sending batch {}: {}",
                      machining_batch_index_ + 1,
                      error.toStdString());
        QMessageBox::warning(this, "加工失败", error);
        return;
    }

    const MachiningBatch& batch = machining_plan_.batches.at(machining_batch_index_);
    const std::string transport_feedback = machining_data_buffer_
        ? machining_data_buffer_->getLastTransportFeedback()
        : std::string();
    if (is_simulation_mode_) {
        spdlog::info("Simulation segment step: batch_index={}/{}, segment_id={}, frames={}, duration_us={}, mark_output={}",
                     machining_batch_index_ + 1,
                     machining_plan_.batches.size(),
                     batch.segment_id,
                     batch.frame_count,
                     batch.duration_us,
                     batch.mark_output);
    } else if (machining_segment_feedback_enabled_) {
        spdlog::info("Machining segment sent: batch_index={}/{}, segment_id={}, bytes={}, frames={}, duration_us={}, mark_output={}, feedback={}",
                     machining_batch_index_ + 1,
                     machining_plan_.batches.size(),
                     batch.segment_id,
                     batch.payload.size(),
                     batch.frame_count,
                     batch.duration_us,
                     batch.mark_output,
                     transport_feedback.empty() ? "n/a" : transport_feedback);
    }
    const int current_segment_id = batch.segment_id;
    ++machining_batch_index_;
    if (current_segment_id >= 0) {
        machining_completed_segment_ids_.insert(current_segment_id);
    }
    applyMachiningProgressHighlight(current_segment_id);

    statusBar()->showMessage(
        QString("%1中: 段 %2 / %3")
            .arg(is_simulation_mode_ ? "模拟加工" : "加工下发")
            .arg(machining_batch_index_)
            .arg(machining_plan_.batches.size()),
        0);

    if (machining_timer_) {
        const int next_interval_ms = is_simulation_mode_
                                         ? 0
                                         : std::max(1, static_cast<int>((batch.duration_us + 999) / 1000));
        machining_timer_->start(next_interval_ms);
    }
    updateMachiningUiState();
}

void MainWindow::updateMachiningProgressStep()
{
    if (machining_run_state_ != MachiningRunState::Running) {
        return;
    }
    if (!machining_execution_clock_started_) {
        machining_execution_start_time_ = std::chrono::steady_clock::now();
        machining_execution_clock_started_ = true;
    }

    const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - machining_execution_start_time_).count();
    while (machining_next_segment_to_mark_ < machining_plan_.segment_end_times_us.size() &&
           elapsed_us >= machining_plan_.segment_end_times_us[machining_next_segment_to_mark_].second) {
        const int segment_id = machining_plan_.segment_end_times_us[machining_next_segment_to_mark_].first;
        machining_completed_segment_ids_.insert(segment_id);
        applyMachiningProgressHighlight(segment_id);
        ++machining_next_segment_to_mark_;
    }

    if (machining_next_segment_to_mark_ < machining_plan_.segment_end_times_us.size()) {
        const qint64 next_elapsed_us = machining_plan_.segment_end_times_us[machining_next_segment_to_mark_].second;
        const qint64 wait_us = std::max<qint64>(1000, next_elapsed_us - elapsed_us);
        if (machining_progress_timer_) {
            machining_progress_timer_->start(static_cast<int>(std::min<qint64>(wait_us / 1000, 1000)));
        }
        return;
    }

    if (!is_simulation_mode_ && !machining_segment_feedback_enabled_) {
        if (machining_data_buffer_ &&
            !machining_data_buffer_->waitUntilIdle(std::chrono::milliseconds(std::max(5000, comm_config_.tcp_io_timeout_ms * 50)))) {
            const QString feedback = QString::fromStdString(machining_data_buffer_->getLastTransportFeedback());
            const QString state = QString::fromStdString(machining_data_buffer_->describeState());
            const QString reason = QString("加工数据已全部写入本地缓冲，但发送线程未在最终等待时间内排空。状态=%1%2")
                                       .arg(state)
                                       .arg(feedback.isEmpty() ? QString() : QString("，反馈=%1").arg(feedback));
            stopMachiningSession(true, true, true);
            spdlog::error("Machining failed while draining queued batches: {}", reason.toStdString());
            QMessageBox::warning(this, "加工失败", reason);
            return;
        }
    }

    stopMachiningSession(false, true, true);
    int total_frames = 0;
    for (const auto& batch : machining_plan_.batches) {
        total_frames += batch.frame_count;
    }
    spdlog::info("Machining finished: mode={}, segment_feedback={}, total_batches={}, total_frames={}, completed_segments={}",
                 is_simulation_mode_ ? "simulate" : "live",
                 machining_segment_feedback_enabled_ ? "on" : "off",
                 machining_plan_.batches.size(),
                 total_frames,
                 machining_completed_segment_ids_.size());
    statusBar()->showMessage(is_simulation_mode_ ? "模拟加工完成" : "加工数据帧下发完成", 5000);
}

void MainWindow::createLanguageMenu()
{
    language_menu_ = menuBar()->addMenu("语言(&G)");
    language_group_ = new QActionGroup(this);
    language_group_->setExclusive(true);
    connect(language_group_, &QActionGroup::triggered, this, &MainWindow::onLanguageSelected);

    QAction* system_action = new QAction("跟随系统", this);
    system_action->setCheckable(true);
    system_action->setData(QString());
    language_group_->addAction(system_action);
    language_menu_->addAction(system_action);

    const QStringList locales = discoverQtTranslationLocales();
    if (!locales.isEmpty()) {
        language_menu_->addSeparator();
        for (const QString& locale_name : locales) {
            QAction* action = new QAction(formatLanguageLabel(locale_name), this);
            action->setCheckable(true);
            action->setData(locale_name);
            language_group_->addAction(action);
            language_menu_->addAction(action);
        }
    }

    system_action->setChecked(true);
    applyQtTranslation(QLocale::system().name());
}

QStringList MainWindow::discoverQtTranslationLocales() const
{
    const QString translation_path = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    QDir dir(translation_path);
    if (!dir.exists()) {
        return {};
    }

    const QStringList files = dir.entryList(QStringList{"qtbase_*.qm", "qt_*.qm"}, QDir::Files);
    QSet<QString> locales;
    for (const QString& file_name : files) {
        const QString locale_name = localeFromQtTranslationFile(file_name);
        if (!locale_name.isEmpty()) {
            locales.insert(locale_name);
        }
    }

    QStringList sorted_locales = locales.values();
    sorted_locales.sort();
    return sorted_locales;
}

bool MainWindow::applyQtTranslation(const QString& locale_name)
{
    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) {
        return false;
    }

    if (!qt_translator_) {
        qt_translator_ = new QTranslator(this);
    }

    app->removeTranslator(qt_translator_);
    if (locale_name.trimmed().isEmpty()) {
        return false;
    }

    const QString translation_path = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    bool loaded = qt_translator_->load("qtbase_" + locale_name, translation_path);
    if (!loaded) {
        loaded = qt_translator_->load("qt_" + locale_name, translation_path);
    }
    if (loaded) {
        app->installTranslator(qt_translator_);
    }
    return loaded;
}

QString MainWindow::formatLanguageLabel(const QString& locale_name) const
{
    const QLocale locale(locale_name);
    QString native_name = locale.nativeLanguageName();
    if (native_name.isEmpty()) {
        native_name = QLocale::languageToString(locale.language());
    }
    if (native_name.isEmpty()) {
        return locale_name;
    }
    return QString("%1 (%2)").arg(native_name, locale_name);
}

void MainWindow::onLanguageSelected(QAction* action)
{
    if (!action) {
        return;
    }

    const QString selected_locale = action->data().toString().trimmed();
    const QString target_locale = selected_locale.isEmpty() ? QLocale::system().name() : selected_locale;
    const bool loaded = applyQtTranslation(target_locale);

    if (!loaded && !selected_locale.isEmpty()) {
        QMessageBox::warning(this,
                             "语言切换",
                             QString("未找到 Qt 翻译文件：%1\n已恢复为系统语言。").arg(target_locale));
        if (language_group_) {
            for (QAction* candidate : language_group_->actions()) {
                if (candidate->data().toString().isEmpty()) {
                    candidate->setChecked(true);
                    break;
                }
            }
        }
        applyQtTranslation(QLocale::system().name());
        return;
    }

    if (loaded) {
        statusBar()->showMessage(QString("Qt语言已切换：%1").arg(target_locale), 3000);
    } else {
        statusBar()->showMessage("未找到系统语言对应Qt翻译，已使用默认语言", 3000);
    }
}

void MainWindow::openPathPreviewDialog()
{
    if (!path_preview_dialog_) {
        return;
    }
    path_preview_dialog_->show();
    path_preview_dialog_->raise();
    path_preview_dialog_->activateWindow();
}

void MainWindow::saveCurrentPathFromPreview()
{
    if (!controller_ || !model_view_ || !texture_list_widget_) {
        return;
    }

    nbcam::LaserJob* live_job = controller_->getCurrentJob();
    const int patch_id = model_view_->getSelectedPatchId();
    if (patch_id < 0) {
        QMessageBox::warning(this, "保存路径", "请先选中一个Patch后再保存路径。");
        return;
    }
    if (!live_job || !live_job->isValid()) {
        QMessageBox::warning(this, "保存路径", "当前没有可保存的路径。");
        return;
    }

    saved_patch_jobs_[patch_id] = cloneLaserJobDeep(*live_job);
    std::vector<SavedUvPoint> uv_snapshot;
    const auto& live_uv_path = controller_->getUVPath();
    uv_snapshot.reserve(live_uv_path.size());
    for (const auto& p : live_uv_path) {
        if (!std::isfinite(p.u) || !std::isfinite(p.v)) {
            continue;
        }
        SavedUvPoint dst;
        dst.u = p.u;
        dst.v = p.v;
        dst.jump_before = p.is_jump_before;
        uv_snapshot.push_back(dst);
    }
    saved_patch_uv_paths_[patch_id] = std::move(uv_snapshot);

    TextureInfo info = texture_list_widget_->getTexture(patch_id);
    if (info.patch_id >= 0) {
        info.has_saved_path = true;
        texture_list_widget_->updateTexture(patch_id, info);
    }
    updateTextureContourInfoFromJob(patch_id, live_job);
    updateUIWithoutMesh();
    statusBar()->showMessage(QString("Patch %1 路径已保存（仅可通过“移除路径”清除）").arg(patch_id), 5000);
}

void MainWindow::refreshExecuteActionState()
{
    const bool live_ready = controller_ && controller_->getCurrentJob() && controller_->getCurrentJob()->isValid();
    if (execute_job_action_) {
        execute_job_action_->setEnabled(live_ready);
    }
    if (machining_start_button_) {
        machining_start_button_->setEnabled(live_ready);
    }
}

bool MainWindow::currentModelFitsProcessingRegion(QString* out_error) const
{
    if (!controller_ || !controller_->getCurrentMesh()) {
        if (out_error) {
            *out_error = "当前没有已加载模型。";
        }
        return false;
    }

    std::string message;
    const bool fits = controller_->currentMeshFitsBounds(kProcessingRegionMinMm,
                                                         kProcessingRegionMaxMm,
                                                         kProcessingRegionMinMm,
                                                         kProcessingRegionMaxMm,
                                                         kProcessingRegionMinMm,
                                                         kProcessingRegionMaxMm,
                                                         &message);
    if (!fits && out_error) {
        *out_error = QString::fromStdString(message);
    }
    return fits;
}

void MainWindow::refreshModelProcessingRegionStatus()
{
    if (!controller_ || !controller_->getCurrentMesh()) {
        return;
    }

    QString error;
    if (!currentModelFitsProcessingRegion(&error)) {
        spdlog::warn("{}", error.toStdString());
        statusBar()->showMessage(QString("模型超出加工区域：%1").arg(error), 6000);
    }
}

bool MainWindow::configureCommunicationSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle("连接设置");
    dialog.setModal(true);
    dialog.resize(620, 520);

    auto* layout = new QVBoxLayout(&dialog);
    const bool use_rtc6 = (comm_config_.executor_backend == ExecutorBackend::RTC6);

    auto* hint_label = new QLabel(
        "设备、板卡、激光器、DataGenerator时序参数、RTC-6参数、机床零点偏置和软限位已移到顶部“设备与板卡”菜单。",
        &dialog);
    hint_label->setWordWrap(true);
    layout->addWidget(hint_label);

    auto* link_group = new QGroupBox("连接链路", &dialog);
    auto* link_form = new QFormLayout(link_group);

    auto* grpc_endpoint_edit = new QLineEdit(comm_config_.grpc_endpoint, link_group);
    auto* grpc_enable_check = new QCheckBox("连接时测试gRPC", link_group);
    grpc_enable_check->setChecked(comm_config_.enable_grpc_test);
    auto* tcp_host_edit = new QLineEdit(comm_config_.tcp_host, link_group);
    auto* tcp_port_spin = new QSpinBox(link_group);
    tcp_port_spin->setRange(1, 65535);
    tcp_port_spin->setValue(comm_config_.tcp_port);
    auto* tcp_timeout_spin = new QSpinBox(link_group);
    tcp_timeout_spin->setRange(50, 10000);
    tcp_timeout_spin->setValue(comm_config_.tcp_io_timeout_ms);
    auto* tcp_retry_spin = new QSpinBox(link_group);
    tcp_retry_spin->setRange(10, 5000);
    tcp_retry_spin->setValue(comm_config_.tcp_connect_retry_ms);
    auto* serial_port_edit = new QLineEdit(comm_config_.serial_port, link_group);
    auto* serial_baud_spin = new QSpinBox(link_group);
    serial_baud_spin->setRange(1200, 921600);
    serial_baud_spin->setSingleStep(1200);
    serial_baud_spin->setValue(comm_config_.serial_baud);
    auto* serial_enable_check = new QCheckBox("执行前联锁时同时测试串口（激光器）", link_group);
    serial_enable_check->setChecked(comm_config_.enable_serial_test);

    grpc_endpoint_edit->setEnabled(!use_rtc6);
    grpc_enable_check->setEnabled(!use_rtc6);
    tcp_host_edit->setEnabled(!use_rtc6);
    tcp_port_spin->setEnabled(!use_rtc6);
    tcp_timeout_spin->setEnabled(!use_rtc6);
    tcp_retry_spin->setEnabled(!use_rtc6);

    link_form->addRow("gRPC端点", grpc_endpoint_edit);
    link_form->addRow("", grpc_enable_check);
    link_form->addRow("TCP主机", tcp_host_edit);
    link_form->addRow("TCP端口", tcp_port_spin);
    link_form->addRow("TCP超时(ms)", tcp_timeout_spin);
    link_form->addRow("TCP重试间隔(ms)", tcp_retry_spin);
    link_form->addRow("串口号", serial_port_edit);
    link_form->addRow("串口波特率", serial_baud_spin);
    link_form->addRow("", serial_enable_check);
    layout->addWidget(link_group);

    auto* exec_group = new QGroupBox("执行安全", &dialog);
    auto* exec_form = new QFormLayout(exec_group);
    auto* transform_check = new QCheckBox("执行前将任务坐标转换到机床坐标", exec_group);
    transform_check->setChecked(comm_config_.apply_job_transform);
    transform_check->setToolTip("如果任务内包含4x4坐标变换矩阵，则先转换到机床坐标，再叠加零点偏置并检查软限位。");
    auto* preexecute_check = new QCheckBox("真实下发前强制做TCP/串口联锁检查", exec_group);
    preexecute_check->setChecked(comm_config_.require_preexecute_comm_check);
    exec_form->addRow("", transform_check);
    exec_form->addRow("", preexecute_check);
    layout->addWidget(exec_group);

    auto* tip_label = new QLabel(
        use_rtc6
            ? "当前设备为RTC-6。此处仅配置链路/执行安全”。"
            : "当前设备为DataGenerator。此处仅配置gRPC/TCP链路和执行安全”。",
        &dialog);
    tip_label->setWordWrap(true);
    layout->addWidget(tip_label);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString grpc_endpoint = grpc_endpoint_edit->text().trimmed();
    const QString tcp_host = tcp_host_edit->text().trimmed();
    const QString serial_port = serial_port_edit->text().trimmed();
    if (!use_rtc6 && tcp_host.isEmpty()) {
        QMessageBox::warning(this, "错误", "DataGenerator链路的TCP参数不能为空。");
        return false;
    }
    if (!use_rtc6 && grpc_enable_check->isChecked() && grpc_endpoint.isEmpty()) {
        QMessageBox::warning(this, "错误", "已启用gRPC测试时，gRPC端点不能为空。");
        return false;
    }
    if (serial_enable_check->isChecked() && serial_port.isEmpty()) {
        QMessageBox::warning(this, "错误", "启用串口联锁时，串口号不能为空。");
        return false;
    }
    comm_config_.grpc_endpoint = grpc_endpoint;
    comm_config_.enable_grpc_test = grpc_enable_check->isChecked();
    comm_config_.tcp_host = tcp_host;
    comm_config_.tcp_port = tcp_port_spin->value();
    comm_config_.tcp_io_timeout_ms = tcp_timeout_spin->value();
    comm_config_.tcp_connect_retry_ms = tcp_retry_spin->value();
    comm_config_.serial_port = serial_port;
    comm_config_.serial_baud = serial_baud_spin->value();
    comm_config_.enable_serial_test = serial_enable_check->isChecked();
    comm_config_.apply_job_transform = transform_check->isChecked();
    comm_config_.require_preexecute_comm_check = preexecute_check->isChecked();

    spdlog::info("连接设置已更新: backend={}, grpc={} (enabled={}), tcp={}:{}, timeout={}ms, retry={}ms, serial={}@{}, serial_test_enabled={}, transform={}, precheck={}",
                 use_rtc6 ? "rtc6" : "datagenerator",
                 comm_config_.grpc_endpoint.toStdString(),
                 comm_config_.enable_grpc_test,
                 comm_config_.tcp_host.toStdString(),
                 comm_config_.tcp_port,
                 comm_config_.tcp_io_timeout_ms,
                 comm_config_.tcp_connect_retry_ms,
                 comm_config_.serial_port.toStdString(),
                 comm_config_.serial_baud,
                 comm_config_.enable_serial_test,
                 comm_config_.apply_job_transform,
                 comm_config_.require_preexecute_comm_check);
    return true;
}

bool MainWindow::configureDeviceBoardSettings()
{
    return configureDeviceSettings() && configureBoardSettings() && configureLaserSettings();
}

bool MainWindow::configureDeviceSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle("设备设置");
    dialog.setModal(true);
    dialog.resize(420, 220);

    auto* layout = new QVBoxLayout(&dialog);

    auto* device_group = new QGroupBox("设备", &dialog);
    auto* device_form = new QFormLayout(device_group);
    auto* device_combo = new QComboBox(device_group);
    auto* axis_mode_combo = new QComboBox(device_group);
    device_combo->addItem("默认板卡", 0);
    device_combo->addItem("RTC-6板卡", 1);
    axis_mode_combo->addItem("三轴", 0);
    axis_mode_combo->addItem("五轴", 1);
    const int backend_data = (comm_config_.executor_backend == ExecutorBackend::RTC6) ? 1 : 0;
    const int axis_mode_data = (comm_config_.axis_mode == AxisMode::FiveAxis) ? 1 : 0;
    const int device_index = device_combo->findData(backend_data);
    const int axis_mode_index = axis_mode_combo->findData(axis_mode_data);
    if (device_index >= 0) {
        device_combo->setCurrentIndex(device_index);
    }
    if (axis_mode_index >= 0) {
        axis_mode_combo->setCurrentIndex(axis_mode_index);
    }
    device_form->addRow("设备", device_combo);
    device_form->addRow("轴模式", axis_mode_combo);
    layout->addWidget(device_group);

    auto* hint_label = new QLabel("三轴模式下A/B轴保持中位值；五轴模式下数据帧携带A/B轴振镜偏摆/旋转数据。", &dialog);
    hint_label->setWordWrap(true);
    layout->addWidget(hint_label);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const bool use_rtc6 = (device_combo->currentData().toInt() == 1);
    comm_config_.executor_backend = use_rtc6 ? ExecutorBackend::RTC6 : ExecutorBackend::DataGenerator;
    comm_config_.axis_mode = (axis_mode_combo->currentData().toInt() == 1) ? AxisMode::FiveAxis : AxisMode::ThreeAxis;

    spdlog::info("设备设置已更新: backend={}, axis_mode={}",
                 use_rtc6 ? "rtc6" : "datagenerator",
                 comm_config_.axis_mode == AxisMode::FiveAxis ? "five_axis" : "three_axis");
    return true;
}

bool MainWindow::configureBoardSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle("板卡设置");
    dialog.setModal(true);
    dialog.resize(680, 720);

    auto* layout = new QVBoxLayout(&dialog);
    auto create_mm_spin = [&dialog](double value) {
        auto* spin = new QDoubleSpinBox(&dialog);
        spin->setRange(-1000.0, 1000.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.1);
        spin->setValue(value);
        return spin;
    };

    auto* delay_group = new QGroupBox("时序参数 (us & mm/s)", &dialog);
    auto* delay_form = new QFormLayout(delay_group);
    auto* jump_speed_spin = new QSpinBox(delay_group);
    jump_speed_spin->setRange(1, 50000);
    jump_speed_spin->setValue(comm_config_.jump_speed_mm_s);
    auto* laser_on_delay_spin = new QSpinBox(delay_group);
    laser_on_delay_spin->setRange(0, 200000);
    laser_on_delay_spin->setValue(comm_config_.laser_on_delay_us);
    auto* laser_off_delay_spin = new QSpinBox(delay_group);
    laser_off_delay_spin->setRange(0, 200000);
    laser_off_delay_spin->setValue(comm_config_.laser_off_delay_us);
    auto* mark_delay_spin = new QSpinBox(delay_group);
    mark_delay_spin->setRange(0, 200000);
    mark_delay_spin->setValue(comm_config_.mark_delay_us);
    auto* jump_delay_spin = new QSpinBox(delay_group);
    jump_delay_spin->setRange(0, 200000);
    jump_delay_spin->setValue(comm_config_.jump_delay_us);
    auto* polygon_delay_spin = new QSpinBox(delay_group);
    polygon_delay_spin->setRange(0, 200000);
    polygon_delay_spin->setValue(comm_config_.polygon_delay_us);
    delay_form->addRow("跳转速度(mm/s)", jump_speed_spin);
    delay_form->addRow("开光延迟", laser_on_delay_spin);
    delay_form->addRow("关光延迟", laser_off_delay_spin);
    delay_form->addRow("扫描延迟", mark_delay_spin);
    delay_form->addRow("跳转延迟", jump_delay_spin);
    delay_form->addRow("多边形延迟", polygon_delay_spin);
    layout->addWidget(delay_group);

    auto* rtc6_group = new QGroupBox("RTC-6参数", &dialog);
    auto* rtc6_form = new QFormLayout(rtc6_group);
    auto* rtc6_card_spin = new QSpinBox(rtc6_group);
    rtc6_card_spin->setRange(1, 16);
    rtc6_card_spin->setValue(comm_config_.rtc6_card_no);
    auto* rtc6_units_spin = new QDoubleSpinBox(rtc6_group);
    rtc6_units_spin->setRange(1.0, 1000000.0);
    rtc6_units_spin->setDecimals(3);
    rtc6_units_spin->setValue(comm_config_.rtc6_units_per_mm);
    auto* rtc6_mark_speed_spin = new QDoubleSpinBox(rtc6_group);
    rtc6_mark_speed_spin->setRange(1.0, 50000.0);
    rtc6_mark_speed_spin->setDecimals(3);
    rtc6_mark_speed_spin->setValue(comm_config_.rtc6_mark_speed_mm_s);
    auto* rtc6_jump_speed_spin = new QDoubleSpinBox(rtc6_group);
    rtc6_jump_speed_spin->setRange(1.0, 50000.0);
    rtc6_jump_speed_spin->setDecimals(3);
    rtc6_jump_speed_spin->setValue(comm_config_.rtc6_jump_speed_mm_s);
    auto* rtc6_program_edit = new QLineEdit(comm_config_.rtc6_program_file, rtc6_group);
    rtc6_form->addRow("卡号(CardNo)", rtc6_card_spin);
    rtc6_form->addRow("编码系数(units/mm)", rtc6_units_spin);
    rtc6_form->addRow("默认扫描速度(mm/s)", rtc6_mark_speed_spin);
    rtc6_form->addRow("默认跳转速度(mm/s)", rtc6_jump_speed_spin);
    rtc6_form->addRow("程序文件", rtc6_program_edit);
    layout->addWidget(rtc6_group);

    auto* offset_group = new QGroupBox("机床零点偏置(mm)", &dialog);
    auto* offset_form = new QFormLayout(offset_group);
    auto* x_offset_spin = create_mm_spin(comm_config_.x_offset_mm);
    auto* y_offset_spin = create_mm_spin(comm_config_.y_offset_mm);
    auto* z_offset_spin = create_mm_spin(comm_config_.z_offset_mm);
    auto* a_offset_spin = create_mm_spin(comm_config_.a_offset_mm);
    auto* b_offset_spin = create_mm_spin(comm_config_.b_offset_mm);
    offset_form->addRow("X偏置", x_offset_spin);
    offset_form->addRow("Y偏置", y_offset_spin);
    offset_form->addRow("Z偏置", z_offset_spin);
    offset_form->addRow("A偏置", a_offset_spin);
    offset_form->addRow("B偏置", b_offset_spin);
    layout->addWidget(offset_group);

    auto* limit_group = new QGroupBox("软限位 (mm)", &dialog);
    auto* limit_layout = new QGridLayout(limit_group);
    limit_layout->addWidget(new QLabel("轴"), 0, 0);
    limit_layout->addWidget(new QLabel("最小"), 0, 1);
    limit_layout->addWidget(new QLabel("最大"), 0, 2);
    limit_layout->addWidget(new QLabel("X"), 1, 0);
    limit_layout->addWidget(new QLabel("Y"), 2, 0);
    limit_layout->addWidget(new QLabel("Z"), 3, 0);
    limit_layout->addWidget(new QLabel("A"), 4, 0);
    limit_layout->addWidget(new QLabel("B"), 5, 0);
    auto* x_min_spin = create_mm_spin(comm_config_.x_min_mm);
    auto* x_max_spin = create_mm_spin(comm_config_.x_max_mm);
    auto* y_min_spin = create_mm_spin(comm_config_.y_min_mm);
    auto* y_max_spin = create_mm_spin(comm_config_.y_max_mm);
    auto* z_min_spin = create_mm_spin(comm_config_.z_min_mm);
    auto* z_max_spin = create_mm_spin(comm_config_.z_max_mm);
    auto* a_min_spin = create_mm_spin(comm_config_.a_min_mm);
    auto* a_max_spin = create_mm_spin(comm_config_.a_max_mm);
    auto* b_min_spin = create_mm_spin(comm_config_.b_min_mm);
    auto* b_max_spin = create_mm_spin(comm_config_.b_max_mm);
    limit_layout->addWidget(x_min_spin, 1, 1);
    limit_layout->addWidget(x_max_spin, 1, 2);
    limit_layout->addWidget(y_min_spin, 2, 1);
    limit_layout->addWidget(y_max_spin, 2, 2);
    limit_layout->addWidget(z_min_spin, 3, 1);
    limit_layout->addWidget(z_max_spin, 3, 2);
    limit_layout->addWidget(a_min_spin, 4, 1);
    limit_layout->addWidget(a_max_spin, 4, 2);
    limit_layout->addWidget(b_min_spin, 5, 1);
    limit_layout->addWidget(b_max_spin, 5, 2);
    layout->addWidget(limit_group);

    auto* tip_label = new QLabel(
        "默认规则：round(machine_mm * 1000 + 32768)。\n"
        "RTC-6规则：round(machine_mm * units_per_mm)。\n"
        "软限位将在任务坐标转换和零点偏置之后检查。",
        &dialog);
    tip_label->setWordWrap(true);
    layout->addWidget(tip_label);

    rtc6_group->setEnabled(comm_config_.executor_backend == ExecutorBackend::RTC6);
    const bool use_five_axis = (comm_config_.axis_mode == AxisMode::FiveAxis);
    a_offset_spin->setEnabled(use_five_axis);
    b_offset_spin->setEnabled(use_five_axis);
    a_min_spin->setEnabled(use_five_axis);
    a_max_spin->setEnabled(use_five_axis);
    b_min_spin->setEnabled(use_five_axis);
    b_max_spin->setEnabled(use_five_axis);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    if (x_min_spin->value() > x_max_spin->value() ||
        y_min_spin->value() > y_max_spin->value() ||
        z_min_spin->value() > z_max_spin->value() ||
        a_min_spin->value() > a_max_spin->value() ||
        b_min_spin->value() > b_max_spin->value()) {
        QMessageBox::warning(this, "错误", "软限位设置非法：最小值不能大于最大值。");
        return false;
    }

    comm_config_.x_offset_mm = x_offset_spin->value();
    comm_config_.y_offset_mm = y_offset_spin->value();
    comm_config_.z_offset_mm = z_offset_spin->value();
    comm_config_.a_offset_mm = a_offset_spin->value();
    comm_config_.b_offset_mm = b_offset_spin->value();
    comm_config_.x_min_mm = x_min_spin->value();
    comm_config_.x_max_mm = x_max_spin->value();
    comm_config_.y_min_mm = y_min_spin->value();
    comm_config_.y_max_mm = y_max_spin->value();
    comm_config_.z_min_mm = z_min_spin->value();
    comm_config_.z_max_mm = z_max_spin->value();
    comm_config_.a_min_mm = a_min_spin->value();
    comm_config_.a_max_mm = a_max_spin->value();
    comm_config_.b_min_mm = b_min_spin->value();
    comm_config_.b_max_mm = b_max_spin->value();
    comm_config_.jump_speed_mm_s = jump_speed_spin->value();
    comm_config_.laser_on_delay_us = laser_on_delay_spin->value();
    comm_config_.laser_off_delay_us = laser_off_delay_spin->value();
    comm_config_.mark_delay_us = mark_delay_spin->value();
    comm_config_.jump_delay_us = jump_delay_spin->value();
    comm_config_.polygon_delay_us = polygon_delay_spin->value();
    comm_config_.rtc6_card_no = rtc6_card_spin->value();
    comm_config_.rtc6_units_per_mm = rtc6_units_spin->value();
    comm_config_.rtc6_mark_speed_mm_s = rtc6_mark_speed_spin->value();
    comm_config_.rtc6_jump_speed_mm_s = rtc6_jump_speed_spin->value();
    comm_config_.rtc6_program_file = rtc6_program_edit->text().trimmed();

    spdlog::info("板卡设置已更新: backend={}, axis_mode={}, offsets=({:.3f}, {:.3f}, {:.3f}, {:.3f}, {:.3f}), x=[{:.3f}, {:.3f}], y=[{:.3f}, {:.3f}], z=[{:.3f}, {:.3f}], a=[{:.3f}, {:.3f}], b=[{:.3f}, {:.3f}], delay(on/off/mark/jump/poly)={}/{}/{}/{}/{}us, jump_speed={}mm/s, rtc6(card={}, units/mm={:.3f}, mark_speed={:.3f}, jump_speed={:.3f}, program='{}')",
                 comm_config_.executor_backend == ExecutorBackend::RTC6 ? "rtc6" : "datagenerator",
                 comm_config_.axis_mode == AxisMode::FiveAxis ? "five_axis" : "three_axis",
                 comm_config_.x_offset_mm,
                 comm_config_.y_offset_mm,
                 comm_config_.z_offset_mm,
                 comm_config_.a_offset_mm,
                 comm_config_.b_offset_mm,
                 comm_config_.x_min_mm,
                 comm_config_.x_max_mm,
                 comm_config_.y_min_mm,
                 comm_config_.y_max_mm,
                 comm_config_.z_min_mm,
                 comm_config_.z_max_mm,
                 comm_config_.a_min_mm,
                 comm_config_.a_max_mm,
                 comm_config_.b_min_mm,
                 comm_config_.b_max_mm,
                 comm_config_.laser_on_delay_us,
                 comm_config_.laser_off_delay_us,
                 comm_config_.mark_delay_us,
                 comm_config_.jump_delay_us,
                 comm_config_.polygon_delay_us,
                 comm_config_.jump_speed_mm_s,
                 comm_config_.rtc6_card_no,
                 comm_config_.rtc6_units_per_mm,
                 comm_config_.rtc6_mark_speed_mm_s,
                 comm_config_.rtc6_jump_speed_mm_s,
                 comm_config_.rtc6_program_file.toStdString());
    return true;
}

bool MainWindow::configureLaserSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle("激光器设置");
    dialog.setModal(true);
    dialog.resize(620, 520);

    auto* layout = new QVBoxLayout(&dialog);

    auto* source_label = new QLabel(QString("默认资料: %1").arg(comm_config_.laser_params_source), &dialog);
    source_label->setWordWrap(true);
    layout->addWidget(source_label);

    auto* laser_group = new QGroupBox("激光器参数", &dialog);
    auto* laser_form = new QFormLayout(laser_group);
    auto* model_edit = new QLineEdit(comm_config_.laser_model, laser_group);
    auto* control_mode_edit = new QLineEdit(comm_config_.laser_control_mode, laser_group);
    auto* source_edit = new QLineEdit(comm_config_.laser_params_source, laser_group);
    auto* rated_power_spin = new QDoubleSpinBox(laser_group);
    rated_power_spin->setRange(0.0, 10000.0);
    rated_power_spin->setDecimals(3);
    rated_power_spin->setValue(comm_config_.laser_rated_power_w);
    auto* min_power_spin = new QDoubleSpinBox(laser_group);
    min_power_spin->setRange(0.0, 10000.0);
    min_power_spin->setDecimals(3);
    min_power_spin->setValue(comm_config_.laser_min_power_w);
    auto* max_power_spin = new QDoubleSpinBox(laser_group);
    max_power_spin->setRange(0.0, 10000.0);
    max_power_spin->setDecimals(3);
    max_power_spin->setValue(comm_config_.laser_max_power_w);
    auto* min_freq_spin = new QSpinBox(laser_group);
    min_freq_spin->setRange(1, 10000000);
    min_freq_spin->setValue(comm_config_.laser_min_freq_hz);
    auto* max_freq_spin = new QSpinBox(laser_group);
    max_freq_spin->setRange(1, 10000000);
    max_freq_spin->setValue(comm_config_.laser_max_freq_hz);
    auto* notes_edit = new QLineEdit(comm_config_.laser_notes, laser_group);

    laser_form->addRow("型号", model_edit);
    laser_form->addRow("控制方式", control_mode_edit);
    laser_form->addRow("资料来源", source_edit);
    laser_form->addRow("额定功率(W)", rated_power_spin);
    laser_form->addRow("最小功率(W)", min_power_spin);
    laser_form->addRow("最大功率(W)", max_power_spin);
    laser_form->addRow("最小频率(Hz)", min_freq_spin);
    laser_form->addRow("最大频率(Hz)", max_freq_spin);
    laser_form->addRow("备注", notes_edit);
    layout->addWidget(laser_group);

    auto* serial_group = new QGroupBox("串口联锁", &dialog);
    auto* serial_form = new QFormLayout(serial_group);
    auto* serial_port_edit = new QLineEdit(comm_config_.serial_port, serial_group);
    auto* serial_baud_spin = new QSpinBox(serial_group);
    serial_baud_spin->setRange(1200, 921600);
    serial_baud_spin->setSingleStep(1200);
    serial_baud_spin->setValue(comm_config_.serial_baud);
    auto* serial_enable_check = new QCheckBox("执行前联锁时测试激光器串口", serial_group);
    serial_enable_check->setChecked(comm_config_.enable_serial_test);
    serial_form->addRow("串口号", serial_port_edit);
    serial_form->addRow("波特率", serial_baud_spin);
    serial_form->addRow("", serial_enable_check);
    layout->addWidget(serial_group);

    auto* import_button = new QPushButton("导入激光器参数", &dialog);
    layout->addWidget(import_button);

    const auto readTextFile = [](const QString& path) -> QString {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        QTextStream stream(&file);
        return stream.readAll();
    };

    const auto firstNumber = [](const QString& text, const QStringList& keys) -> QString {
        for (const QString& key : keys) {
            const QRegularExpression rx(QString("(?i)(%1)\\s*[=:：,，]?\\s*([0-9]+(?:\\.[0-9]+)?)").arg(QRegularExpression::escape(key)));
            const auto match = rx.match(text);
            if (match.hasMatch()) {
                return match.captured(2);
            }
        }
        return {};
    };

    const auto firstText = [](const QString& text, const QStringList& keys) -> QString {
        for (const QString& key : keys) {
            const QRegularExpression rx(QString("(?i)(%1)\\s*[=:：,，]\\s*([^\\r\\n,，;；]+)").arg(QRegularExpression::escape(key)));
            const auto match = rx.match(text);
            if (match.hasMatch()) {
                return match.captured(2).trimmed();
            }
        }
        return {};
    };

    connect(import_button, &QPushButton::clicked, &dialog, [&dialog,
                                                            readTextFile,
                                                            firstNumber,
                                                            firstText,
                                                            source_edit,
                                                            notes_edit,
                                                            model_edit,
                                                            control_mode_edit,
                                                            rated_power_spin,
                                                            min_power_spin,
                                                            max_power_spin,
                                                            min_freq_spin,
                                                            max_freq_spin,
                                                            serial_baud_spin,
                                                            serial_port_edit]() {
        const QString path = openFileSafe(
            &dialog,
            "导入激光器参数",
            "reference",
            "参数文件 (*.json *.csv *.txt *.ini *.pdf);;所有文件 (*.*)");
        if (path.isEmpty()) {
            return;
        }

        source_edit->setText(QDir::current().relativeFilePath(path));
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == "pdf") {
            notes_edit->setText(QString("参数资料已切换为PDF：%1；请按说明书确认并补充可编辑字段。").arg(QFileInfo(path).fileName()));
            return;
        }

        const QString text = readTextFile(path);
        if (text.isEmpty()) {
            QMessageBox::warning(&dialog, "导入失败", "无法读取参数文件。");
            return;
        }

        const QString model = firstText(text, {"model", "laser_model", "型号", "产品型号"});
        const QString control_mode = firstText(text, {"control_mode", "控制方式", "接口", "interface"});
        if (!model.isEmpty()) {
            model_edit->setText(model);
        }
        if (!control_mode.isEmpty()) {
            control_mode_edit->setText(control_mode);
        }

        bool ok = false;
        double value = firstNumber(text, {"rated_power_w", "rated_power", "额定功率"}).toDouble(&ok);
        if (ok) {
            rated_power_spin->setValue(value);
        }
        value = firstNumber(text, {"min_power_w", "min_power", "最小功率"}).toDouble(&ok);
        if (ok) {
            min_power_spin->setValue(value);
        }
        value = firstNumber(text, {"max_power_w", "max_power", "最大功率"}).toDouble(&ok);
        if (ok) {
            max_power_spin->setValue(value);
        }
        int int_value = firstNumber(text, {"min_freq_hz", "min_frequency_hz", "最小频率"}).toInt(&ok);
        if (ok) {
            min_freq_spin->setValue(int_value);
        }
        int_value = firstNumber(text, {"max_freq_hz", "max_frequency_hz", "最大频率"}).toInt(&ok);
        if (ok) {
            max_freq_spin->setValue(int_value);
        }
        int_value = firstNumber(text, {"serial_baud", "baud", "波特率"}).toInt(&ok);
        if (ok) {
            serial_baud_spin->setValue(int_value);
        }
        const QString port = firstText(text, {"serial_port", "串口号", "串口"});
        if (!port.isEmpty()) {
            serial_port_edit->setText(port);
        }
        notes_edit->setText(QString("已从 %1 导入可识别字段。").arg(QFileInfo(path).fileName()));
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    if (min_power_spin->value() > max_power_spin->value()) {
        QMessageBox::warning(this, "错误", "激光器功率范围非法：最小功率不能大于最大功率。");
        return false;
    }
    if (min_freq_spin->value() > max_freq_spin->value()) {
        QMessageBox::warning(this, "错误", "激光器频率范围非法：最小频率不能大于最大频率。");
        return false;
    }
    if (serial_enable_check->isChecked() && serial_port_edit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "错误", "启用激光器串口联锁时，串口号不能为空。");
        return false;
    }

    comm_config_.laser_model = model_edit->text().trimmed();
    comm_config_.laser_control_mode = control_mode_edit->text().trimmed();
    comm_config_.laser_params_source = source_edit->text().trimmed();
    comm_config_.laser_rated_power_w = rated_power_spin->value();
    comm_config_.laser_min_power_w = min_power_spin->value();
    comm_config_.laser_max_power_w = max_power_spin->value();
    comm_config_.laser_min_freq_hz = min_freq_spin->value();
    comm_config_.laser_max_freq_hz = max_freq_spin->value();
    comm_config_.laser_notes = notes_edit->text().trimmed();
    comm_config_.serial_port = serial_port_edit->text().trimmed();
    comm_config_.serial_baud = serial_baud_spin->value();
    comm_config_.enable_serial_test = serial_enable_check->isChecked();

    spdlog::info("激光器设置已更新: model='{}', source='{}', power=[{:.3f}, {:.3f}]W rated={:.3f}W, freq=[{}, {}]Hz, control='{}', serial={}@{}, serial_test={}",
                 comm_config_.laser_model.toStdString(),
                 comm_config_.laser_params_source.toStdString(),
                 comm_config_.laser_min_power_w,
                 comm_config_.laser_max_power_w,
                 comm_config_.laser_rated_power_w,
                 comm_config_.laser_min_freq_hz,
                 comm_config_.laser_max_freq_hz,
                 comm_config_.laser_control_mode.toStdString(),
                 comm_config_.serial_port.toStdString(),
                 comm_config_.serial_baud,
                 comm_config_.enable_serial_test);
    return true;
}

bool MainWindow::testGrpcCommunicationImpl(QString* out_feedback)
{
    QString host;
    int port = 0;
    QString parse_error;
    if (!parseHostPort(comm_config_.grpc_endpoint, host, port, &parse_error)) {
        if (out_feedback) {
            *out_feedback = parse_error;
        }
        spdlog::error("gRPC通信测试失败: {}", parse_error.toStdString());
        return false;
    }

    spdlog::info("开始gRPC通信测试(连通性): endpoint={} -> {}:{}",
                 comm_config_.grpc_endpoint.toStdString(),
                 host.toStdString(),
                 port);

    QString feedback;
#ifdef _WIN32
    const bool ok = probeTcpEndpoint(host, port, comm_config_.tcp_io_timeout_ms, &feedback);
#else
    const bool ok = false;
    feedback = "当前平台未实现通信测试";
#endif

    if (ok) {
        spdlog::info("gRPC通信测试成功: {}", feedback.toStdString());
    } else {
        spdlog::error("gRPC通信测试失败: {}", feedback.toStdString());
    }
    if (out_feedback) {
        *out_feedback = feedback;
    }
    return ok;
}

bool MainWindow::testTcpCommunicationImpl(QString* out_feedback)
{
    if (comm_config_.tcp_host.trimmed().isEmpty() || comm_config_.tcp_port <= 0 || comm_config_.tcp_port > 65535) {
        const QString reason = QString("主机或端口无效 host='%1', port=%2")
                                   .arg(comm_config_.tcp_host)
                                   .arg(comm_config_.tcp_port);
        if (out_feedback) {
            *out_feedback = reason;
        }
        spdlog::error("TCP通信测试失败: 主机或端口无效 host='{}', port={}",
                      comm_config_.tcp_host.toStdString(),
                      comm_config_.tcp_port);
        return false;
    }

    std::array<uint8_t, 16> payload{};
    payload[10] = 0x00;
    payload[11] = 0xFF;  // opcode=0xFF00, 对应BEGIN帧

    spdlog::info("开始TCP通信测试: target={}:{}, payload={}",
                 comm_config_.tcp_host.toStdString(),
                 comm_config_.tcp_port,
                 bytesToHex(QByteArray(reinterpret_cast<const char*>(payload.data()),
                                       static_cast<int>(payload.size())))
                     .toStdString());

    QString feedback;
#ifdef _WIN32
    const bool ok = probeTcpSendAndRecv(comm_config_.tcp_host,
                                        comm_config_.tcp_port,
                                        payload,
                                        comm_config_.tcp_io_timeout_ms,
                                        &feedback);
#else
    const bool ok = false;
    feedback = "当前平台未实现通信测试";
#endif

    if (ok) {
        spdlog::info("TCP通信测试成功: {}", feedback.toStdString());
    } else {
        spdlog::error("TCP通信测试失败: {}", feedback.toStdString());
    }
    if (out_feedback) {
        *out_feedback = feedback;
    }
    return ok;
}

bool MainWindow::testSerialCommunicationImpl(QString* out_feedback)
{
    const QString serial_port = comm_config_.serial_port.trimmed();
    if (serial_port.isEmpty()) {
        if (out_feedback) {
            *out_feedback = "串口号为空";
        }
        spdlog::error("串口通信测试失败: 串口号为空");
        return false;
    }

    spdlog::info("开始串口通信测试: port={}, baud={}",
                 serial_port.toStdString(),
                 comm_config_.serial_baud);

#ifdef _WIN32
    const QString device = serial_port.startsWith("\\\\.\\") ? serial_port : QString("\\\\.\\%1").arg(serial_port);
    const std::wstring device_path = device.toStdWString();

    HANDLE serial_handle = CreateFileW(device_path.c_str(),
                                       GENERIC_READ | GENERIC_WRITE,
                                       0,
                                       nullptr,
                                       OPEN_EXISTING,
                                       0,
                                       nullptr);
    if (serial_handle == INVALID_HANDLE_VALUE) {
        const QString reason = QString("打开%1失败, %2")
                                   .arg(device)
                                   .arg(winSockErrorString(GetLastError()));
        if (out_feedback) {
            *out_feedback = reason;
        }
        spdlog::error("串口通信测试失败: 打开{}失败, {}",
                      device.toStdString(),
                      winSockErrorString(GetLastError()).toStdString());
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(serial_handle, &dcb)) {
        const DWORD code = GetLastError();
        CloseHandle(serial_handle);
        if (out_feedback) {
            *out_feedback = QString("读取串口状态失败, code=%1").arg(static_cast<unsigned long>(code));
        }
        spdlog::error("串口通信测试失败: 读取串口状态失败, code={}", static_cast<unsigned long>(code));
        return false;
    }

    dcb.BaudRate = static_cast<DWORD>(comm_config_.serial_baud);
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    if (!SetCommState(serial_handle, &dcb)) {
        const DWORD code = GetLastError();
        CloseHandle(serial_handle);
        if (out_feedback) {
            *out_feedback = QString("设置串口参数失败, code=%1").arg(static_cast<unsigned long>(code));
        }
        spdlog::error("串口通信测试失败: 设置串口参数失败, code={}", static_cast<unsigned long>(code));
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 200;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 200;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(serial_handle, &timeouts);

    const QByteArray probe = QByteArrayLiteral("NBCAM_SERIAL_TEST\r\n");
    DWORD bytes_written = 0;
    const BOOL write_ok = WriteFile(serial_handle, probe.constData(), static_cast<DWORD>(probe.size()), &bytes_written, nullptr);
    const DWORD write_error = write_ok ? ERROR_SUCCESS : GetLastError();

    char read_buf[64] = {0};
    DWORD bytes_read = 0;
    const BOOL read_ok = ReadFile(serial_handle, read_buf, sizeof(read_buf), &bytes_read, nullptr);
    const QByteArray feedback_bytes = (read_ok && bytes_read > 0) ? QByteArray(read_buf, static_cast<int>(bytes_read)) : QByteArray();

    CloseHandle(serial_handle);

    if (!write_ok) {
        if (out_feedback) {
            *out_feedback = QString("写入失败, code=%1").arg(static_cast<unsigned long>(write_error));
        }
        spdlog::error("串口通信测试失败: 写入失败, code={}", static_cast<unsigned long>(write_error));
        return false;
    }

    const QString success_feedback = QString("写入%1字节, 反馈=%2")
                                         .arg(static_cast<unsigned long>(bytes_written))
                                         .arg(bytesToHex(feedback_bytes));
    spdlog::info("串口通信测试成功: 写入{}字节, 反馈={}",
                 static_cast<unsigned long>(bytes_written),
                 bytesToHex(feedback_bytes).toStdString());
    if (out_feedback) {
        *out_feedback = success_feedback;
    }
    return true;
#else
    if (out_feedback) {
        *out_feedback = "当前平台不支持该实现";
    }
    spdlog::warn("串口通信测试未实现: 当前平台不支持该实现");
    return false;
#endif
}

void MainWindow::testGrpcCommunication()
{
    if (comm_config_.executor_backend == ExecutorBackend::RTC6) {
        statusBar()->showMessage("RTC-6模式下已跳过gRPC测试", 3000);
        return;
    }
    const bool ok = testGrpcCommunicationImpl();
    statusBar()->showMessage(ok ? "gRPC通信测试成功" : "gRPC通信测试失败", 3000);
}

void MainWindow::testTcpCommunication()
{
    if (comm_config_.executor_backend == ExecutorBackend::RTC6) {
        statusBar()->showMessage("RTC-6模式下已跳过TCP测试", 3000);
        return;
    }
    const bool ok = testTcpCommunicationImpl();
    statusBar()->showMessage(ok ? "TCP通信测试成功" : "TCP通信测试失败", 3000);
}

void MainWindow::testSerialCommunication()
{
    const bool ok = testSerialCommunicationImpl();
    statusBar()->showMessage(ok ? "串口通信测试成功" : "串口通信测试失败", 3000);
}

void MainWindow::connectCommunication()
{
    try {
        if (!configureCommunicationSettings()) {
            statusBar()->showMessage("已取消连接", 2000);
            return;
        }

        const bool use_rtc6 = (comm_config_.executor_backend == ExecutorBackend::RTC6);
        const bool grpc_test_enabled = !use_rtc6 && comm_config_.enable_grpc_test;
        QString grpc_feedback = use_rtc6 ? "SKIPPED" : (grpc_test_enabled ? QString() : "SKIPPED");
        QString tcp_feedback = use_rtc6 ? "SKIPPED" : QString();
        QString serial_feedback = "SKIPPED";
        bool grpc_ok = use_rtc6 ? true : (grpc_test_enabled ? testGrpcCommunicationImpl(&grpc_feedback) : true);
        bool tcp_ok = use_rtc6 ? true : testTcpCommunicationImpl(&tcp_feedback);
        if (!grpc_test_enabled && !use_rtc6) {
            spdlog::info("gRPC通信测试已跳过");
        }

        bool serial_ok = true;
        if (comm_config_.enable_serial_test) {
            serial_ok = testSerialCommunicationImpl(&serial_feedback);
        } else {
            spdlog::info("串口通信测试已跳过");
        }

        nbcam::LaserJob* job = controller_ ? controller_->getCurrentJob() : nullptr;
        if (!use_rtc6 && grpc_test_enabled && job && job->isValid()) {
            QString log_dir = QDir::current().absoluteFilePath("log/test");
            const QString cmake_path = resolveExampleFile("CMakeLists.txt");
            if (!cmake_path.isEmpty()) {
                log_dir = QDir(QFileInfo(cmake_path).absolutePath()).absoluteFilePath("log/test");
            }
            QDir().mkpath(log_dir);

            std::array<uint16_t, 8> first_frame{};
            std::string frame_map_log_path;
            if (nbcam::exportJobFramesForDemo(*job, log_dir.toStdString(), &frame_map_log_path, &first_frame)) {
                std::string concurrent_log_path;
                bool grpc_send_ok = false;
                bool tcp_send_ok = false;
                std::string grpc_send_feedback;
                std::string tcp_send_feedback;
                const bool send_ok = nbcam::sendFrameGrpcTcpConcurrent(
                    first_frame,
                    log_dir.toStdString(),
                    &concurrent_log_path,
                    comm_config_.grpc_endpoint.toStdString(),
                    comm_config_.tcp_host.toStdString(),
                    static_cast<uint16_t>(comm_config_.tcp_port),
                    kCommTimeoutMs,
                    &grpc_send_ok,
                    &grpc_send_feedback,
                    &tcp_send_ok,
                    &tcp_send_feedback);

                captureGeneratedLog(frame_map_log_path);
                captureGeneratedLog(concurrent_log_path);

                grpc_ok = grpc_ok && grpc_send_ok;
                tcp_ok = tcp_ok && tcp_send_ok;
                if (!grpc_send_feedback.empty()) {
                    grpc_feedback += QString(" | frame_send=%1").arg(QString::fromStdString(grpc_send_feedback));
                }
                if (!tcp_send_feedback.empty()) {
                    tcp_feedback += QString(" | frame_send=%1").arg(QString::fromStdString(tcp_send_feedback));
                }
                spdlog::info("连接流程帧发送验证: ok={}, grpc_ok={}, tcp_ok={}", send_ok, grpc_send_ok, tcp_send_ok);
            } else {
                spdlog::warn("连接流程帧发送验证跳过: 导出首帧失败");
            }
        } else if (!use_rtc6 && !grpc_test_enabled) {
            spdlog::info("连接流程首帧gRPC/TCP并发验证已跳过: gRPC测试未启用");
        }

        const bool all_ok = serial_ok && (use_rtc6 ? true : (tcp_ok && (!grpc_test_enabled || grpc_ok)));

        spdlog::info("连接流程完成: backend={}, grpc={}, tcp={}, serial={}, all={}",
                     use_rtc6 ? "rtc6" : "datagenerator",
                     grpc_ok,
                     tcp_ok,
                     serial_ok,
                     all_ok);
        if (use_rtc6) {
            spdlog::info("RTC-6链路已启用: card={}, units/mm={}, program='{}'",
                         comm_config_.rtc6_card_no,
                         comm_config_.rtc6_units_per_mm,
                         comm_config_.rtc6_program_file.toStdString());
        } else {
            spdlog::info("杰普特软件对接预留: 当前版本仅提供串口连通性测试与日志输出，业务指令暂未接入。");
        }

        std::string frame_log_path;
        if (job && job->isValid()) {
            QString log_dir = QDir::current().absoluteFilePath("log/test");
            const QString cmake_path = resolveExampleFile("CMakeLists.txt");
            if (!cmake_path.isEmpty()) {
                log_dir = QDir(QFileInfo(cmake_path).absolutePath()).absoluteFilePath("log/test");
            }
            QDir().mkpath(log_dir);

            // 连接阶段只导出轻量级首帧/首包验证日志，避免对大任务做完整插补展开。
            const bool frame_dump_ok = nbcam::exportJobFramesForDemo(
                *job,
                log_dir.toStdString(),
                &frame_log_path,
                nullptr);

            if (frame_dump_ok) {
                captureGeneratedLog(frame_log_path);
                spdlog::info("连接数据帧日志已生成(轻量模式): {}", current_log_path_.toStdString());
            } else {
                spdlog::warn("连接数据帧日志生成失败，目录: {}", log_dir.toStdString());
            }
        } else {
            spdlog::warn("连接数据帧日志跳过: 当前无有效LaserJob");
        }

        const QString grpc_state = use_rtc6 ? "跳过" : (grpc_test_enabled ? (grpc_ok ? "成功" : "失败") : "跳过");
        const QString serial_state = comm_config_.enable_serial_test ? (serial_ok ? "成功" : "失败") : "跳过";
        const QString summary = use_rtc6
            ? QString("连接结果: 设备=RTC-6(card=%1), 串口=%2")
                  .arg(comm_config_.rtc6_card_no)
                  .arg(serial_state)
            : QString("连接结果: 设备=DataGenerator, gRPC=%1, TCP=%2, 串口=%3")
                  .arg(grpc_state)
                  .arg(tcp_ok ? "成功" : "失败")
                  .arg(serial_state);
        statusBar()->showMessage(summary, 6000);

        if (all_ok) {
            QMessageBox::information(this, "连接", summary);
        } else {
            QMessageBox::warning(this, "连接", summary + "\n请查看spdlog定位失败原因。");
        }
    } catch (const std::exception& e) {
        spdlog::error("连接流程异常: {}", e.what());
        QMessageBox::critical(this, "连接异常", QString("连接过程中发生异常：%1").arg(e.what()));
    } catch (...) {
        spdlog::error("连接流程发生未知异常");
        QMessageBox::critical(this, "连接异常", "连接过程中发生未知异常。");
    }
}

void MainWindow::openCurrentLog()
{
    if (current_log_path_.isEmpty()) {
        QMessageBox::information(this, "日志", "当前没有可打开的日志，请先生成路径日志。");
        return;
    }

    const QString abs_log_path = QFileInfo(current_log_path_).absoluteFilePath();
    if (!QFileInfo::exists(abs_log_path)) {
        QMessageBox::warning(this, "日志", QString("日志文件不存在：\n%1").arg(abs_log_path));
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(abs_log_path))) {
        QMessageBox::warning(this, "日志", QString("打开日志失败：\n%1").arg(abs_log_path));
        return;
    }

    statusBar()->showMessage(QString("已打开日志: %1").arg(QFileInfo(abs_log_path).fileName()), 4000);
}

void MainWindow::clearCurrentLog()
{
    if (current_log_path_.isEmpty()) {
        QMessageBox::information(this, "清理日志", "当前没有可清理的日志。");
        return;
    }

    const QString abs_log_path = QFileInfo(current_log_path_).absoluteFilePath();
    if (!QFileInfo::exists(abs_log_path)) {
        current_log_path_.clear();
        QMessageBox::warning(this, "清理日志", QString("日志文件不存在：\n%1").arg(abs_log_path));
        return;
    }

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "清理当前日志",
        QString("确认清理当前日志？\n\n%1").arg(abs_log_path),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    if (QFile::remove(abs_log_path)) {
        current_log_path_.clear();
        statusBar()->showMessage("当前日志已清理", 3000);
        spdlog::info("用户清理当前日志: {}", abs_log_path.toStdString());
    } else {
        QMessageBox::warning(this, "清理日志", QString("删除失败：\n%1").arg(abs_log_path));
    }
}

void MainWindow::clearAllLogs()
{
    const QString log_root = resolveProjectLogRoot();
    if (!QDir(log_root).exists()) {
        QMessageBox::information(this, "清理日志", QString("日志目录不存在：\n%1").arg(log_root));
        return;
    }

    QStringList log_files;
    QDirIterator it(log_root, QStringList() << "*.log", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        log_files.push_back(QFileInfo(it.next()).absoluteFilePath());
    }

    if (log_files.isEmpty()) {
        QMessageBox::information(this, "清理日志", "log目录下没有日志文件。");
        return;
    }

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "清理所有日志",
        QString("确认清理log目录下全部日志文件？\n\n目录: %1\n文件数量: %2")
            .arg(log_root)
            .arg(log_files.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    int removed_count = 0;
    int failed_count = 0;
    const QString current_log_abs = QFileInfo(current_log_path_).absoluteFilePath();
    for (const QString& file_path : log_files) {
        if (QFile::remove(file_path)) {
            ++removed_count;
            if (!current_log_abs.isEmpty() && QFileInfo(file_path).absoluteFilePath() == current_log_abs) {
                current_log_path_.clear();
            }
        } else {
            ++failed_count;
        }
    }

    const QString summary = QString("日志清理完成：删除%1个，失败%2个").arg(removed_count).arg(failed_count);
    statusBar()->showMessage(summary, 5000);
    if (failed_count == 0) {
        QMessageBox::information(this, "清理日志", summary);
    } else {
        QMessageBox::warning(this, "清理日志", summary + "\n请检查文件占用或权限。");
    }
    spdlog::info("用户清理所有日志: root={}, removed={}, failed={}",
                 log_root.toStdString(),
                 removed_count,
                 failed_count);
}

void MainWindow::ensureManualOperationDialog()
{
    if (manual_operation_dialog_) {
        return;
    }

    manual_operation_dialog_ = new QDialog(this);
    manual_operation_dialog_->setWindowTitle("手动操作");
    manual_operation_dialog_->resize(320, 300);
    manual_operation_dialog_->setModal(false);

    auto* layout = new QVBoxLayout(manual_operation_dialog_);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* title_label = new QLabel("手动操作占位界面（功能预留）", manual_operation_dialog_);
    layout->addWidget(title_label);

    auto* grid = new QGridLayout();
    grid->setSpacing(10);
    layout->addLayout(grid);

    const QString arrow_path = resolveExampleFile("assets/view/arrow.png");
    const QString stop_path = resolveExampleFile("assets/view/stop.png");
    const QPixmap arrow_base(arrow_path);
    const QPixmap stop_base(stop_path);
    spdlog::info("手动操作图标路径: arrow='{}', stop='{}'",
                 arrow_path.toStdString(),
                 stop_path.toStdString());

    auto create_arrow_label = [&arrow_base](const QString& fallback_text, double rotate_deg) {
        auto* label = new QLabel();
        label->setFixedSize(72, 72);
        label->setAlignment(Qt::AlignCenter);
        label->setText(fallback_text);
        if (!arrow_base.isNull()) {
            const QPixmap rotated = arrow_base.transformed(QTransform().rotate(rotate_deg), Qt::SmoothTransformation);
            label->setPixmap(rotated.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            label->setText(QString());
        }
        return label;
    };

    auto* up_label = create_arrow_label("上", 0.0);
    auto* down_label = create_arrow_label("下", 180.0);
    auto* left_label = create_arrow_label("左", -90.0);
    auto* right_label = create_arrow_label("右", 90.0);
    auto* stop_label = new QLabel();
    stop_label->setFixedSize(72, 72);
    stop_label->setAlignment(Qt::AlignCenter);
    stop_label->setText("停止");
    if (!stop_base.isNull()) {
        stop_label->setPixmap(stop_base.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        stop_label->setText(QString());
    }

    grid->addWidget(up_label, 0, 1, Qt::AlignCenter);
    grid->addWidget(left_label, 1, 0, Qt::AlignCenter);
    grid->addWidget(stop_label, 1, 1, Qt::AlignCenter);
    grid->addWidget(right_label, 1, 2, Qt::AlignCenter);
    grid->addWidget(down_label, 2, 1, Qt::AlignCenter);

    layout->addStretch(1);
}

void MainWindow::openManualOperationPanel()
{
    ensureManualOperationDialog();
    if (!manual_operation_dialog_) {
        return;
    }

    manual_operation_dialog_->show();
    manual_operation_dialog_->raise();
    manual_operation_dialog_->activateWindow();
}

void MainWindow::connectSignals()
{
    // 连接控制器信号
    connect(controller_, &ApplicationController::modelLoaded,
            this, &MainWindow::onModelLoaded);
    connect(controller_, &ApplicationController::parameterizationCompleted,
            this, &MainWindow::onParameterizationCompleted);
    connect(controller_, &ApplicationController::patternGenerated,
            this, &MainWindow::onPatternGenerated);
    connect(controller_, &ApplicationController::pathMapped,
            this, &MainWindow::onPathMapped);
    connect(controller_, &ApplicationController::jobReady,
            this, &MainWindow::onJobReady);
    connect(controller_, &ApplicationController::executionStatusChanged,
            this, &MainWindow::onExecutionStatusChanged);

    if (model_view_) {
        connect(model_view_, &ModelView::patchSelected, this, [this](int patch_id) {
            onPatchSelected(patch_id);
            updateUIWithoutMesh();
        });
    }

    if (parameter_panel_) {
        connect(parameter_panel_, &ParameterPanel::replanRequested,
                this, &MainWindow::onPlanPath);
    }

}

void MainWindow::updateUI()
{
    if (!controller_) {
        return;
    }
    
    // 更新3D视图（只在mesh改变时更新，避免清除patch选中状态）
    if (model_view_ && controller_->getCurrentMesh()) {
        model_view_->setMesh(controller_->getCurrentMesh());
        model_view_->resetCamera();
    }
    
    // 更新其他UI组件
    updateUIWithoutMesh();
}

void MainWindow::updateUIWithoutMesh()
{
    if (!controller_) {
        return;
    }
    
    // 更新UV视图
    if (uv_view_) {
        uv_view_->clear();

        int selected_patch_id = model_view_ ? model_view_->getSelectedPatchId() : -1;
        if (texture_list_widget_) {
            texture_list_widget_->setSelectedPatchId(selected_patch_id);
        }
        if (selected_patch_id >= 0 && model_view_ && controller_->getCurrentMesh()) {
            const auto& patches = model_view_->getPatches();
            if (selected_patch_id < static_cast<int>(patches.size())) {
                const auto& patch = patches[selected_patch_id];
                std::vector<nbcam::Triangle> display_triangles;
                display_triangles.reserve(patch.triangle_indices.size());
                for (size_t tri_idx : patch.triangle_indices) {
                    if (tri_idx < controller_->getCurrentMesh()->triangles.size()) {
                        display_triangles.push_back(controller_->getCurrentMesh()->triangles[tri_idx]);
                    }
                }

                const bool uv_ready_for_patch =
                    controller_->hasParameterization() &&
                    controller_->getParameterizedPatchId() == selected_patch_id &&
                    !display_triangles.empty();

                if (uv_ready_for_patch) {
                    const auto& uv_all = controller_->getUVCoords();
                    std::vector<char> vertex_in_patch(uv_all.size(), 0);
                    for (const auto& tri : display_triangles) {
                        if (tri.v0 < vertex_in_patch.size()) {
                            vertex_in_patch[tri.v0] = 1;
                        }
                        if (tri.v1 < vertex_in_patch.size()) {
                            vertex_in_patch[tri.v1] = 1;
                        }
                        if (tri.v2 < vertex_in_patch.size()) {
                            vertex_in_patch[tri.v2] = 1;
                        }
                    }

                    std::vector<nbcam::UVCoord> display_uv = uv_all;
                    const double nan = std::numeric_limits<double>::quiet_NaN();
                    for (size_t i = 0; i < display_uv.size(); ++i) {
                        if (!vertex_in_patch[i]) {
                            display_uv[i].u = nan;
                            display_uv[i].v = nan;
                        }
                    }
                    uv_view_->setUVCoords(display_uv);
                    uv_view_->setTriangles(display_triangles);

                    const bool path_ready_for_patch =
                        controller_->hasPattern() &&
                        controller_->getPatternPatchId() == selected_patch_id;
                    if (hasSavedPathForPatch(selected_patch_id)) {
                        auto it = saved_patch_uv_paths_.find(selected_patch_id);
                        if (it != saved_patch_uv_paths_.end() && !it->second.empty()) {
                            std::vector<nbcam::UVPathPoint> saved_uv_path;
                            saved_uv_path.reserve(it->second.size());
                            for (const auto& p : it->second) {
                                nbcam::UVPathPoint uvp;
                                uvp.u = p.u;
                                uvp.v = p.v;
                                uvp.is_jump_before = p.jump_before;
                                uvp.is_arrow_tip = false;
                                saved_uv_path.push_back(uvp);
                            }
                            if (!saved_uv_path.empty()) {
                                saved_uv_path.back().is_arrow_tip = true;
                            }
                            uv_view_->setUVPath(saved_uv_path);
                        } else {
                            uv_view_->setUVPath({});
                        }
                    } else if (path_ready_for_patch) {
                        const auto& uv_path = controller_->getUVPath();
                        if (uv_path.size() > 8000) {
                            static bool warned_once = false;
                            if (!warned_once) {
                                spdlog::warn("UV路径点数过多（{}），为避免Qt绘制崩溃，UV视图中不显示完整路径", uv_path.size());
                                warned_once = true;
                            }
                            uv_view_->setUVPath({});
                        } else {
                            uv_view_->setUVPath(uv_path);
                        }
                    } else {
                        uv_view_->setUVPath({});
                    }

                    if (texture_list_widget_) {
                        const TextureInfo info = texture_list_widget_->getTexture(selected_patch_id);
                        if (info.patch_id >= 0 && !info.svg_filepath.empty()) {
                            double u_min = 0.0, u_max = 0.0, v_min = 0.0, v_max = 0.0;
                            if (computePatchUVBounds(selected_patch_id, u_min, u_max, v_min, v_max)) {
                                uv_view_->setTexture(selected_patch_id, info.svg_filepath,
                                                     u_min, u_max, v_min, v_max,
                                                     info.scale_x, info.scale_y,
                                                     info.translate_x, info.translate_y, info.rotation_deg);
                            }
                        }
                    }
                }
            }
        }
    }

    nbcam::LaserJob* job = buildDisplayJobForView();
    if (uv_view_) {
        uv_view_->setJob(job);
    }

    // 更新路径预览
    if (path_preview_) {
        path_preview_->updateJob(job);
    }
    
    // 更新3D视图中的路径
    if (model_view_) {
        model_view_->setPathSpacingHint(controller_ ? controller_->getPlannedSpacingHint() : -1.0);
        if (job && job->segments.size() > 500) {
            spdlog::info("updateUIWithoutMesh: updating 3D path actor, segments={}, points={}",
                         job->segments.size(), job->getTotalPointCount());
        }
        model_view_->setJob(job);
        if (job && job->segments.size() > 500) {
            spdlog::info("updateUIWithoutMesh: 3D path actor update finished");
        }
    }

    refreshExecuteActionState();
}

void MainWindow::openModel()
{
    QString fileName = openFileSafe(
        this,
        "打开模型文件",
        "",
        "模型文件 (*.obj *.stl);;所有文件(*.*)"
    );
    
    if (!fileName.isEmpty()) {
        QProgressDialog progress("正在加载模型...", "取消", 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();
        
        bool success = controller_->loadModel(fileName.toStdString());
        progress.close();
        
        if (success) {
            current_model_path_ = QFileInfo(fileName).absoluteFilePath();
            statusBar()->showMessage("模型已加载: " + fileName, 3000);
            updateUI();
        } else {
            QMessageBox::warning(this, "错误", "模型加载失败！");
        }
    }
}

void MainWindow::exportModel()
{
    if (!controller_ || !controller_->getCurrentMesh() || !controller_->getCurrentMesh()->isValid()) {
        QMessageBox::warning(this, "警告", "当前没有可导出的模型！");
        return;
    }

    QString fileName = saveFileSafe(
        this,
        "导出模型文件",
        "",
        "模型文件 (*.obj *.stl);;OBJ文件 (*.obj);;STL文件 (*.stl)"
    );

    if (fileName.isEmpty()) {
        return;
    }

    QFileInfo info(fileName);
    if (info.suffix().isEmpty()) {
        fileName += ".obj";
    }

    const bool success = controller_->saveCurrentMesh(fileName.toStdString());
    if (success) {
        statusBar()->showMessage("模型已导出: " + fileName, 4000);
    } else {
        QMessageBox::warning(this, "错误", "模型导出失败！");
    }
}

void MainWindow::parameterize()
{
    if (!controller_->getCurrentMesh()) {
        QMessageBox::warning(this, "警告", "请先加载模型！");
        return;
    }
    
    // 获取当前选择的算法
    std::string algorithm = "LSCM";
    if (param_arap_action_ && param_arap_action_->isChecked()) {
        algorithm = "ARAP";
    }
    
    QProgressDialog progress("正在参数化...", "取消", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    
    bool success = controller_->parameterizeMesh(algorithm);
    progress.close();
    
    if (success) {
        updateUI();
    }
}

// setParameterizationAlgorithm函数已移除，参数化算法选择在parameterizeSelectedPatch中处理
void MainWindow::toggleWireframe3D(bool enabled)
{
    if (model_view_) {
        model_view_->setWireframeMode(enabled);
    }
}

void MainWindow::toggleWireframeUV(bool enabled)
{
    if (uv_view_) {
        uv_view_->setWireframeMode(enabled);
    }
}

void MainWindow::toggleBoundingBox(bool enabled)
{
    if (model_view_) {
        model_view_->setBoundingBoxVisible(enabled);
    }
}

void MainWindow::toggleAxes(bool enabled)
{
    if (model_view_) {
        model_view_->setAxesVisible(enabled);
    }
}


void MainWindow::importSVG()
{
    if (!controller_->getCurrentMesh()) {
        QMessageBox::warning(this, "警告", "请先加载模型！");
        return;
    }
    
    if (!controller_->hasParameterization()) {
        QMessageBox::warning(this, "警告", "请先进行参数化！");
        return;
    }
    
    // 打开文件选择对话框
    QString filepath = openFileSafe(
        this,
        "导入SVG图案",
        "",
        "SVG文件 (*.svg);;所有文件(*.*)"
    );
    
    if (filepath.isEmpty()) {
        return;
    }
    
    try {
        QProgressDialog progress("正在导入SVG...", "取消", 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();
        
        // 暂停VTK渲染以避免DirectX错误
        if (model_view_) {
            model_view_->setUpdatesEnabled(false);
        }
        
        // 导入SVG并平铺（均匀映射到上表面）        // 计算平铺尺寸：根据SVG的实际尺寸（29mm x 14mm）和模型大小
        // 这里使用较小的平铺单元以实现均匀分布
        double tile_u = 0.05;  // 每个平铺单元占UV空间的5%
        double tile_v = 0.05;
        
        // 如果已参数化，可以计算上表面的UV范围
        std::vector<double> uv_bounds;  // 空表示使用整个UV空间
        
        bool success = controller_->importSVGWithTiling(filepath.toStdString(), tile_u, tile_v, uv_bounds);
        progress.close();
        
        // 恢复VTK渲染
        if (model_view_) {
            model_view_->setUpdatesEnabled(true);
        }
        
        if (success) {
            // 映射到XYZ
            progress.setLabelText("正在映射到XYZ...");
            progress.show();
            if (model_view_) {
                model_view_->setUpdatesEnabled(false);
            }
            
            bool mapped = controller_->mapToXYZ();
            progress.close();
            
            if (model_view_) {
                model_view_->setUpdatesEnabled(true);
            }
            
            if (mapped) {
                current_svg_path_ = QFileInfo(filepath).absoluteFilePath();
                // 分配工艺参数（使用默认值）
                controller_->assignProcessParams("curvature");
                
                // 路径规划（生成mark/jump分段）- 已注释                // controller_->planPath();
                
                // 更新UI
                updateUI();
                statusBar()->showMessage("SVG导入成功", 3000);
            } else {
                QMessageBox::warning(this, "错误", "UV到XYZ映射失败！");
            }
        } else {
            QMessageBox::warning(this, "错误", "SVG导入失败！");
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "错误", QString("导入异常: %1").arg(e.what()));
    }
}

void MainWindow::generatePattern()
{
    if (!controller_) {
        QMessageBox::warning(this, "警告", "控制器未初始化！");
        return;
    }
    
    if (!controller_->hasParameterization()) {
        QMessageBox::warning(this, "警告", "请先进行参数化！");
        return;
    }
    
    if (!parameter_panel_) {
        QMessageBox::warning(this, "警告", "参数面板未初始化！");
        return;
    }
    
    try {
        // 获取参数面板的参数
        double spacing = parameter_panel_->getSpacing();
        double angle = parameter_panel_->getAngle();
        QString strategy = parameter_panel_->getStrategy();
        const QString direction_mode = parameter_panel_->getDirectionMode();
        if (direction_mode == "锁定U轴") {
            angle = 0.0;
        } else if (direction_mode == "锁定V轴") {
            angle = 90.0;
        }
        
        QProgressDialog progress("正在生成图案...", "取消", 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();
        
        // 暂停VTK渲染以避免DirectX错误
        if (model_view_) {
            model_view_->setUpdatesEnabled(false);
        }
        
        bool success = controller_->generatePattern(strategy.toStdString(), spacing, angle);
        progress.close();
        
        // 恢复VTK渲染
        if (model_view_) {
            model_view_->setUpdatesEnabled(true);
        }
        
        if (success) {
            // 映射到XYZ并分配参数
            if (controller_->mapToXYZ()) {
                controller_->assignProcessParams("curvature");
                
                /* 路径规划 - 已注释
                if (controller_->planPath()) {
                    // 延迟更新UI以避免DirectX错误
                    QTimer::singleShot(100, this, [this]() {
                        if (this) {
                            updateUI();
                        }
                    });
                } else {
                    QMessageBox::warning(this, "警告", "路径规划失败！");
                }
                */
            } else {
                QMessageBox::warning(this, "警告", "UV到XYZ映射失败！");
            }
        } else {
            QMessageBox::warning(this, "警告", "图案生成失败！");
        }
    } catch (const std::exception& e) {
        spdlog::error("生成图案时发生异常: {}", e.what());
        QMessageBox::critical(this, "错误", 
            QString("生成图案失败: %1").arg(e.what()));
        
        // 恢复VTK渲染
        if (model_view_) {
            model_view_->setUpdatesEnabled(true);
        }
    } catch (...) {
        spdlog::error("生成图案时发生未知异常");
        QMessageBox::critical(this, "错误", "生成图案时发生未知错误");
        
        // 恢复VTK渲染
        if (model_view_) {
            model_view_->setUpdatesEnabled(true);
        }
    }
}

void MainWindow::executeJob()
{
    if (!controller_ || !controller_->getCurrentJob() || !controller_->getCurrentJob()->isValid()) {
        QMessageBox::warning(this, "警告", "没有可执行的任务！");
        return;
    }

    nbcam::LaserJob* const job = controller_->getCurrentJob();
    const bool use_rtc6 = (comm_config_.executor_backend == ExecutorBackend::RTC6);
    const bool dry_run_only = comm_config_.dry_run_only;
    bool live_laser_output_enabled = false;
    bool segment_feedback_enabled = true;
    if (!dry_run_only) {
        if (!queryMachiningStartOptions(this, "执行任务", &live_laser_output_enabled, &segment_feedback_enabled)) {
            statusBar()->showMessage("已取消下发", 3000);
            return;
        }
    }
    if (!dry_run_only) {
        QString region_error;
        if (!currentModelFitsProcessingRegion(&region_error)) {
            QMessageBox::warning(this,
                                 "预检失败",
                                 QString("当前模型仍超出加工区域，禁止真实下发。\n\n%1").arg(region_error));
            statusBar()->showMessage("模型超出加工区域，已阻止真实下发", 5000);
            return;
        }
    }

    if (!dry_run_only && comm_config_.require_preexecute_comm_check) {
        if (!use_rtc6) {
            QString tcp_feedback;
            if (!testTcpCommunicationImpl(&tcp_feedback)) {
                QMessageBox::warning(this,
                                     "联锁失败",
                                     QString("TCP链路检查失败，已阻止真实下发。\n\n%1").arg(tcp_feedback));
                return;
            }
        }

        if (comm_config_.enable_serial_test) {
            QString serial_feedback;
            if (!testSerialCommunicationImpl(&serial_feedback)) {
                QMessageBox::warning(this,
                                     "联锁失败",
                                     QString("串口联锁检查失败，已阻止真实下发。\n\n%1").arg(serial_feedback));
                return;
            }
        }
    }

    if (use_rtc6) {
        nbcam::Rtc6Executor executor;
        nbcam::Rtc6Executor::MachineConfig machine_config;
        machine_config.card_no = static_cast<uint32_t>(comm_config_.rtc6_card_no);
        machine_config.dry_run_only = dry_run_only;
        machine_config.live_laser_enabled = !dry_run_only;
        machine_config.mask_laser_output = !dry_run_only && !live_laser_output_enabled;
        machine_config.apply_job_transform = comm_config_.apply_job_transform;
        machine_config.units_per_mm = comm_config_.rtc6_units_per_mm;
        machine_config.default_mark_speed_mm_s = comm_config_.rtc6_mark_speed_mm_s;
        machine_config.default_jump_speed_mm_s = comm_config_.rtc6_jump_speed_mm_s;
        machine_config.program_file = comm_config_.rtc6_program_file.toStdString();
        machine_config.delay.laser_on_delay_us = comm_config_.laser_on_delay_us;
        machine_config.delay.laser_off_delay_us = comm_config_.laser_off_delay_us;
        machine_config.delay.mark_delay_us = comm_config_.mark_delay_us;
        machine_config.delay.jump_delay_us = comm_config_.jump_delay_us;
        machine_config.delay.polygon_delay_us = comm_config_.polygon_delay_us;
        machine_config.x_axis.offset_mm = comm_config_.x_offset_mm;
        machine_config.x_axis.limit_min_mm = comm_config_.x_min_mm;
        machine_config.x_axis.limit_max_mm = comm_config_.x_max_mm;
        machine_config.y_axis.offset_mm = comm_config_.y_offset_mm;
        machine_config.y_axis.limit_min_mm = comm_config_.y_min_mm;
        machine_config.y_axis.limit_max_mm = comm_config_.y_max_mm;
        machine_config.z_axis.offset_mm = comm_config_.z_offset_mm;
        machine_config.z_axis.limit_min_mm = comm_config_.z_min_mm;
        machine_config.z_axis.limit_max_mm = comm_config_.z_max_mm;
        executor.setMachineConfig(machine_config);

        std::string preflight_error;
        nbcam::Rtc6Executor::PreflightReport report;
        if (!executor.validateJob(*job, &preflight_error, &report)) {
            QMessageBox::warning(this, "预检失败", QString::fromStdString(preflight_error));
            statusBar()->showMessage("预检失败", 4000);
            return;
        }

        if (!dry_run_only) {
            const QString confirm_text = QString(
                "即将执行当前任务到RTC-6。\n\n"
                "CardNo: %1\n"
                "总点数: %2\n"
                "Mark点: %3\n"
                "Jump点: %4\n"
                "激光输出: %5\n\n"
                "请确认硬件急停、光闸和加工环境均已就绪。")
                                            .arg(comm_config_.rtc6_card_no)
                                            .arg(static_cast<qulonglong>(report.total_points))
                                            .arg(static_cast<qulonglong>(report.mark_points))
                                            .arg(static_cast<qulonglong>(report.jump_points))
                                            .arg(live_laser_output_enabled ? "开光" : "不开光");
            if (QMessageBox::question(this, "确认RTC-6下发", confirm_text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
                statusBar()->showMessage("已取消下发", 3000);
                return;
            }
        }

        if (!executor.initialize()) {
            QMessageBox::warning(this, "错误", "RTC-6执行器初始化失败！");
            return;
        }

        statusBar()->showMessage(dry_run_only ? "RTC-6 Dry Run执行中" : "RTC-6任务执行中", 0);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        if (!executor.execute(*job)) {
            QMessageBox::warning(this, "错误", "任务执行失败！");
            return;
        }

        statusBar()->showMessage(dry_run_only ? "RTC-6 Dry Run完成" : "RTC-6任务执行完成", 5000);
        return;
    }

    nbcam::BoardExecutor executor;
    nbcam::BoardExecutor::MachineConfig machine_config;
    machine_config.transport.host = comm_config_.tcp_host.toStdString();
    machine_config.transport.port = static_cast<uint16_t>(comm_config_.tcp_port);
    machine_config.transport.connect_retry_ms = comm_config_.tcp_connect_retry_ms;
    machine_config.transport.max_connect_attempts = std::max(1, comm_config_.tcp_max_connect_attempts);
    machine_config.transport.io_timeout_ms = comm_config_.tcp_io_timeout_ms;
    machine_config.apply_job_transform = comm_config_.apply_job_transform;
    machine_config.dry_run_only = dry_run_only;
    machine_config.live_laser_enabled = !dry_run_only;
    machine_config.mask_laser_output = !dry_run_only && !live_laser_output_enabled;
    machine_config.axis_mode = (comm_config_.axis_mode == AxisMode::FiveAxis)
        ? nbcam::BoardExecutor::AxisMode::FiveAxis
        : nbcam::BoardExecutor::AxisMode::ThreeAxis;
    machine_config.x_axis.offset_mm = comm_config_.x_offset_mm;
    machine_config.x_axis.limit_min_mm = comm_config_.x_min_mm;
    machine_config.x_axis.limit_max_mm = comm_config_.x_max_mm;
    machine_config.y_axis.offset_mm = comm_config_.y_offset_mm;
    machine_config.y_axis.limit_min_mm = comm_config_.y_min_mm;
    machine_config.y_axis.limit_max_mm = comm_config_.y_max_mm;
    machine_config.z_axis.offset_mm = comm_config_.z_offset_mm;
    machine_config.z_axis.limit_min_mm = comm_config_.z_min_mm;
    machine_config.z_axis.limit_max_mm = comm_config_.z_max_mm;
    machine_config.a_axis.offset_mm = comm_config_.a_offset_mm;
    machine_config.a_axis.limit_min_mm = comm_config_.a_min_mm;
    machine_config.a_axis.limit_max_mm = comm_config_.a_max_mm;
    machine_config.b_axis.offset_mm = comm_config_.b_offset_mm;
    machine_config.b_axis.limit_min_mm = comm_config_.b_min_mm;
    machine_config.b_axis.limit_max_mm = comm_config_.b_max_mm;
    machine_config.delay.jump_speed_mm_s = comm_config_.jump_speed_mm_s;
    machine_config.delay.laser_on_delay_us = comm_config_.laser_on_delay_us;
    machine_config.delay.laser_off_delay_us = comm_config_.laser_off_delay_us;
    machine_config.delay.mark_delay_us = comm_config_.mark_delay_us;
    machine_config.delay.jump_delay_us = comm_config_.jump_delay_us;
    machine_config.delay.polygon_delay_us = comm_config_.polygon_delay_us;
    executor.setMachineConfig(machine_config);

    std::string preflight_error;
    nbcam::BoardExecutor::PreflightReport report;
    if (!executor.validateJob(*job, &preflight_error, &report)) {
        QMessageBox::warning(this, "预检失败", QString::fromStdString(preflight_error));
        statusBar()->showMessage("预检失败", 4000);
        return;
    }

    if (!dry_run_only) {
        const QString confirm_text = QString(
            "即将通过TCP下发当前任务。\n\n"
            "目标: %1:%2\n"
            "总点数: %3\n"
            "Mark点: %4\n"
            "Jump点: %5\n"
            "激光输出: %6\n\n"
            "请确认硬件急停、光闸和加工环境均已就绪。")
                                        .arg(comm_config_.tcp_host)
                                        .arg(comm_config_.tcp_port)
                                        .arg(static_cast<qulonglong>(report.total_points))
                                        .arg(static_cast<qulonglong>(report.mark_points))
                                        .arg(static_cast<qulonglong>(report.jump_points))
                                        .arg(live_laser_output_enabled ? "开光" : "不开光");
        if (QMessageBox::question(this, "确认真实下发", confirm_text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            statusBar()->showMessage("已取消真实下发", 3000);
            return;
        }
    }

    if (!executor.initialize()) {
        QMessageBox::warning(this, "错误", "执行器初始化失败！");
        return;
    }

    statusBar()->showMessage(dry_run_only ? "Dry Run执行中" : "TCP任务发送中", 0);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if (!executor.execute(*job)) {
        QMessageBox::warning(this, "错误", "任务执行失败！");
        return;
    }

    if (dry_run_only) {
        QMessageBox::information(this,
                                 "Dry Run完成",
                                 "当前任务Dry Run完成。");
    } else {
        statusBar()->showMessage("TCP任务发送完成", 5000);
    }
}

void MainWindow::about()
{
    QMessageBox::about(this, "关于NBCAM",
        "<h2>NBCAM v0.0.2</h2>"
        "<p>All Right Reserved by dcj</p>"
        "<p>开发文档请参考 docs/ 目录</p>");
}

void MainWindow::openCommandLineMode()
{
    const QString exe_path = QCoreApplication::applicationFilePath();
    const QString working_dir = QDir::currentPath();
    bool started = false;

#ifdef _WIN32
    QString escaped_exe_path = QDir::toNativeSeparators(exe_path);
    escaped_exe_path.replace('\'', "''");
    const QString command = QString("[Console]::OutputEncoding=[System.Text.Encoding]::UTF8; "
                                    "[Console]::InputEncoding=[System.Text.Encoding]::UTF8; "
                                    "& '%1' --cli")
                                .arg(escaped_exe_path);
    started = QProcess::startDetached("powershell",
                                      QStringList() << "-NoExit" << "-Command" << command,
                                      working_dir);
#endif

    if (!started) {
        started = QProcess::startDetached(exe_path, QStringList() << "--cli", working_dir);
    }

    if (!started) {
        QMessageBox::warning(this,
                             "命令行模式",
                             QString("启动CLI失败。\n\n可手动执行：\n%1 --cli")
                                 .arg(QDir::toNativeSeparators(exe_path)));
        return;
    }

    statusBar()->showMessage("已启动命令行模式", 4000);
}

void MainWindow::openModelOffsetDialog()
{
    if (!controller_ || !controller_->getCurrentMesh()) {
        QMessageBox::warning(this, "模型偏置", "请先加载模型。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("模型偏置与加工区域");
    dialog.setModal(true);
    dialog.resize(420, 320);

    auto* layout = new QVBoxLayout(&dialog);

    auto* region_check = new QCheckBox("显示加工区域黄框 (+-32.768 mm)", &dialog);
    region_check->setChecked(model_view_ ? model_view_->isProcessingRegionVisible() : true);
    layout->addWidget(region_check);

    auto* form = new QFormLayout();
    const auto current_transform = controller_->getModelTransform();

    auto* scale_spin = new QDoubleSpinBox(&dialog);
    scale_spin->setRange(0.001, 1000.0);
    scale_spin->setDecimals(4);
    scale_spin->setSingleStep(0.01);
    scale_spin->setValue(current_transform.uniform_scale);

    auto create_offset_spin = [&dialog](double value) {
        auto* spin = new QDoubleSpinBox(&dialog);
        spin->setRange(-1000.0, 1000.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.1);
        spin->setValue(value);
        return spin;
    };

    auto* x_spin = create_offset_spin(current_transform.offset_x_mm);
    auto* y_spin = create_offset_spin(current_transform.offset_y_mm);
    auto* z_spin = create_offset_spin(current_transform.offset_z_mm);

    form->addRow("等比例缩放", scale_spin);
    form->addRow("X 偏移(mm)", x_spin);
    form->addRow("Y 偏移(mm)", y_spin);
    form->addRow("Z 偏移(mm)", z_spin);
    layout->addLayout(form);

    auto* button_row = new QHBoxLayout();
    auto* auto_center_btn = new QPushButton("自动居中", &dialog);
    auto* reset_btn = new QPushButton("重置", &dialog);
    button_row->addWidget(auto_center_btn);
    button_row->addWidget(reset_btn);
    button_row->addStretch();
    layout->addLayout(button_row);

    auto apply_transform_to_ui = [this, scale_spin, x_spin, y_spin, z_spin]() {
        const auto transform = controller_->getModelTransform();
        scale_spin->setValue(transform.uniform_scale);
        x_spin->setValue(transform.offset_x_mm);
        y_spin->setValue(transform.offset_y_mm);
        z_spin->setValue(transform.offset_z_mm);
    };

    connect(region_check, &QCheckBox::toggled, &dialog, [this](bool checked) {
        if (model_view_) {
            model_view_->setProcessingRegionVisible(checked);
        }
    });

    connect(auto_center_btn, &QPushButton::clicked, &dialog, [this, apply_transform_to_ui]() {
        if (!controller_->autoCenterModelToBounds(kProcessingRegionMinMm,
                                                  kProcessingRegionMaxMm,
                                                  kProcessingRegionMinMm,
                                                  kProcessingRegionMaxMm,
                                                  kProcessingRegionMinMm,
                                                  kProcessingRegionMaxMm)) {
            QMessageBox::warning(this, "模型偏置", "自动居中失败。");
            return;
        }
        apply_transform_to_ui();
        updateUI();
        refreshModelProcessingRegionStatus();
    });

    connect(reset_btn, &QPushButton::clicked, &dialog, [this, apply_transform_to_ui]() {
        ApplicationController::ModelTransform transform;
        if (!controller_->setModelTransform(transform)) {
            QMessageBox::warning(this, "模型偏置", "重置变换失败。");
            return;
        }
        apply_transform_to_ui();
        updateUI();
        refreshModelProcessingRegionStatus();
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, this, scale_spin, x_spin, y_spin, z_spin]() {
        ApplicationController::ModelTransform transform;
        transform.uniform_scale = scale_spin->value();
        transform.offset_x_mm = x_spin->value();
        transform.offset_y_mm = y_spin->value();
        transform.offset_z_mm = z_spin->value();
        if (!controller_->setModelTransform(transform)) {
            QMessageBox::warning(this, "模型偏置", "应用模型变换失败。");
            return;
        }
        refreshModelProcessingRegionStatus();
        updateUI();
        dialog.accept();
    });

    dialog.exec();
}

void MainWindow::onModelLoaded(bool success)
{
    if (success) {
        saved_patch_jobs_.clear();
        saved_patch_uv_paths_.clear();
        display_job_cache_.reset();
        resetUVLayoutState();
        refreshModelProcessingRegionStatus();
        updateUI();
    }
}

void MainWindow::onParameterizationCompleted(bool success)
{
    if (success) {
        updateUIWithoutMesh();
    }
}

void MainWindow::onPatternGenerated(bool success)
{
    if (success) {
        updateUIWithoutMesh();
    }
}

void MainWindow::onPathMapped(bool success)
{
    if (success) {
        updateUIWithoutMesh();
    }
}

void MainWindow::onJobReady(bool ready)
{
    if (ready) {
        machining_refresh_available_ = false;
    }
    refreshModelProcessingRegionStatus();
    updateUIWithoutMesh();
}

void MainWindow::onExecutionStatusChanged(const QString& status)
{
    statusBar()->showMessage(status, 0);
}

void MainWindow::onPlanPath()
{
    if (!controller_) {
        QMessageBox::warning(this, "警告", "控制器未初始化！");
        return;
    }
    
    if (!controller_->getCurrentMesh()) {
        QMessageBox::warning(this, "警告", "请先加载模型！");
        return;
    }

    if (!model_view_) {
        QMessageBox::warning(this, "警告", "3D视图未初始化！");
        return;
    }

    const int patch_id = model_view_->getSelectedPatchId();
    if (patch_id < 0) {
        QMessageBox::warning(this, "警告", "请先在3D视图中选中一个面片，再执行路径规划。");
        return;
    }

    const std::vector<nbcam::Patch> patches = model_view_->getPatches();
    if (patch_id >= static_cast<int>(patches.size())) {
        QMessageBox::warning(this, "警告", "选中的面片ID无效。");
        return;
    }

    if (!texture_list_widget_) {
        QMessageBox::warning(this, "警告", "纹理列表未初始化！");
        return;
    }

    TextureInfo texture_info = texture_list_widget_->getTexture(patch_id);
    if (texture_info.patch_id < 0 || texture_info.svg_filepath.empty()) {
        QMessageBox::warning(this, "警告", "该面片尚未贴图。请先“应用SVG纹理到Patch”，再执行路径规划。");
        return;
    }
    current_svg_path_ = QFileInfo(QString::fromStdString(texture_info.svg_filepath)).absoluteFilePath();
    
    try {
        ApplicationController::PatternPlanOptions options;
        options.strategy = parameter_panel_ ? parameter_panel_->getStrategy().toStdString() : "轴向往返填充";
        options.spacing = parameter_panel_ ? parameter_panel_->getSpacing() : 0.1;
        options.angle_deg = parameter_panel_ ? parameter_panel_->getAngle() : 0.0;
        options.direction_mode = parameter_panel_ ? parameter_panel_->getDirectionMode().toStdString() : "按角度";
        const QString contour_mode = parameter_panel_ ? parameter_panel_->getContourProcessMode() : QStringLiteral("all");
        if (contour_mode == "none") {
            options.contour_mode = ApplicationController::PatternPlanOptions::ContourProcessMode::NONE;
        } else if (contour_mode == "contour_only") {
            options.contour_mode = ApplicationController::PatternPlanOptions::ContourProcessMode::CONTOUR_ONLY;
        } else {
            options.contour_mode = ApplicationController::PatternPlanOptions::ContourProcessMode::ALL;
        }
        options.z_layer_enabled = parameter_panel_ ? parameter_panel_->isZLayerEnabled() : false;
        options.layer_height = parameter_panel_ ? parameter_panel_->getLayerHeight() : options.spacing;
        options.scan_speed_mm_s = parameter_panel_ ? parameter_panel_->getSpeed() : 300.0;
        options.svg_filepath = texture_info.svg_filepath;
        options.tex_scale_x = texture_info.scale_x;
        options.tex_scale_y = texture_info.scale_y;
        options.tex_translate_x = texture_info.translate_x;
        options.tex_translate_y = texture_info.translate_y;
        options.tex_rotation_deg = texture_info.rotation_deg;
        options.enable_inverse_stretch_prewarp = texture_info.enable_inverse_stretch_prewarp;
        options.inverse_stretch_prewarp_strength = texture_info.inverse_stretch_prewarp_strength;

        if (controller_->generatePatternInPatch(patch_id, patches, options)) {
            double raw_u_min = 0.0, raw_u_max = 0.0, raw_v_min = 0.0, raw_v_max = 0.0;
            if (computePatchUVBounds(patch_id, raw_u_min, raw_u_max, raw_v_min, raw_v_max)) {
                registerPatchInUVLayout(patch_id, {raw_u_min, raw_u_max, raw_v_min, raw_v_max});
                refreshUVTexturesForLayout();
            }
            updateUIWithoutMesh();
            model_view_->selectPatch(patch_id);

            const QString prewarp_status = texture_info.enable_inverse_stretch_prewarp
                ? QString("补偿开, 强度=%1").arg(texture_info.inverse_stretch_prewarp_strength, 0, 'f', 2)
                : QString("补偿关");
            QString plan_status = QString("路径规划完成（按SVG图案区域，%1）").arg(prewarp_status);
            std::string frame_log_path;
            nbcam::LaserJob* job = controller_->getCurrentJob();
            updateTextureContourInfoFromJob(patch_id, job);
            if (job && job->isValid()) {
                QString log_dir = QDir::current().absoluteFilePath("log");
                const QString cmake_path = resolveExampleFile("CMakeLists.txt");
                if (!cmake_path.isEmpty()) {
                    log_dir = QDir(QFileInfo(cmake_path).absolutePath()).absoluteFilePath("log");
                }
                QDir().mkpath(log_dir);

                const bool export_ok = nbcam::exportConnectionFrameLog(
                    *job,
                    log_dir.toStdString(),
                    false,
                    "NOT_TESTED",
                    false,
                    "NOT_TESTED",
                    comm_config_.grpc_endpoint.toStdString(),
                    QString("%1:%2").arg(comm_config_.tcp_host).arg(comm_config_.tcp_port).toStdString(),
                    comm_config_.enable_serial_test,
                    false,
                    comm_config_.enable_serial_test ? "NOT_TESTED" : "SKIPPED",
                    &frame_log_path);
                if (export_ok) {
                    captureGeneratedLog(frame_log_path);
                    spdlog::info("路径规划连接帧日志已生成: {}", current_log_path_.toStdString());
                    plan_status += QString("，日志=%1")
                                       .arg(QFileInfo(current_log_path_).fileName());
                } else {
                    spdlog::warn("路径规划连接帧日志导出失败，目录: {}", log_dir.toStdString());
                }
            } else {
                spdlog::warn("路径规划连接帧日志跳过: 当前无有效LaserJob");
            }

            statusBar()->showMessage(plan_status, 5000);
        } else {
            QMessageBox::warning(this, "警告",
                "路径规划失败！\n\n请确保已：\n1. 参数化选中Patch\n2. 对该Patch完成SVG贴图\n3. 间隔参数大于0");
        }
    } catch (const std::exception& e) {
        spdlog::error("路径规划异常: {}", e.what());
        QMessageBox::critical(this, "错误", QString("路径规划异常: %1").arg(e.what()));
    } catch (...) {
        spdlog::error("路径规划未知异常");
        QMessageBox::critical(this, "错误", "路径规划发生未知错误");
    }
}

void MainWindow::clusterPatches()
{
    if (!model_view_ || !controller_->getCurrentMesh()) {
        QMessageBox::warning(this, "警告", "请先加载模型！");
        return;
    }
    
    // 从设置中读取上次使用的阈值
    QSettings settings;
    double default_threshold = settings.value("patch_clustering/dihedral_threshold_deg", 20.0).toDouble();
    
    // 让用户输入二面角阈值（角度）
    bool ok;
    double threshold_deg = QInputDialog::getDouble(
        this,
        "面片聚类参数",
        QString("请输入二面角阈值（角度）：\n\n"),
        default_threshold,  // 使用上次保存的值作为默认值
        0.1,      // 最小值（0.1度）
        180.0,    // 最大值（180度）
        1,        // 小数位数
        &ok
    );
    
    if (!ok) {
        return;  // 用户取消
    }
    
    // 保存阈值到设置
    settings.setValue("patch_clustering/dihedral_threshold_deg", threshold_deg);
    
    // 转换为弧度（算法需要弧度）
    double threshold_rad = threshold_deg * M_PI / 180.0;
    
    QString progress_text = QString("正在进行面片聚类（二面角阈值: %1°）...").arg(threshold_deg, 0, 'f', 1);
    QProgressDialog progress(progress_text, "取消", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    
    model_view_->clusterPatches(threshold_rad);
    
    progress.close();
    
    const std::vector<nbcam::Patch> patches = model_view_->getPatches();
    QString status_msg = QString("面片聚类完成: %1 个面片（阈值: %2°）")
        .arg(patches.size())
        .arg(threshold_deg, 0, 'f', 1);
    statusBar()->showMessage(status_msg, 5000);
}

void MainWindow::onPatchSelected(int patch_id)
{
    if (texture_list_widget_) {
        texture_list_widget_->setSelectedPatchId(patch_id);
    }

    if (patch_id < 0) {
        statusBar()->showMessage("已取消选择面片", 2000);
        return;
    }
    
    const auto& patches = model_view_->getPatches();
    if (patch_id < static_cast<int>(patches.size())) {
        const auto& patch = patches[patch_id];
        QString msg = QString("已选择面片 %1: %2 个三角形, 面积: %3 (现在可以进行参数化和导入SVG)")
            .arg(patch_id)
            .arg(patch.triangle_indices.size())
            .arg(patch.total_area, 0, 'f', 4);
        statusBar()->showMessage(msg, 5000);
    }
}

void MainWindow::parameterizeSelectedPatch()
{
    if (!model_view_ || !controller_->getCurrentMesh()) {
        QMessageBox::warning(this, "警告", "请先加载模型！");
        return;
    }
    
    int patch_id = model_view_->getSelectedPatchId();
    if (patch_id < 0) {
        QMessageBox::warning(this, "警告", "请先选择一个面片！\n\n提示：在3D视图中左键点击一个面片来选中它！");
        return;
    }
    
    const auto& patches = model_view_->getPatches();
    if (patch_id >= static_cast<int>(patches.size())) {
        QMessageBox::warning(this, "警告", "无效的面片ID！");
        return;
    }
    
    // 创建算法选择对话框
    QDialog dialog(this);
    dialog.setWindowTitle("选择参数化算法");
    dialog.setModal(true);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QRadioButton* lscm_radio = new QRadioButton("LSCM (最小二乘保形映射)", &dialog);
    QRadioButton* arap_radio = new QRadioButton("ARAP (尽可能刚性)", &dialog);
    QRadioButton* authalic_radio = new QRadioButton("Authalic (保面积优先)", &dialog);
    arap_radio->setChecked(true);  // 默认选择ARAP（验证示例流程）
    
    layout->addWidget(lscm_radio);
    layout->addWidget(arap_radio);
    layout->addWidget(authalic_radio);
    
    QDialogButtonBox* button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(button_box);
    
    if (dialog.exec() != QDialog::Accepted) {
        return;  // 用户取消
    }
    
    // 确定选择的算法
    std::string algorithm = "ARAP";
    if (lscm_radio->isChecked()) {
        algorithm = "LSCM";
    } else if (authalic_radio->isChecked()) {
        algorithm = "AUTHALIC";
    }
    const QString algorithm_label =
        (algorithm == "LSCM") ? "LSCM" : ((algorithm == "AUTHALIC") ? "Authalic" : "ARAP");
    
    QString progress_text = QString("正在对面片 %1 进行参数化（%2）...")
        .arg(patch_id)
        .arg(algorithm_label);
    QProgressDialog progress(progress_text, "取消", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    
    bool success = controller_->parameterizePatch(patch_id, patches, algorithm);
    progress.close();
    
    if (success) {
        double raw_u_min = 0.0, raw_u_max = 0.0, raw_v_min = 0.0, raw_v_max = 0.0;
        if (computePatchUVBounds(patch_id, raw_u_min, raw_u_max, raw_v_min, raw_v_max)) {
            registerPatchInUVLayout(patch_id, {raw_u_min, raw_u_max, raw_v_min, raw_v_max});
            refreshUVTexturesForLayout();
        }

        QString msg = QString("面片 %1 参数化完成（%2），已保持选中状态，可以导入SVG").arg(patch_id).arg(algorithm_label);
        statusBar()->showMessage(msg, 5000);
        
        // 更新UI（但不调用setMesh，避免清除patch数据）
        updateUIWithoutMesh();
        
        // 强制重新设置选中状态和高亮，确保patch保持高亮显示
        // 这样后续导入SVG时可以直接使用这个patch
        model_view_->selectPatch(patch_id);
        
        // 延迟再次确保高亮
        QTimer::singleShot(100, this, [this, patch_id]() {
            if (this && model_view_) {
                model_view_->selectPatch(patch_id);  // 再次确保高亮
            }
        });
    } else {
        QMessageBox::warning(this, "错误", QString("面片参数化失败（算法: %1）！").arg(algorithm.c_str()));
    }
}

/*  // SVG路径规划功能已注释
void MainWindow::importSVGToPatch()
{
    if (!model_view_ || !controller_->getCurrentMesh()) {
        QMessageBox::warning(this, "警告", "请先加载模型！");
        return;
    }
    
    int patch_id = model_view_->getSelectedPatchId();
    if (patch_id < 0) {
        QMessageBox::warning(this, "警告", "请先选择一个面片！\n\n提示：在3D视图中左键点击一个面片来选中它！");
        return;
    }
    
    const auto& patches = model_view_->getPatches();
    if (patch_id >= static_cast<int>(patches.size())) {
        QMessageBox::warning(this, "警告", "无效的面片ID！");
        return;
    }
    
    // 打开文件选择对话框
    QString filepath = openFileSafe(
        this,
        QString("导入SVG图案到面片 %1").arg(patch_id),
        "",
        "SVG文件 (*.svg);;所有文件(*.*)"
    );
    
    if (filepath.isEmpty()) {
        return;
    }
    
    QProgressDialog progress(QString("正在导入SVG到面片 %1...").arg(patch_id), "取消", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    
    bool success = controller_->mapSVGToPatch(patch_id, filepath.toStdString(), patches);
    progress.close();
    
    if (success) {
        QString msg = QString("SVG已映射到面片 %1（使用已选定的面片）").arg(patch_id);
        statusBar()->showMessage(msg, 5000);
        
        // 更新UI（但不调用setMesh，避免清除patch数据）
        updateUIWithoutMesh();
        
        // 确保patch保持高亮状态
        model_view_->selectPatch(patch_id);
        
        // 延迟再次确保高亮
        QTimer::singleShot(100, this, [this, patch_id]() {
            if (this && model_view_) {
                model_view_->selectPatch(patch_id);  // 再次确保高亮
            }
        });
    } else {
        QMessageBox::warning(this, "错误", "SVG映射失败！");
    }
}
*/

void MainWindow::applyTextureToPatch()
{
    if (!model_view_ || !controller_->getCurrentMesh()) {
        QMessageBox::warning(this, "警告", "请先加载模型！");
        return;
    }
    
    int patch_id = model_view_->getSelectedPatchId();
    if (patch_id < 0) {
        QMessageBox::warning(this, "警告", "请先选择一个面片！\n\n提示：在3D视图中左键点击一个面片来选中它！");
        return;
    }
    
    const auto& patches = model_view_->getPatches();
    if (patch_id >= static_cast<int>(patches.size())) {
        QMessageBox::warning(this, "警告", "无效的面片ID！");
        return;
    }
    
    // 检查是否已参数化
    if (!controller_->hasParameterization() || 
        controller_->getUVCoords().size() != controller_->getCurrentMesh()->vertices.size()) {
        QMessageBox::warning(this, "警告", "请先对面片进行参数化！");
        return;
    }
    
    // 打开文件选择对话框
    QString filepath = openFileSafe(
        this,
        QString("应用SVG纹理到面片 %1").arg(patch_id),
        "",
        "SVG文件 (*.svg);;所有文件(*.*)"
    );
    
    if (filepath.isEmpty()) {
        return;
    }
    
    // 从设置中读取上次使用的纹理参数
    QSettings settings;
    int default_width = settings.value("texture/width", 1024).toInt();
    int default_height = settings.value("texture/height", 1024).toInt();
    double default_opacity = settings.value("texture/opacity", 1.0).toDouble();
    double default_ambient = settings.value("texture/ambient", 0.3).toDouble();
    double default_diffuse = settings.value("texture/diffuse", 0.7).toDouble();
    double default_specular = settings.value("texture/specular", 0.0).toDouble();
    
    // 创建纹理参数设置对话框
    QDialog dialog(this);
    dialog.setWindowTitle("SVG纹理参数设置");
    dialog.setModal(true);
    dialog.resize(400, 300);
    
    QVBoxLayout* main_layout = new QVBoxLayout(&dialog);
    
    // 纹理分辨率组
    QGroupBox* resolution_group = new QGroupBox("纹理分辨率", &dialog);
    QFormLayout* resolution_layout = new QFormLayout(resolution_group);
    
    QSpinBox* width_spin = new QSpinBox(&dialog);
    width_spin->setRange(256, 4096);
    width_spin->setSingleStep(256);
    width_spin->setValue(default_width);
    width_spin->setSuffix(" 像素");
    resolution_layout->addRow("宽度:", width_spin);
    
    QSpinBox* height_spin = new QSpinBox(&dialog);
    height_spin->setRange(256, 4096);
    height_spin->setSingleStep(256);
    height_spin->setValue(default_height);
    height_spin->setSuffix(" 像素");
    resolution_layout->addRow("高度:", height_spin);
    
    main_layout->addWidget(resolution_group);
    
    // 渲染属性组
    QGroupBox* render_group = new QGroupBox("渲染属性", &dialog);
    QFormLayout* render_layout = new QFormLayout(render_group);
    
    QDoubleSpinBox* opacity_spin = new QDoubleSpinBox(&dialog);
    opacity_spin->setRange(0.0, 1.0);
    opacity_spin->setSingleStep(0.1);
    opacity_spin->setDecimals(2);
    opacity_spin->setValue(default_opacity);
    render_layout->addRow("不透明度", opacity_spin);
    
    QDoubleSpinBox* ambient_spin = new QDoubleSpinBox(&dialog);
    ambient_spin->setRange(0.0, 1.0);
    ambient_spin->setSingleStep(0.1);
    ambient_spin->setDecimals(2);
    ambient_spin->setValue(default_ambient);
    render_layout->addRow("环境光", ambient_spin);
    
    QDoubleSpinBox* diffuse_spin = new QDoubleSpinBox(&dialog);
    diffuse_spin->setRange(0.0, 1.0);
    diffuse_spin->setSingleStep(0.1);
    diffuse_spin->setDecimals(2);
    diffuse_spin->setValue(default_diffuse);
    render_layout->addRow("漫反射", diffuse_spin);
    
    QDoubleSpinBox* specular_spin = new QDoubleSpinBox(&dialog);
    specular_spin->setRange(0.0, 1.0);
    specular_spin->setSingleStep(0.1);
    specular_spin->setDecimals(2);
    specular_spin->setValue(default_specular);
    render_layout->addRow("镜面反射:", specular_spin);
    
    main_layout->addWidget(render_group);
    
    // 按钮
    QDialogButtonBox* button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    main_layout->addWidget(button_box);
    
    if (dialog.exec() != QDialog::Accepted) {
        return;  // 用户取消
    }
    
    // 获取用户设置的参数
    int texture_width = width_spin->value();
    int texture_height = height_spin->value();
    double opacity = opacity_spin->value();
    double ambient = ambient_spin->value();
    double diffuse = diffuse_spin->value();
    double specular = specular_spin->value();
    
    // 保存参数到设置
    settings.setValue("texture/width", texture_width);
    settings.setValue("texture/height", texture_height);
    settings.setValue("texture/opacity", opacity);
    settings.setValue("texture/ambient", ambient);
    settings.setValue("texture/diffuse", diffuse);
    settings.setValue("texture/specular", specular);
    
    // 从纹理编辑器获取变换参数（如果存在）
    double scale_x = 1.0, scale_y = 1.0;
    double translate_x = 0.0, translate_y = 0.0;
    double rotation_deg = 0.0;
    bool enable_inverse_stretch_prewarp = true;
    double inverse_stretch_prewarp_strength = 1.2;
    
    if (texture_editor_dialog_) {
        scale_x = texture_editor_dialog_->getScaleX();
        scale_y = texture_editor_dialog_->getScaleY();
        translate_x = texture_editor_dialog_->getTranslateX();
        translate_y = texture_editor_dialog_->getTranslateY();
        rotation_deg = texture_editor_dialog_->getRotation();
        enable_inverse_stretch_prewarp = texture_editor_dialog_->getInverseStretchPrewarpEnabled();
        inverse_stretch_prewarp_strength = texture_editor_dialog_->getInverseStretchPrewarpStrength();
    }

    QProgressDialog progress(QString("正在应用SVG纹理到面片 %1...").arg(patch_id), "取消", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    
    // 应用纹理（使用用户设置的参数和变换参数）
    model_view_->applyTextureToPatch(patch_id, filepath.toStdString(), 
                                     controller_->getUVCoords(),
                                     texture_width, texture_height,
                                     opacity, ambient, diffuse, specular,
                                     scale_x, scale_y, translate_x, translate_y, rotation_deg);
    current_svg_path_ = QFileInfo(filepath).absoluteFilePath();
    
    progress.close();
    
    // 更新纹理列表
    if (texture_list_widget_) {
        TextureInfo info;
        info.patch_id = patch_id;
        info.svg_filepath = filepath.toStdString();
        info.texture_width = texture_width;
        info.texture_height = texture_height;
        info.opacity = opacity;
        info.ambient = ambient;
        info.diffuse = diffuse;
        info.specular = specular;
        info.scale_x = scale_x;
        info.scale_y = scale_y;
        info.translate_x = translate_x;
        info.translate_y = translate_y;
        info.rotation_deg = rotation_deg;
        info.enable_inverse_stretch_prewarp = enable_inverse_stretch_prewarp;
        info.inverse_stretch_prewarp_strength = inverse_stretch_prewarp_strength;
        info.has_saved_path = hasSavedPathForPatch(patch_id);
        
        texture_list_widget_->addTexture(patch_id, info);
        updateTextureContourInfoFromJob(
            patch_id,
            hasSavedPathForPatch(patch_id) ? saved_patch_jobs_[patch_id].get() : controller_->getCurrentJob());
    }
    
    double raw_u_min = 0.0, raw_u_max = 0.0, raw_v_min = 0.0, raw_v_max = 0.0;
    if (computePatchUVBounds(patch_id, raw_u_min, raw_u_max, raw_v_min, raw_v_max)) {
        registerPatchInUVLayout(patch_id, {raw_u_min, raw_u_max, raw_v_min, raw_v_max});
    }
    refreshUVTexturesForLayout();
    
    // 更新其他UI组件
    updateUIWithoutMesh();
    
    QString msg = QString("SVG纹理已应用到面片 %1").arg(patch_id);
    statusBar()->showMessage(msg, 5000);
    
    // 确保patch保持高亮
    model_view_->selectPatch(patch_id);
}

void MainWindow::adaptiveSubdivideAndExportStl()
{
    if (!controller_ || !controller_->getCurrentMesh()) {
        QMessageBox::warning(this, "警告", "请先加载模型！");
        return;
    }

    const auto* mesh_before = controller_->getCurrentMesh();
    if (!mesh_before || !mesh_before->isValid()) {
        QMessageBox::warning(this, "警告", "当前模型无效，无法细分！");
        return;
    }

    constexpr int kMaxTriangles = 80000;
    const int current_triangles = static_cast<int>(mesh_before->getTriangleCount());
    int target_triangles = current_triangles;

    if (current_triangles < kMaxTriangles) {
        const int default_target = std::max(current_triangles, 5000);
        bool ok = false;
        target_triangles = QInputDialog::getInt(
            this,
            "自适应均匀细分",
            QString("当前三角形数: %1\n请输入目标三角形数（上限 %2）：")
                .arg(current_triangles)
                .arg(kMaxTriangles),
            default_target,
            current_triangles,
            kMaxTriangles,
            100,
            &ok
        );
        if (!ok) {
            return;
        }
    } else {
        QMessageBox::information(this,
                                 "提示",
                                 QString("当前模型三角形数已达到/超过上限（%1），将跳过细分并直接导出。")
                                     .arg(kMaxTriangles));
    }

    int applied_iterations = 0;
    if (!controller_->adaptiveSubdivideCurrentMesh(static_cast<size_t>(target_triangles),
                                                   static_cast<size_t>(kMaxTriangles),
                                                   6,
                                                   &applied_iterations)) {
        QMessageBox::warning(this, "错误", "自适应均匀细分失败。");
        return;
    }

    updateUI();

    const auto* mesh_after = controller_->getCurrentMesh();
    const int final_triangles = mesh_after ? static_cast<int>(mesh_after->getTriangleCount()) : current_triangles;

    statusBar()->showMessage(
        QString("细分完成：iters=%1，三角形 %2 -> %3（可在 文件->导出模型 导出OBJ/STL）")
            .arg(applied_iterations)
            .arg(current_triangles)
            .arg(final_triangles),
        8000);
}

void MainWindow::measureArea()
{
    if (!controller_ || !controller_->getCurrentMesh() || !controller_->getCurrentMesh()->isValid()) {
        QMessageBox::warning(this, "警告", "请先加载有效模型！");
        return;
    }

    const double total_area = computeMeshTotalArea();
    QString message = QString("整模型面积: %1 mm²").arg(total_area, 0, 'f', 6);

    if (model_view_) {
        const int selected_patch_id = model_view_->getSelectedPatchId();
        const auto& patches = model_view_->getPatches();
        if (selected_patch_id >= 0 && selected_patch_id < static_cast<int>(patches.size())) {
            const auto& patch = patches[selected_patch_id];
            message += QString("\n选中面片面积(ID=%1): %2 mm²")
                           .arg(patch.id)
                           .arg(patch.total_area, 0, 'f', 6);
        }
    }

    QMessageBox::information(this, "面积测量", message);
}

void MainWindow::measureSurfacePointCoordinate()
{
    if (!controller_ || !controller_->getCurrentMesh() || !controller_->getCurrentMesh()->isValid()) {
        QMessageBox::warning(this, "警告", "请先加载有效模型！");
        return;
    }

    size_t vertex_index = 0;
    if (!queryVertexIndex("曲面点坐标", vertex_index)) {
        return;
    }

    const auto* mesh = controller_->getCurrentMesh();
    if (vertex_index >= mesh->vertices.size()) {
        QMessageBox::warning(this, "错误", "顶点索引越界。");
        return;
    }

    const auto& v = mesh->vertices[vertex_index];
    QMessageBox::information(
        this,
        "曲面点坐标",
        QString("顶点 #%1 坐标:\nX = %2 mm\nY = %3 mm\nZ = %4 mm")
            .arg(vertex_index)
            .arg(v.x, 0, 'f', 6)
            .arg(v.y, 0, 'f', 6)
            .arg(v.z, 0, 'f', 6));
}

void MainWindow::measureLineDistance()
{
    if (!controller_ || !controller_->getCurrentMesh() || !controller_->getCurrentMesh()->isValid()) {
        QMessageBox::warning(this, "警告", "请先加载有效模型！");
        return;
    }

    size_t index_a = 0;
    size_t index_b = 0;
    if (!queryTwoVertexIndices("两点直线距离", index_a, index_b)) {
        return;
    }

    const auto* mesh = controller_->getCurrentMesh();
    if (index_a >= mesh->vertices.size() || index_b >= mesh->vertices.size()) {
        QMessageBox::warning(this, "错误", "顶点索引越界。");
        return;
    }

    const auto& a = mesh->vertices[index_a];
    const auto& b = mesh->vertices[index_b];
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    QMessageBox::information(
        this,
        "两点直线距离",
        QString("顶点 #%1 与 #%2 的直线距离:\n%3 mm")
            .arg(index_a)
            .arg(index_b)
            .arg(distance, 0, 'f', 6));
}

void MainWindow::measureGeodesicDistance()
{
    if (!controller_ || !controller_->getCurrentMesh() || !controller_->getCurrentMesh()->isValid()) {
        QMessageBox::warning(this, "警告", "请先加载有效模型！");
        return;
    }

    size_t index_a = 0;
    size_t index_b = 0;
    if (!queryTwoVertexIndices("两点测地线距离", index_a, index_b)) {
        return;
    }

    const auto* mesh = controller_->getCurrentMesh();
    if (index_a >= mesh->vertices.size() || index_b >= mesh->vertices.size()) {
        QMessageBox::warning(this, "错误", "顶点索引越界。");
        return;
    }

    const double geodesic = computeGeodesicDistance(index_a, index_b);
    if (!std::isfinite(geodesic)) {
        QMessageBox::warning(this, "测地线距离", "两点不连通，无法计算曲面测地线距离。");
        return;
    }

    const auto& a = mesh->vertices[index_a];
    const auto& b = mesh->vertices[index_b];
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    const double line_distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    QMessageBox::information(
        this,
        "两点测地线距离",
        QString("顶点 #%1 与 #%2:\n直线距离 = %3 mm\n测地线距离 = %4 mm")
            .arg(index_a)
            .arg(index_b)
            .arg(line_distance, 0, 'f', 6)
            .arg(geodesic, 0, 'f', 6));
}

bool MainWindow::queryVertexIndex(const QString& title, size_t& out_index) const
{
    if (!controller_ || !controller_->getCurrentMesh()) {
        return false;
    }

    const auto* mesh = controller_->getCurrentMesh();
    if (mesh->vertices.empty()) {
        QMessageBox::warning(const_cast<MainWindow*>(this), "警告", "当前模型没有可用顶点。");
        return false;
    }

    const size_t max_idx_size_t =
        std::min(mesh->vertices.size() - 1, static_cast<size_t>(std::numeric_limits<int>::max()));
    bool ok = false;
    const int value = QInputDialog::getInt(const_cast<MainWindow*>(this),
                                           title,
                                           QString("请输入顶点索引 [0, %1]:").arg(max_idx_size_t),
                                           0,
                                           0,
                                           static_cast<int>(max_idx_size_t),
                                           1,
                                           &ok);
    if (!ok) {
        return false;
    }
    out_index = static_cast<size_t>(value);
    return true;
}

bool MainWindow::queryTwoVertexIndices(const QString& title, size_t& index_a, size_t& index_b) const
{
    if (!controller_ || !controller_->getCurrentMesh()) {
        return false;
    }

    const auto* mesh = controller_->getCurrentMesh();
    if (mesh->vertices.empty()) {
        QMessageBox::warning(const_cast<MainWindow*>(this), "警告", "当前模型没有可用顶点。");
        return false;
    }

    const size_t max_idx_size_t =
        std::min(mesh->vertices.size() - 1, static_cast<size_t>(std::numeric_limits<int>::max()));
    const int max_idx = static_cast<int>(max_idx_size_t);

    bool ok = false;
    const int first = QInputDialog::getInt(const_cast<MainWindow*>(this),
                                           title,
                                           QString("请输入第一个顶点索引 [0, %1]:").arg(max_idx_size_t),
                                           0,
                                           0,
                                           max_idx,
                                           1,
                                           &ok);
    if (!ok) {
        return false;
    }

    const int second = QInputDialog::getInt(const_cast<MainWindow*>(this),
                                            title,
                                            QString("请输入第二个顶点索引 [0, %1]:").arg(max_idx_size_t),
                                            first,
                                            0,
                                            max_idx,
                                            1,
                                            &ok);
    if (!ok) {
        return false;
    }

    index_a = static_cast<size_t>(first);
    index_b = static_cast<size_t>(second);
    return true;
}

double MainWindow::computeMeshTotalArea() const
{
    if (!controller_ || !controller_->getCurrentMesh()) {
        return 0.0;
    }

    const auto* mesh = controller_->getCurrentMesh();
    double total_area = 0.0;
    for (const auto& tri : mesh->triangles) {
        if (tri.v0 >= mesh->vertices.size() ||
            tri.v1 >= mesh->vertices.size() ||
            tri.v2 >= mesh->vertices.size()) {
            continue;
        }

        const auto& a = mesh->vertices[tri.v0];
        const auto& b = mesh->vertices[tri.v1];
        const auto& c = mesh->vertices[tri.v2];

        const double abx = b.x - a.x;
        const double aby = b.y - a.y;
        const double abz = b.z - a.z;
        const double acx = c.x - a.x;
        const double acy = c.y - a.y;
        const double acz = c.z - a.z;

        const double cx = aby * acz - abz * acy;
        const double cy = abz * acx - abx * acz;
        const double cz = abx * acy - aby * acx;
        total_area += 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
    }
    return total_area;
}

double MainWindow::computeGeodesicDistance(size_t start_idx, size_t end_idx) const
{
    if (!controller_ || !controller_->getCurrentMesh()) {
        return std::numeric_limits<double>::infinity();
    }

    const auto* mesh = controller_->getCurrentMesh();
    const size_t vertex_count = mesh->vertices.size();
    if (start_idx >= vertex_count || end_idx >= vertex_count) {
        return std::numeric_limits<double>::infinity();
    }
    if (start_idx == end_idx) {
        return 0.0;
    }

    std::vector<std::vector<std::pair<size_t, double>>> adjacency(vertex_count);
    const auto add_edge = [&](size_t a, size_t b) {
        if (a >= vertex_count || b >= vertex_count) {
            return;
        }
        const auto& va = mesh->vertices[a];
        const auto& vb = mesh->vertices[b];
        const double dx = va.x - vb.x;
        const double dy = va.y - vb.y;
        const double dz = va.z - vb.z;
        const double w = std::sqrt(dx * dx + dy * dy + dz * dz);
        adjacency[a].emplace_back(b, w);
        adjacency[b].emplace_back(a, w);
    };

    for (const auto& tri : mesh->triangles) {
        add_edge(tri.v0, tri.v1);
        add_edge(tri.v1, tri.v2);
        add_edge(tri.v2, tri.v0);
    }

    std::vector<double> dist(vertex_count, std::numeric_limits<double>::infinity());
    std::vector<char> visited(vertex_count, 0);
    using QueueNode = std::pair<double, size_t>;
    std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> pq;

    dist[start_idx] = 0.0;
    pq.emplace(0.0, start_idx);

    while (!pq.empty()) {
        const auto [curr_dist, u] = pq.top();
        pq.pop();

        if (visited[u]) {
            continue;
        }
        visited[u] = 1;
        if (u == end_idx) {
            return curr_dist;
        }

        for (const auto& [v, weight] : adjacency[u]) {
            if (visited[v]) {
                continue;
            }
            const double next = curr_dist + weight;
            if (next < dist[v]) {
                dist[v] = next;
                pq.emplace(next, v);
            }
        }
    }

    return std::numeric_limits<double>::infinity();
}

QString MainWindow::resolveExampleFile(const QString& relative_path) const
{
    for (const QString& base : candidateBaseDirs()) {
        const QString candidate = QDir(base).absoluteFilePath(relative_path);
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }
    return {};
}

QString MainWindow::resolveProjectLogRoot() const
{
    QString log_dir = QDir::current().absoluteFilePath("log");
    const QString cmake_path = resolveExampleFile("CMakeLists.txt");
    if (!cmake_path.isEmpty()) {
        log_dir = QDir(QFileInfo(cmake_path).absolutePath()).absoluteFilePath("log");
    }
    return QFileInfo(log_dir).absoluteFilePath();
}

QString MainWindow::sanitizeLogNamePart(const QString& raw, const QString& fallback) const
{
    QString value = QFileInfo(raw).completeBaseName().trimmed();
    if (value.isEmpty()) {
        value = fallback;
    }
    value.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
    value.replace(QRegularExpression(R"(\s+)"), "_");
    value.replace(QRegularExpression(R"(_+)"), "_");
    value = value.trimmed();
    if (value.isEmpty()) {
        value = fallback;
    }
    return value;
}

QString MainWindow::buildUnifiedLogFileName(const QString& model_path, const QString& svg_path) const
{
    const QString model_part = sanitizeLogNamePart(model_path, "unknown_model");
    const QString svg_part = sanitizeLogNamePart(svg_path, "unknown_svg");
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    return QString("frame_%1_%2-%3.log").arg(model_part, svg_part, timestamp);
}

bool MainWindow::captureGeneratedLog(const std::string& generated_log_path,
                                     const QString& model_path,
                                     const QString& svg_path)
{
    const QString source_path = QFileInfo(QString::fromStdString(generated_log_path)).absoluteFilePath();
    if (source_path.isEmpty() || !QFileInfo::exists(source_path)) {
        return false;
    }

    const QString effective_model = model_path.isEmpty() ? current_model_path_ : model_path;
    const QString effective_svg = svg_path.isEmpty() ? current_svg_path_ : svg_path;

    const QDir source_dir(QFileInfo(source_path).absolutePath());
    const QString default_target_name = buildUnifiedLogFileName(effective_model, effective_svg);
    QString target_path = source_dir.absoluteFilePath(default_target_name);
    const QString target_base_name = QFileInfo(default_target_name).completeBaseName();
    const QString target_ext = QFileInfo(default_target_name).suffix();
    int dedup_idx = 1;
    while (QFileInfo::exists(target_path) && QFileInfo(target_path).absoluteFilePath() != source_path) {
        target_path = source_dir.absoluteFilePath(
            QString("%1_%2.%3").arg(target_base_name).arg(dedup_idx++).arg(target_ext));
    }

    QString final_path = source_path;
    if (QFileInfo(target_path).absoluteFilePath() != source_path) {
        if (QFile::rename(source_path, target_path)) {
            final_path = target_path;
        } else {
            if (QFile::copy(source_path, target_path) && QFile::remove(source_path)) {
                final_path = target_path;
            } else {
                spdlog::warn("日志重命名失败，保留原文件: {}", source_path.toStdString());
            }
        }
    }

    current_log_path_ = QFileInfo(final_path).absoluteFilePath();
    spdlog::info("统一日志文件: {}", current_log_path_.toStdString());
    return true;
}

bool MainWindow::computePatchUVBounds(int patch_id, double& u_min, double& u_max, double& v_min, double& v_max) const
{
    if (!controller_ || !model_view_ || !controller_->getCurrentMesh() || !controller_->hasParameterization()) {
        return false;
    }
    const auto& patches = model_view_->getPatches();
    if (patch_id < 0 || patch_id >= static_cast<int>(patches.size())) {
        return false;
    }

    const auto& uv_coords = controller_->getUVCoords();
    const auto* mesh = controller_->getCurrentMesh();
    bool initialized = false;
    u_min = std::numeric_limits<double>::max();
    u_max = std::numeric_limits<double>::lowest();
    v_min = std::numeric_limits<double>::max();
    v_max = std::numeric_limits<double>::lowest();
    std::vector<double> u_samples;
    std::vector<double> v_samples;

    const auto collectVertex = [&](size_t v_idx) {
        if (v_idx >= uv_coords.size()) {
            return;
        }
        const auto& uv = uv_coords[v_idx];
        if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) {
            return;
        }
        u_samples.push_back(uv.u);
        v_samples.push_back(uv.v);
        if (!initialized) {
            u_min = u_max = uv.u;
            v_min = v_max = uv.v;
            initialized = true;
        } else {
            u_min = std::min(u_min, uv.u);
            u_max = std::max(u_max, uv.u);
            v_min = std::min(v_min, uv.v);
            v_max = std::max(v_max, uv.v);
        }
    };

    for (size_t i = 0; i + 1 < patches[patch_id].boundary_edges.size(); i += 2) {
        collectVertex(patches[patch_id].boundary_edges[i]);
        collectVertex(patches[patch_id].boundary_edges[i + 1]);
    }

    for (size_t tri_idx : patches[patch_id].triangle_indices) {
        if (tri_idx >= mesh->triangles.size()) {
            continue;
        }
        const auto& tri = mesh->triangles[tri_idx];
        collectVertex(tri.v0);
        collectVertex(tri.v1);
        collectVertex(tri.v2);
    }

    if (!initialized || u_max <= u_min || v_max <= v_min) {
        return false;
    }

    if (u_samples.size() >= 8 && v_samples.size() >= 8) {
        std::sort(u_samples.begin(), u_samples.end());
        std::sort(v_samples.begin(), v_samples.end());
        const auto pickPercentile = [](const std::vector<double>& values, double p) {
            const double pos = p * static_cast<double>(values.size() - 1);
            const size_t lo = static_cast<size_t>(std::floor(pos));
            const size_t hi = static_cast<size_t>(std::ceil(pos));
            if (lo == hi) {
                return values[lo];
            }
            const double t = pos - static_cast<double>(lo);
            return values[lo] * (1.0 - t) + values[hi] * t;
        };

        const double raw_u_range = u_max - u_min;
        const double raw_v_range = v_max - v_min;
        const double robust_u_min = pickPercentile(u_samples, 0.05);
        const double robust_u_max = pickPercentile(u_samples, 0.95);
        const double robust_v_min = pickPercentile(v_samples, 0.05);
        const double robust_v_max = pickPercentile(v_samples, 0.95);
        const double robust_u_range = robust_u_max - robust_u_min;
        const double robust_v_range = robust_v_max - robust_v_min;

        if (robust_u_range > 1e-9 && raw_u_range > robust_u_range * 1.5) {
            u_min = robust_u_min;
            u_max = robust_u_max;
        }
        if (robust_v_range > 1e-9 && raw_v_range > robust_v_range * 1.5) {
            v_min = robust_v_min;
            v_max = robust_v_max;
        }
    }

    return u_max > u_min && v_max > v_min;
}

void MainWindow::registerPatchInUVLayout(int patch_id, const UVDisplayRange& raw_range)
{
    patch_raw_uv_ranges_[patch_id] = raw_range;
    const bool already_exists = std::find(uv_layout_order_.begin(), uv_layout_order_.end(), patch_id) != uv_layout_order_.end();
    if (!already_exists) {
        uv_layout_order_.push_back(patch_id);
    }
    rebuildUVLayoutDisplayRanges();
}

void MainWindow::rebuildUVLayoutDisplayRanges()
{
    patch_display_ranges_.clear();
    const size_t count = uv_layout_order_.size();
    if (count == 0) {
        return;
    }

    const auto fitRawRangeToSlot = [](const UVDisplayRange& raw, const UVDisplayRange& slot) -> UVDisplayRange {
        const double raw_u = raw.u_max - raw.u_min;
        const double raw_v = raw.v_max - raw.v_min;
        const double slot_u = slot.u_max - slot.u_min;
        const double slot_v = slot.v_max - slot.v_min;
        if (!(raw_u > 1e-12) || !(raw_v > 1e-12) || !(slot_u > 1e-12) || !(slot_v > 1e-12)) {
            return slot;
        }

        const double raw_aspect = raw_u / raw_v;
        const double slot_aspect = slot_u / slot_v;
        if (!(raw_aspect > 1e-12) || !(slot_aspect > 1e-12)) {
            return slot;
        }

        UVDisplayRange fitted = slot;
        if (raw_aspect >= slot_aspect) {
            const double fitted_v = slot_u / raw_aspect;
            const double v_pad = (slot_v - fitted_v) * 0.5;
            fitted.v_min = slot.v_min + v_pad;
            fitted.v_max = slot.v_max - v_pad;
        } else {
            const double fitted_u = slot_v * raw_aspect;
            const double u_pad = (slot_u - fitted_u) * 0.5;
            fitted.u_min = slot.u_min + u_pad;
            fitted.u_max = slot.u_max - u_pad;
        }
        return fitted;
    };

    for (size_t i = 0; i < count; ++i) {
        const int patch_id = uv_layout_order_[i];
        const double u0 = static_cast<double>(i) / static_cast<double>(count);
        const double u1 = static_cast<double>(i + 1) / static_cast<double>(count);
        UVDisplayRange slot{u0, u1, 0.0, 1.0};
        auto raw_it = patch_raw_uv_ranges_.find(patch_id);
        if (raw_it != patch_raw_uv_ranges_.end()) {
            patch_display_ranges_[patch_id] = fitRawRangeToSlot(raw_it->second, slot);
        } else {
            patch_display_ranges_[patch_id] = slot;
        }
    }
}

MainWindow::UVDisplayRange MainWindow::resolveDisplayRangeForPatch(int patch_id, const UVDisplayRange& raw_fallback) const
{
    auto it = patch_display_ranges_.find(patch_id);
    if (it != patch_display_ranges_.end()) {
        return it->second;
    }
    return raw_fallback;
}

std::vector<nbcam::UVCoord> MainWindow::remapUVCoordsToDisplayRange(const std::vector<nbcam::UVCoord>& uv_coords,
                                                                    const UVDisplayRange& raw_range,
                                                                    const UVDisplayRange& display_range) const
{
    std::vector<nbcam::UVCoord> remapped = uv_coords;
    const double raw_u_range = raw_range.u_max - raw_range.u_min;
    const double raw_v_range = raw_range.v_max - raw_range.v_min;
    const double display_u_range = display_range.u_max - display_range.u_min;
    const double display_v_range = display_range.v_max - display_range.v_min;
    if (!(raw_u_range > 1e-12) || !(raw_v_range > 1e-12) ||
        !(display_u_range > 1e-12) || !(display_v_range > 1e-12)) {
        return remapped;
    }

    for (auto& uv : remapped) {
        if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) {
            continue;
        }
        const double nu = (uv.u - raw_range.u_min) / raw_u_range;
        const double nv = (uv.v - raw_range.v_min) / raw_v_range;
        uv.u = display_range.u_min + nu * display_u_range;
        uv.v = display_range.v_min + nv * display_v_range;
    }
    return remapped;
}

std::vector<nbcam::UVPathPoint> MainWindow::remapUVPathToDisplayRange(const std::vector<nbcam::UVPathPoint>& uv_path,
                                                                      const UVDisplayRange& raw_range,
                                                                      const UVDisplayRange& display_range) const
{
    std::vector<nbcam::UVPathPoint> remapped = uv_path;
    const double raw_u_range = raw_range.u_max - raw_range.u_min;
    const double raw_v_range = raw_range.v_max - raw_range.v_min;
    const double display_u_range = display_range.u_max - display_range.u_min;
    const double display_v_range = display_range.v_max - display_range.v_min;
    if (!(raw_u_range > 1e-12) || !(raw_v_range > 1e-12) ||
        !(display_u_range > 1e-12) || !(display_v_range > 1e-12)) {
        return remapped;
    }

    for (auto& pt : remapped) {
        if (!std::isfinite(pt.u) || !std::isfinite(pt.v)) {
            continue;
        }
        const double nu = (pt.u - raw_range.u_min) / raw_u_range;
        const double nv = (pt.v - raw_range.v_min) / raw_v_range;
        pt.u = display_range.u_min + nu * display_u_range;
        pt.v = display_range.v_min + nv * display_v_range;
    }
    return remapped;
}

void MainWindow::refreshUVTexturesForLayout()
{
    if (!uv_view_ || !texture_list_widget_ || !model_view_) {
        return;
    }

    const auto textures = texture_list_widget_->getAllTextures();
    for (const auto& pair : textures) {
        uv_view_->removeTexture(pair.first);
    }

    const int selected_patch_id = model_view_->getSelectedPatchId();
    if (selected_patch_id < 0) {
        return;
    }

    const auto tex_it = textures.find(selected_patch_id);
    if (tex_it == textures.end()) {
        return;
    }

    const TextureInfo& info = tex_it->second;
    if (info.patch_id < 0 || info.svg_filepath.empty()) {
        return;
    }

    double u_min = 0.0, u_max = 0.0, v_min = 0.0, v_max = 0.0;
    if (!computePatchUVBounds(selected_patch_id, u_min, u_max, v_min, v_max)) {
        return;
    }

    uv_view_->setTexture(selected_patch_id, info.svg_filepath,
                         u_min, u_max, v_min, v_max,
                         info.scale_x, info.scale_y,
                         info.translate_x, info.translate_y, info.rotation_deg);
}

void MainWindow::resetUVLayoutState()
{
    patch_raw_uv_ranges_.clear();
    patch_display_ranges_.clear();
    uv_layout_order_.clear();
}

bool MainWindow::hasSavedPathForPatch(int patch_id) const
{
    if (patch_id < 0) {
        return false;
    }
    return saved_patch_jobs_.find(patch_id) != saved_patch_jobs_.end();
}

nbcam::LaserJob* MainWindow::buildDisplayJobForView()
{
    display_job_cache_.reset();
    if (!controller_) {
        return nullptr;
    }

    nbcam::LaserJob* live_job = controller_->getCurrentJob();
    const bool live_valid = (live_job && live_job->isValid());
    const int live_patch_id = controller_->getPatternPatchId();

    if (saved_patch_jobs_.empty()) {
        return live_valid ? live_job : nullptr;
    }

    display_job_cache_ = std::make_unique<nbcam::LaserJob>();
    if (live_valid) {
        display_job_cache_->meta = live_job->meta;
        display_job_cache_->coordinate = live_job->coordinate;
        display_job_cache_->parameterization = live_job->parameterization;
        display_job_cache_->process_defaults = live_job->process_defaults;
    }

    for (const auto& pair : saved_patch_jobs_) {
        if (!pair.second || !pair.second->isValid()) {
            continue;
        }
        appendJobSegmentsDeep(*display_job_cache_, *pair.second);
    }

    const bool live_already_saved = hasSavedPathForPatch(live_patch_id);
    if (live_valid && !live_already_saved) {
        appendJobSegmentsDeep(*display_job_cache_, *live_job);
    }

    for (size_t i = 0; i < display_job_cache_->segments.size(); ++i) {
        display_job_cache_->segments[i].id = static_cast<int>(i);
    }

    if (!display_job_cache_->isValid()) {
        return live_valid ? live_job : nullptr;
    }
    return display_job_cache_.get();
}

void MainWindow::updateTextureContourInfoFromJob(int patch_id, const nbcam::LaserJob* job)
{
    if (!texture_list_widget_ || patch_id < 0) {
        return;
    }
    TextureInfo info = texture_list_widget_->getTexture(patch_id);
    if (info.patch_id < 0) {
        return;
    }
    const ContourStats stats = collectContourStats(job);
    info.contour_loop_count = stats.contour_loops;
    info.contour_mark_segment_count = stats.contour_mark_segments;
    info.contour_jump_segment_count = stats.contour_jump_segments;
    info.contour_mark_point_count = stats.contour_mark_points;
    info.has_saved_path = hasSavedPathForPatch(patch_id);
    texture_list_widget_->updateTexture(patch_id, info);
}

void MainWindow::runValidationExample(double spacing_mm, const ValidationExampleOptions& options)
{
    if (!controller_ || !model_view_) {
        return;
    }

    const QString svg_relative_path = options.svg_relative_path;
    const QString model_path = resolveExampleFile("docs/model1111.STL");
    const QString svg_path = resolveExampleFile(svg_relative_path);

    QStringList missing;
    if (model_path.isEmpty()) {
        missing << "docs/model1111.STL";
    }
    if (svg_path.isEmpty()) {
        missing << svg_relative_path;
    }
    if (!missing.isEmpty()) {
        QMessageBox::warning(this, "错误",
                             QString("验证示例缺少文件：\n%1").arg(missing.join("\n")));
        return;
    }
    current_model_path_ = QFileInfo(model_path).absoluteFilePath();
    current_svg_path_ = QFileInfo(svg_path).absoluteFilePath();

    QProgressDialog progress(QString("正在执行验证示例流程（间隔 %1 mm）...").arg(spacing_mm, 0, 'f', 3), "取消", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    if (!controller_->loadModel(model_path.toStdString())) {
        progress.close();
        QMessageBox::warning(this, "错误", "示例模型加载失败！");
        return;
    }
    if (!controller_->autoCenterModelToBounds(kProcessingRegionMinMm,
                                              kProcessingRegionMaxMm,
                                              kProcessingRegionMinMm,
                                              kProcessingRegionMaxMm,
                                              kProcessingRegionMinMm,
                                              kProcessingRegionMaxMm)) {
        spdlog::warn("验证示例: 自动居中失败，继续使用原始位置");
    }
    updateUI();

    if (options.adaptive_target_triangles > 0) {
        int applied_iterations = 0;
        if (!controller_->adaptiveSubdivideCurrentMesh(options.adaptive_target_triangles,
                                                       80000,
                                                       8,
                                                       &applied_iterations)) {
            progress.close();
            QMessageBox::warning(this, "错误",
                                 QString("自适应细分失败，目标三角形数=%1。")
                                     .arg(static_cast<qulonglong>(options.adaptive_target_triangles)));
            return;
        }
        spdlog::info("验证示例: 自适应细分完成，target_triangles={}, applied_iterations={}",
                     options.adaptive_target_triangles,
                     applied_iterations);
        updateUI();
    }

    constexpr double kDihedralDeg = 10.0;
    model_view_->clusterPatches(kDihedralDeg * M_PI / 180.0);
    const auto& patches = model_view_->getPatches();
    if (patches.empty()) {
        progress.close();
        QMessageBox::warning(this, "错误", "面片聚类失败：未生成任何面片。");
        return;
    }

    int patch_id = -1;
    auto* mesh_ptr = controller_->getCurrentMesh();
    if (mesh_ptr) {
        struct PatchHeightScore {
            int idx = -1;
            double z_max = -std::numeric_limits<double>::infinity();
            double z_mean = -std::numeric_limits<double>::infinity();
            double area = 0.0;
        };

        std::vector<PatchHeightScore> scores;
        scores.reserve(patches.size());
        double global_z_min = std::numeric_limits<double>::infinity();
        double global_z_max = -std::numeric_limits<double>::infinity();

        for (size_t i = 0; i < patches.size(); ++i) {
            const auto& patch = patches[i];
            double z_sum = 0.0;
            size_t z_count = 0;
            double patch_z_max = -std::numeric_limits<double>::infinity();
            for (size_t tri_idx : patch.triangle_indices) {
                if (tri_idx >= mesh_ptr->triangles.size()) {
                    continue;
                }
                const auto& tri = mesh_ptr->triangles[tri_idx];
                for (size_t vi : {tri.v0, tri.v1, tri.v2}) {
                    if (vi >= mesh_ptr->vertices.size()) {
                        continue;
                    }
                    const double z = mesh_ptr->vertices[vi].z;
                    if (!std::isfinite(z)) {
                        continue;
                    }
                    z_sum += z;
                    ++z_count;
                    patch_z_max = std::max(patch_z_max, z);
                    global_z_min = std::min(global_z_min, z);
                    global_z_max = std::max(global_z_max, z);
                }
            }

            if (z_count == 0) {
                continue;
            }
            PatchHeightScore score;
            score.idx = static_cast<int>(i);
            score.z_max = patch_z_max;
            score.z_mean = z_sum / static_cast<double>(z_count);
            score.area = patch.total_area;
            scores.push_back(score);
        }

        if (!scores.empty() && std::isfinite(global_z_min) && std::isfinite(global_z_max)) {
            const double z_span = std::max(1e-6, global_z_max - global_z_min);
            const double z_window = std::max(0.2, z_span * 0.08);  // 仅在顶端高度窗口内选面积最大的弧面

            int best_idx = -1;
            double best_area = -std::numeric_limits<double>::infinity();
            double best_z_mean = -std::numeric_limits<double>::infinity();
            for (const auto& s : scores) {
                if (s.z_max < global_z_max - z_window) {
                    continue;
                }
                if (s.area > best_area + 1e-9 ||
                    (std::abs(s.area - best_area) <= 1e-9 && s.z_mean > best_z_mean)) {
                    best_idx = s.idx;
                    best_area = s.area;
                    best_z_mean = s.z_mean;
                }
            }

            if (best_idx >= 0) {
                patch_id = best_idx;
            } else {
                // 兜底：取平均高度最高且面积更大的 patch
                for (const auto& s : scores) {
                    if (s.z_mean > best_z_mean + 1e-9 ||
                        (std::abs(s.z_mean - best_z_mean) <= 1e-9 && s.area > best_area)) {
                        patch_id = s.idx;
                        best_z_mean = s.z_mean;
                        best_area = s.area;
                    }
                }
            }
        }
    }

    if (patch_id < 0 || patch_id >= static_cast<int>(patches.size())) {
        progress.close();
        QMessageBox::warning(this, "错误", "未找到上方弧面片（Z轴上表面）。");
        return;
    }
    model_view_->selectPatch(patch_id);

    if (!controller_->parameterizePatch(patch_id, patches, "ARAP")) {
        progress.close();
        QMessageBox::warning(this, "错误", "ARAP 参数化失败。");
        return;
    }
    updateUIWithoutMesh();
    model_view_->selectPatch(patch_id);

    const auto& uv_coords = controller_->getUVCoords();
    constexpr int kTextureWidth = 1024;
    constexpr int kTextureHeight = 1024;
    constexpr double kOpacity = 1.0;
    constexpr double kAmbient = 0.3;
    constexpr double kDiffuse = 0.7;
    constexpr double kSpecular = 0.0;
    constexpr double kScaleX = 1.0;
    constexpr double kScaleY = 1.0;
    constexpr double kTranslateX = 0.0;
    constexpr double kTranslateY = 0.0;
    constexpr double kRotationDeg = 0.0;

    model_view_->applyTextureToPatch(patch_id, svg_path.toStdString(),
                                     uv_coords,
                                     kTextureWidth, kTextureHeight,
                                     kOpacity, kAmbient, kDiffuse, kSpecular,
                                     kScaleX, kScaleY, kTranslateX, kTranslateY, kRotationDeg);

    if (texture_list_widget_) {
        TextureInfo info;
        info.patch_id = patch_id;
        info.svg_filepath = svg_path.toStdString();
        info.texture_width = kTextureWidth;
        info.texture_height = kTextureHeight;
        info.opacity = kOpacity;
        info.ambient = kAmbient;
        info.diffuse = kDiffuse;
        info.specular = kSpecular;
        info.scale_x = kScaleX;
        info.scale_y = kScaleY;
        info.translate_x = kTranslateX;
        info.translate_y = kTranslateY;
        info.rotation_deg = kRotationDeg;
        info.has_saved_path = hasSavedPathForPatch(patch_id);
        texture_list_widget_->addTexture(patch_id, info);
    }

    if (uv_view_) {
        double u_min = 0.0;
        double u_max = 0.0;
        double v_min = 0.0;
        double v_max = 0.0;
        if (computePatchUVBounds(patch_id, u_min, u_max, v_min, v_max)) {
            registerPatchInUVLayout(patch_id, {u_min, u_max, v_min, v_max});
            refreshUVTexturesForLayout();
        }
    }

    ApplicationController::PatternPlanOptions plan_options;
    const QString strategy_text = options.strategy_override.isEmpty()
                                      ? (parameter_panel_ ? parameter_panel_->getStrategy() : QStringLiteral("轴向往返填充"))
                                      : options.strategy_override;
    plan_options.strategy = strategy_text.toStdString();
    plan_options.spacing = spacing_mm;
    plan_options.angle_deg = parameter_panel_ ? parameter_panel_->getAngle() : 0.0;
    plan_options.direction_mode = parameter_panel_ ? parameter_panel_->getDirectionMode().toStdString() : "按角度";
    const QString contour_mode = parameter_panel_ ? parameter_panel_->getContourProcessMode() : QStringLiteral("all");
    if (contour_mode == "none") {
        plan_options.contour_mode = ApplicationController::PatternPlanOptions::ContourProcessMode::NONE;
    } else if (contour_mode == "contour_only") {
        plan_options.contour_mode = ApplicationController::PatternPlanOptions::ContourProcessMode::CONTOUR_ONLY;
    } else {
        plan_options.contour_mode = ApplicationController::PatternPlanOptions::ContourProcessMode::ALL;
    }
    plan_options.z_layer_enabled = (strategy_text == "Z轴分层填充" || strategy_text == "z_layer");
    plan_options.layer_height = parameter_panel_ ? parameter_panel_->getLayerHeight() : spacing_mm;
    plan_options.scan_speed_mm_s = parameter_panel_ ? parameter_panel_->getSpeed() : 300.0;
    plan_options.svg_filepath = svg_path.toStdString();
    plan_options.tex_scale_x = kScaleX;
    plan_options.tex_scale_y = kScaleY;
    plan_options.tex_translate_x = kTranslateX;
    plan_options.tex_translate_y = kTranslateY;
    plan_options.tex_rotation_deg = kRotationDeg;
    plan_options.arc_center_x = options.arc_center_x;
    plan_options.arc_center_z = options.arc_center_z;
    plan_options.arc_radius = options.arc_radius;

    const bool success = controller_->generatePatternInPatch(patch_id, patches, plan_options);
    progress.close();
    if (!success) {
        QMessageBox::warning(this, "错误", "验证示例路径规划失败。");
        return;
    }

    updateUIWithoutMesh();
    model_view_->selectPatch(patch_id);

    QString log_dir = QDir::current().absoluteFilePath(options.export_connection_frame_log ? "log/test" : "log");
    const QString cmake_path = resolveExampleFile("CMakeLists.txt");
    if (!cmake_path.isEmpty()) {
        log_dir = QDir(QFileInfo(cmake_path).absolutePath())
                      .absoluteFilePath(options.export_connection_frame_log ? "log/test" : "log");
    }
    QDir().mkpath(log_dir);

    std::string frame_log_path;
    nbcam::LaserJob* job = controller_->getCurrentJob();
    if (job && job->isValid()) {
        if (options.export_connection_frame_log) {
            bool grpc_ok = false;
            bool tcp_ok = false;
            QString grpc_feedback = "N/A";
            QString tcp_feedback = "N/A";
            if (options.run_comm_probe_for_log) {
                grpc_ok = testGrpcCommunicationImpl(&grpc_feedback);
                tcp_ok = testTcpCommunicationImpl(&tcp_feedback);
            }

            const bool export_ok = nbcam::exportConnectionFrameLog(
                *job,
                log_dir.toStdString(),
                grpc_ok,
                grpc_feedback.toStdString(),
                tcp_ok,
                tcp_feedback.toStdString(),
                comm_config_.grpc_endpoint.toStdString(),
                QString("%1:%2").arg(comm_config_.tcp_host).arg(comm_config_.tcp_port).toStdString(),
                false,
                false,
                "SKIPPED",
                &frame_log_path);
            if (export_ok) {
                captureGeneratedLog(frame_log_path, model_path, svg_path);
                spdlog::info("验证示例连接帧日志: {}", current_log_path_.toStdString());
            } else {
                spdlog::warn("验证示例连接帧日志导出失败，目录: {}", log_dir.toStdString());
            }
        } else {
            std::string grpc_log_path;
            std::array<uint16_t, 8> first_data_frame{};
            const bool export_ok = nbcam::exportJobFramesForDemo(*job,
                                                                 log_dir.toStdString(),
                                                                 &frame_log_path,
                                                                 &first_data_frame);
            if (export_ok) {
                const bool grpc_demo_ok = nbcam::sendFrameGrpcTcpDemo(first_data_frame,
                                                                      log_dir.toStdString(),
                                                                      &grpc_log_path);
                captureGeneratedLog(frame_log_path, model_path, svg_path);
                spdlog::info("示例帧映射日志: {}", current_log_path_.toStdString());
                captureGeneratedLog(grpc_log_path, model_path, svg_path);
                spdlog::info("示例 gRPC+TCP示例日志: {}", current_log_path_.toStdString());
                if (!grpc_demo_ok) {
                    spdlog::warn("示例 gRPC+TCP单帧发送示例执行失败，请查看日志: {}", current_log_path_.toStdString());
                }
            } else {
                spdlog::warn("示例帧映射日志导出失败，目录: {}", log_dir.toStdString());
            }
        }
    } else {
        spdlog::warn("验证示例未生成有效任务，跳过帧日志导出");
    }

    const QString target_triangles_text = options.adaptive_target_triangles > 0
                                              ? QString::number(static_cast<qulonglong>(options.adaptive_target_triangles))
                                              : QStringLiteral("未细分");
    statusBar()->showMessage(
        QString("验证示例完成：策略=%1；间隔=%2mm；模型=%3；SVG=%4；目标三角形=%5；日志目录=%6")
            .arg(strategy_text)
            .arg(spacing_mm, 0, 'f', 3)
            .arg(QFileInfo(model_path).fileName())
            .arg(QFileInfo(svg_path).fileName())
            .arg(target_triangles_text)
            .arg(log_dir),
        8000);
}

void MainWindow::removeSavedPathFromPatch(int patch_id)
{
    bool removed = false;
    auto it_job = saved_patch_jobs_.find(patch_id);
    if (it_job != saved_patch_jobs_.end()) {
        saved_patch_jobs_.erase(it_job);
        removed = true;
    }
    auto it_uv = saved_patch_uv_paths_.find(patch_id);
    if (it_uv != saved_patch_uv_paths_.end()) {
        saved_patch_uv_paths_.erase(it_uv);
        removed = true;
    }

    if (!removed) {
        statusBar()->showMessage(QString("Patch %1 没有已保存路径").arg(patch_id), 3000);
        return;
    }

    if (controller_ && controller_->getPatternPatchId() == patch_id) {
        controller_->clearPatternForPatch(patch_id);
    }
    updateTextureContourInfoFromJob(patch_id, controller_ ? controller_->getCurrentJob() : nullptr);
    updateUIWithoutMesh();
    statusBar()->showMessage(QString("已移除Patch %1 的保存路径").arg(patch_id), 4000);
}

void MainWindow::removeTextureFromPatch(int patch_id)
{
    if (!model_view_) {
        return;
    }

    if (controller_) {
        controller_->clearPatternForPatch(patch_id);
    }
    
    // 移除3D视图中的纹理（如果当前显示的是这个patch的纹理）
    model_view_->removePatchTexture(patch_id);
    
    // 从纹理列表中移除
    if (texture_list_widget_) {
        texture_list_widget_->removeTexture(patch_id);
    }
    
    // 从UV视图中移除
    if (uv_view_) {
        uv_view_->removeTexture(patch_id);
    }

    patch_raw_uv_ranges_.erase(patch_id);
    patch_display_ranges_.erase(patch_id);
    uv_layout_order_.erase(std::remove(uv_layout_order_.begin(), uv_layout_order_.end(), patch_id), uv_layout_order_.end());
    rebuildUVLayoutDisplayRanges();
    refreshUVTexturesForLayout();
    
    // 更新UI
    updateUIWithoutMesh();
    
    statusBar()->showMessage(QString("已移除面片 %1 的纹理").arg(patch_id), 3000);
}

void MainWindow::editTextureForPatch(int patch_id)
{
    if (!texture_list_widget_) {
        return;
    }
    
    // 获取当前纹理信息
    TextureInfo info = texture_list_widget_->getTexture(patch_id);
    if (info.patch_id < 0) {
        QMessageBox::warning(this, "警告", "该面片没有应用纹理！");
        return;
    }
    
    // 如果纹理编辑器对话框不存在，先创建它
    if (!texture_editor_dialog_) {
        texture_editor_dialog_ = new TextureEditorDialog(this);
        connect(texture_editor_dialog_, &TextureEditorDialog::transformChanged, this, [this]() {
            // 当变换参数改变时，实时更新纹理（如果已有纹理）
            if (model_view_ && model_view_->getSelectedPatchId() >= 0 && 
                controller_->hasParameterization()) {
                // 检查是否有已应用的纹理，如果有则重新应用                // 这里需要保存上次的SVG文件路径，暂时先记录日志
                spdlog::info("纹理变换参数已更新 缩放=({:.2f}, {:.2f}), 平移=({:.2f}, {:.2f}), 旋转={:.1f}°",
                            texture_editor_dialog_->getScaleX(), texture_editor_dialog_->getScaleY(),
                            texture_editor_dialog_->getTranslateX(), texture_editor_dialog_->getTranslateY(),
                            texture_editor_dialog_->getRotation());
                spdlog::info("提示：请重新应用SVG纹理以查看变换效果");
            }
        });
    }
    
    // 设置纹理编辑器的参数
    texture_editor_dialog_->setScaleX(info.scale_x);
    texture_editor_dialog_->setScaleY(info.scale_y);
    texture_editor_dialog_->setTranslateX(info.translate_x);
    texture_editor_dialog_->setTranslateY(info.translate_y);
    texture_editor_dialog_->setRotation(info.rotation_deg);
    texture_editor_dialog_->setInverseStretchPrewarpEnabled(info.enable_inverse_stretch_prewarp);
    texture_editor_dialog_->setInverseStretchPrewarpStrength(info.inverse_stretch_prewarp_strength);
    
    // 连接变换改变信号，实时更新纹理    // 断开之前的连接（如果有），避免重复连接
    disconnect(texture_editor_dialog_, &TextureEditorDialog::transformChanged, this, nullptr);
    connect(texture_editor_dialog_, &TextureEditorDialog::transformChanged, this, [this, patch_id, info]() mutable {
        // 获取最新的变换参数
        info.scale_x = texture_editor_dialog_->getScaleX();
        info.scale_y = texture_editor_dialog_->getScaleY();
        info.translate_x = texture_editor_dialog_->getTranslateX();
        info.translate_y = texture_editor_dialog_->getTranslateY();
        info.rotation_deg = texture_editor_dialog_->getRotation();
        info.enable_inverse_stretch_prewarp = texture_editor_dialog_->getInverseStretchPrewarpEnabled();
        info.inverse_stretch_prewarp_strength = texture_editor_dialog_->getInverseStretchPrewarpStrength();
        
        // 实时更新纹理
        if (model_view_ && controller_->hasParameterization()) {
            model_view_->applyTextureToPatch(patch_id, info.svg_filepath,
                                             controller_->getUVCoords(),
                                             info.texture_width, info.texture_height,
                                             info.opacity, info.ambient, info.diffuse, info.specular,
                                             info.scale_x, info.scale_y, 
                                             info.translate_x, info.translate_y, info.rotation_deg);
            
            // 更新纹理列表
            texture_list_widget_->updateTexture(patch_id, info);
            
            double raw_u_min = 0.0, raw_u_max = 0.0, raw_v_min = 0.0, raw_v_max = 0.0;
            if (computePatchUVBounds(patch_id, raw_u_min, raw_u_max, raw_v_min, raw_v_max)) {
                registerPatchInUVLayout(patch_id, {raw_u_min, raw_u_max, raw_v_min, raw_v_max});
                refreshUVTexturesForLayout();
            }
        }
    });
    
    // 打开纹理编辑器（非模态对话框）
    texture_editor_dialog_->show();
    texture_editor_dialog_->raise();
    texture_editor_dialog_->activateWindow();
}

void MainWindow::showTextureDetails(int patch_id)
{
    if (!texture_list_widget_) {
        return;
    }
    
    // 获取纹理信息
    TextureInfo info = texture_list_widget_->getTexture(patch_id);
    if (info.patch_id < 0) {
        QMessageBox::warning(this, "警告", "该面片没有应用纹理！");
        return;
    }
    
    // 创建或显示详细信息对话框
    if (!texture_details_dialog_) {
        texture_details_dialog_ = new TextureDetailsDialog(this);
    }
    
    texture_details_dialog_->setTextureInfo(patch_id, info);
    texture_details_dialog_->show();
    texture_details_dialog_->raise();
    texture_details_dialog_->activateWindow();
}

void MainWindow::clearPatchData()
{
    if (!model_view_) {
        return;
    }

    // 先清理控制器中的路径/参数化/SVG状态，仅保留当前模型（含细分后模型）
    if (controller_) {
        controller_->clearProcessingStateKeepMesh();
    }

    // 清理3D视图中的面片选择、高亮、贴图、路径高亮
    model_view_->clearHighlightedPathSegments();
    model_view_->setJob(nullptr);
    model_view_->clearPatchData();

    // 清理路径预览、纹理列表、UV视图
    if (path_preview_) {
        path_preview_->updateJob(nullptr);
    }
    if (texture_list_widget_) {
        texture_list_widget_->clearTextures();
    }
    if (uv_view_) {
        uv_view_->clear();
        uv_view_->setJob(nullptr);
    }
    saved_patch_jobs_.clear();
    saved_patch_uv_paths_.clear();
    display_job_cache_.reset();

    resetUVLayoutState();
    current_svg_path_.clear();

    // 保留当前网格，刷新其余UI状态
    updateUIWithoutMesh();

    statusBar()->showMessage("已清除路径/SVG/选中面片，仅保留当前模型", 4000);
    spdlog::info("用户清理面片数据：已清除路径信息、SVG信息和选中面片，保留当前模型网格");
}

void MainWindow::openTextureEditor()
{
    if (!texture_editor_dialog_) {
        texture_editor_dialog_ = new TextureEditorDialog(this);
        connect(texture_editor_dialog_, &TextureEditorDialog::transformChanged, this, [this]() {
            // 当变换参数改变时，实时更新纹理（如果已有纹理）
            if (model_view_ && model_view_->getSelectedPatchId() >= 0 && 
                controller_->hasParameterization()) {
                // 检查是否有已应用的纹理，如果有则重新应用                // 这里需要保存上次的SVG文件路径，暂时先记录日志
                spdlog::info("纹理变换参数已更新 缩放=({:.2f}, {:.2f}), 平移=({:.2f}, {:.2f}), 旋转={:.1f}°",
                            texture_editor_dialog_->getScaleX(), texture_editor_dialog_->getScaleY(),
                            texture_editor_dialog_->getTranslateX(), texture_editor_dialog_->getTranslateY(),
                            texture_editor_dialog_->getRotation());
                spdlog::info("提示：请重新应用SVG纹理以查看变换效果");
            }
        });
    }
    
    texture_editor_dialog_->show();
    texture_editor_dialog_->raise();
    texture_editor_dialog_->activateWindow();
}

void MainWindow::toggleUVView(bool enabled)
{
    if (!left_splitter_ || !uv_view_) {
        return;
    }
    
    if (enabled) {
        // 显示UV视图
        if (left_splitter_->indexOf(uv_view_) < 0) {
            left_splitter_->addWidget(uv_view_);
            left_splitter_->setStretchFactor(0, 2);
            left_splitter_->setStretchFactor(1, 1);
        }
        uv_view_->show();
    } else {
        // 隐藏UV视图
        uv_view_->hide();
        // 注意：不移除widget，只是隐藏，这样数据不会丢失
    }
}

void MainWindow::toggleUVValidation(bool enabled)
{
    if (!uv_view_) {
        return;
    }
    
    uv_view_->setValidationMode(enabled);
    
    // 如果启用校验模式，设置网格信息
    if (enabled && controller_ && controller_->getCurrentMesh()) {
        const auto* mesh = controller_->getCurrentMesh();
        uv_view_->setMeshInfo(const_cast<nbcam::TriangleMesh*>(mesh), mesh->triangles);
    }
    
    // 如果禁用，清除高亮
    if (!enabled) {
        if (model_view_) {
            model_view_->clearVertexHighlight();
        }
    }
}

void MainWindow::selectTopSurfacePatch()
{
    if (!model_view_ || !controller_->getCurrentMesh()) {
        QMessageBox::warning(this, "警告", "请先加载模型！");
        return;
    }
    
    const auto& patches = model_view_->getPatches();
    if (patches.empty()) {
        QMessageBox::warning(this, "警告", "请先进行Patch聚类！操作步骤：1. 加载模型2. 点击 工具 -> Patch聚类 ");
        return;
    }
    
    // 查找法向量最接近(0, 1, 0)的patch（上表面，Y轴向上）
    // 角度容差：0.3弧度（约17度），允许一定的倾斜
    int top_patch_index = model_view_->findPatchByNormal(0.0, 1.0, 0.0, 0.3);
    
    if (top_patch_index >= 0 && top_patch_index < static_cast<int>(patches.size())) {
        model_view_->selectPatch(top_patch_index);
        const auto& patch = patches[top_patch_index];
        
        // 计算角度（用于显示）
        double dot = patch.normal_y;  // 与(0,1,0)的点积就是normal_y
        dot = std::max(-1.0, std::min(1.0, dot));
        double angle_deg = std::acos(dot) * 180.0 / M_PI;
        
        QString msg = QString("已选择上表面面片 %1 (ID: %2): 法向量(%3, %4, %5), 角度: %6°, 面积: %7, 三角形数: %8")
            .arg(top_patch_index)
            .arg(patch.id)
            .arg(patch.normal_x, 0, 'f', 3)
            .arg(patch.normal_y, 0, 'f', 3)
            .arg(patch.normal_z, 0, 'f', 3)
            .arg(angle_deg, 0, 'f', 1)
            .arg(patch.total_area, 0, 'f', 4)
            .arg(patch.triangle_indices.size());
        statusBar()->showMessage(msg, 5000);
    } else {
        // 如果没找到，尝试查找所有可能的候选patch
        QString candidate_info;
        int candidate_count = 0;
        for (size_t i = 0; i < patches.size() && candidate_count < 5; ++i) {
            const auto& patch = patches[i];
            double dot = patch.normal_y;
            dot = std::max(-1.0, std::min(1.0, dot));
            double angle = std::acos(dot);
            if (angle < 0.5) {  // 45度以内
                double angle_deg_candidate = angle * 180.0 / M_PI;
                candidate_info += QString("\n  面片 %1: 法向量(%2, %3, %4), 角度: %5°")
                    .arg(i)
                    .arg(patch.normal_x, 0, 'f', 2)
                    .arg(patch.normal_y, 0, 'f', 2)
                    .arg(patch.normal_z, 0, 'f', 2)
                    .arg(angle_deg_candidate, 0, 'f', 1);
                candidate_count++;
            }
        }
        
        QMessageBox::information(this, "提示", 
            QString("未找到严格的上表面patch（法向量接近(0,1,0)）。\n\n"
                   "可能原因：\n"
                   "1. 模型的上表面不是平面\n"
                   "2. 上表面被分割成多个patch\n"
                   "3. 模型坐标系不同（可能Z轴向上）\n\n"
                   "候选patch（法向量Y分量较大的）：%1\n\n"
                   "建议：\n"
                   "1. 手动在3D视图中点击选择patch\n"
                   "2. 或调整二面角阈值重新聚类")
                .arg(candidate_info.isEmpty() ? "无" : candidate_info));
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    // 设置初始大小比例：纹理列表1/8，中间视图6/8，右侧面板1/8
    if (main_splitter_) {
        int total_width = main_splitter_->width();
        if (total_width > 0) {
            // 计算各部分的大小
            int texture_width = total_width / 8;
            int right_width = total_width / 8;
            int middle_width = total_width - texture_width - right_width;
            
            // 设置主分割器的初始大小
            QList<int> main_sizes;
            main_sizes << texture_width << middle_width << right_width;
            main_splitter_->setSizes(main_sizes);
            
            // 设置左侧垂直分割器（3D视图和UV视图）的比例
            if (left_splitter_) {
                int left_height = left_splitter_->height();
                if (left_height > 0) {
                    // 3D视图2/3，UV视图1/3
                    QList<int> left_sizes;
                    left_sizes << static_cast<int>(left_height * 2.0 / 3.0);
                    left_sizes << static_cast<int>(left_height * 1.0 / 3.0);
                    left_splitter_->setSizes(left_sizes);
                }
            }
            
            // 设置右侧垂直分割器的比例
            QWidget* right_widget = main_splitter_->widget(2);
            if (right_widget) {
                QSplitter* right_splitter = qobject_cast<QSplitter*>(right_widget);
                if (right_splitter) {
                    int right_height = right_splitter->height();
                    if (right_height > 0) {
                        // 参数面板、平面位姿、任务队列平均分配，路径预览按钮区域保持紧凑
                        const int button_height = std::clamp(right_height / 8, 56, 96);
                        const int remain_height = std::max(1, right_height - button_height);
                        const int upper_height = remain_height / 3;
                        const int middle_height = remain_height / 3;
                        const int lower_height = std::max(1, remain_height - upper_height - middle_height);

                        QList<int> right_sizes;
                        right_sizes << upper_height << middle_height << button_height << lower_height;
                        right_splitter->setSizes(right_sizes);
                    }
                }
            }
        }
    }

    static bool auto_run_checked = false;
    if (!auto_run_checked) {
        auto_run_checked = true;
        const QByteArray raw_spacing = qgetenv("NBCAM_AUTORUN_EXAMPLE_SPACING");
        if (!raw_spacing.isEmpty()) {
            bool ok = false;
            const double spacing = QString::fromLocal8Bit(raw_spacing).toDouble(&ok);
            if (ok && spacing > 0.0) {
                spdlog::info("Auto-run validation example enabled, spacing={} mm", spacing);
                QTimer::singleShot(0, this, [this, spacing]() {
                    runValidationExample(spacing);
                });
            } else {
                spdlog::warn("Invalid NBCAM_AUTORUN_EXAMPLE_SPACING value: {}", raw_spacing.constData());
            }
        }
    }
}
