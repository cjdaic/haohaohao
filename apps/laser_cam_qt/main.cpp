#include "mainwindow.h"
#include "application_controller.h"
#include <QApplication>
#include <QCoreApplication>
#include <QSurfaceFormat>
#include <QString>
#include <QByteArray>
#include <QTextStream>
#include <QStringConverter>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QVTKOpenGLNativeWidget.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#include <io.h>
#include <fcntl.h>
#pragma comment(lib, "Dbghelp.lib")
#endif

#ifdef _WIN32
namespace {

void configureOpenGLForVTK()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 3, 0)
    // 在创建首个QVTK控件前设置OpenGL默认格式。
    QSurfaceFormat fmt = QVTKOpenGLNativeWidget::defaultFormat();
    QSurfaceFormat::setDefaultFormat(fmt);
#endif
}

void configureQtPluginPath(const char* argv0)
{
    if (!argv0 || argv0[0] == '\0') {
        return;
    }
    const QString exe_path = QFileInfo(QString::fromLocal8Bit(argv0)).absoluteFilePath();
    const QString exe_dir = QFileInfo(exe_path).absolutePath();
    const QString platforms_dir = QDir(exe_dir).absoluteFilePath("platforms");
    if (QFileInfo::exists(platforms_dir)) {
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", platforms_dir.toLocal8Bit());
    }
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    const std::string text = msg.toLocal8Bit().constData();
    const char* file = context.file ? context.file : "unknown_file";
    const char* function = context.function ? context.function : "unknown_func";

    switch (type) {
    case QtDebugMsg:
        spdlog::debug("[Qt][{}:{}][{}] {}", file, context.line, function, text);
        break;
    case QtInfoMsg:
        spdlog::info("[Qt][{}:{}][{}] {}", file, context.line, function, text);
        break;
    case QtWarningMsg:
        spdlog::warn("[Qt][{}:{}][{}] {}", file, context.line, function, text);
        break;
    case QtCriticalMsg:
        spdlog::error("[Qt][{}:{}][{}] {}", file, context.line, function, text);
        break;
    case QtFatalMsg:
        spdlog::critical("[QtFatal][{}:{}][{}] {}", file, context.line, function, text);
        spdlog::default_logger()->flush();
        break;
    }
}

std::string moduleForAddress(DWORD64 address)
{
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0 || !mbi.AllocationBase) {
        return "unknown_module";
    }

    char module_path[MAX_PATH] = {0};
    if (GetModuleFileNameA(static_cast<HMODULE>(mbi.AllocationBase), module_path, MAX_PATH) == 0) {
        return "unknown_module";
    }
    return std::string(module_path);
}

LONG WINAPI topLevelExceptionFilter(EXCEPTION_POINTERS* exception_pointers)
{
    if (!exception_pointers || !exception_pointers->ExceptionRecord) {
        spdlog::critical("Unhandled exception with empty exception pointers");
        return EXCEPTION_EXECUTE_HANDLER;
    }

    const DWORD code = exception_pointers->ExceptionRecord->ExceptionCode;
    const auto fault_addr = reinterpret_cast<DWORD64>(exception_pointers->ExceptionRecord->ExceptionAddress);
    spdlog::critical("Unhandled exception: code=0x{:08X}, address=0x{:016X}", code, fault_addr);

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);

    if (!SymInitialize(process, nullptr, TRUE)) {
        spdlog::critical("SymInitialize failed: {}", GetLastError());
        return EXCEPTION_EXECUTE_HANDLER;
    }

    void* stack[64] = {};
    const USHORT frame_count = CaptureStackBackTrace(0, 64, stack, nullptr);
    spdlog::critical("Captured {} stack frames", frame_count);

    constexpr size_t kMaxNameLen = 1024;
    unsigned char symbol_buffer[sizeof(SYMBOL_INFO) + kMaxNameLen] = {};
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = kMaxNameLen - 1;

    for (USHORT i = 0; i < frame_count; ++i) {
        const DWORD64 addr = reinterpret_cast<DWORD64>(stack[i]);
        DWORD64 symbol_disp = 0;

        if (SymFromAddr(process, addr, &symbol_disp, symbol)) {
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD line_disp = 0;
            if (SymGetLineFromAddr64(process, addr, &line_disp, &line)) {
                spdlog::critical("#{} {}+0x{:X} ({}:{})",
                                 i,
                                 symbol->Name,
                                 static_cast<unsigned>(symbol_disp),
                                 line.FileName ? line.FileName : "unknown_file",
                                 line.LineNumber);
            } else {
                spdlog::critical("#{} {}+0x{:X} @0x{:016X}",
                                 i,
                                 symbol->Name,
                                 static_cast<unsigned>(symbol_disp),
                                 addr);
            }
        } else {
            const std::string module = moduleForAddress(addr);
            spdlog::critical("#{} {} @0x{:016X}", i, module, addr);
        }
    }

    SymCleanup(process);
    return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace
#endif

namespace {

QStringList tokenizeCliLine(const QString& line)
{
    QStringList tokens;
    QString current;
    QChar quote_char;

    for (QChar ch : line) {
        if (!quote_char.isNull()) {
            if (ch == quote_char) {
                quote_char = QChar();
            } else {
                current += ch;
            }
            continue;
        }

        if (ch == '"' || ch == '\'') {
            quote_char = ch;
            continue;
        }

        if (ch.isSpace()) {
            if (!current.isEmpty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current += ch;
    }

    if (!current.isEmpty()) {
        tokens.push_back(current);
    }
    return tokens;
}

bool parseCliDouble(const QString& token, double* out_value)
{
    bool ok = false;
    const double value = token.toDouble(&ok);
    if (ok && out_value) {
        *out_value = value;
    }
    return ok;
}

QString resolveCliPath(const QString& raw_path)
{
    if (raw_path.isEmpty()) {
        return QDir::currentPath();
    }

    const QFileInfo file_info(raw_path);
    if (file_info.isAbsolute()) {
        return QDir::cleanPath(file_info.absoluteFilePath());
    }
    return QDir::cleanPath(QDir::current().absoluteFilePath(raw_path));
}

void printCliDirectoryEntry(QTextStream& out, const QFileInfo& entry)
{
    if (entry.isDir()) {
        out << "<DIR>  " << entry.fileName() << "/" << Qt::endl;
        return;
    }

    out << entry.size() << "  " << entry.fileName() << Qt::endl;
}

void printCliDirectoryListing(QTextStream& out, const QString& raw_path)
{
    const QString resolved_path = resolveCliPath(raw_path);
    const QFileInfo target_info(resolved_path);

    if (!target_info.exists()) {
        out << "Path not found: " << resolved_path << Qt::endl;
        return;
    }

    if (target_info.isFile()) {
        printCliDirectoryEntry(out, target_info);
        return;
    }

    QDir dir(resolved_path);
    const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                                    QDir::DirsFirst | QDir::IgnoreCase | QDir::Name);
    out << resolved_path << Qt::endl;
    for (const QFileInfo& entry : entries) {
        printCliDirectoryEntry(out, entry);
    }
}

void printCliHelp(QTextStream& out)
{
    out << "NBCAM CLI commands:" << Qt::endl;
    out << "  help" << Qt::endl;
    out << "  status" << Qt::endl;
    out << "  pwd" << Qt::endl;
    out << "  cd <path>" << Qt::endl;
    out << "  ls [path]" << Qt::endl;
    out << "  load_model <path>" << Qt::endl;
    out << "  parameterize [LSCM|ARAP|AUTHALIC]" << Qt::endl;
    out << "  import_svg <path> [tile_u] [tile_v]" << Qt::endl;
    out << "  generate_pattern <strategy> <spacing_mm> [angle_deg]" << Qt::endl;
    out << "  map_to_xyz" << Qt::endl;
    out << "  assign_params [curvature]" << Qt::endl;
    out << "  plan_path" << Qt::endl;
    out << "  save_job <path>" << Qt::endl;
    out << "  load_job <path>" << Qt::endl;
    out << "  estimate_time" << Qt::endl;
    out << "  exit" << Qt::endl;
    out << Qt::endl;
    out << "Strategy examples: line_hatch, arc_hatch, contour, ring, ring_outin," << Qt::endl;
    out << "or GUI names such as 轴向往返填充 / 轴向往返填充(圆弧)." << Qt::endl;
    out << Qt::endl;
    out << "Typical workflow:" << Qt::endl;
    out << "  load_model \"docs/model1111.STL\"" << Qt::endl;
    out << "  parameterize LSCM" << Qt::endl;
    out << "  import_svg \"docs/HUST.svg\" 0.05 0.05" << Qt::endl;
    out << "  map_to_xyz" << Qt::endl;
    out << "  assign_params curvature" << Qt::endl;
    out << "  plan_path" << Qt::endl;
    out << "  save_job \"out/job.json\"" << Qt::endl;
    out << Qt::endl;
}

void initializeLogging()
{
    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("nbcam.log", true);
        if (file_sink) {
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        }

#ifdef _DEBUG
        auto logger = std::make_shared<spdlog::logger>("nbcam", file_sink);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
#else
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        if (console_sink) {
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        }

        std::vector<spdlog::sink_ptr> sinks;
        if (file_sink) sinks.push_back(file_sink);
        if (console_sink) sinks.push_back(console_sink);
        if (sinks.empty()) {
            throw spdlog::spdlog_ex("no available sinks");
        }

        auto logger = std::make_shared<spdlog::logger>("nbcam", sinks.begin(), sinks.end());
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
#endif
    } catch (const std::exception&) {
        try {
            auto fallback_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("nbcam_fallback.log", true);
            auto fallback_logger = std::make_shared<spdlog::logger>("nbcam", fallback_sink);
            spdlog::set_default_logger(fallback_logger);
            spdlog::set_level(spdlog::level::info);
            spdlog::flush_on(spdlog::level::info);
        } catch (const std::exception&) {
        }
    }
}

int runCliMode()
{
    QTextStream in(stdin);
    QTextStream out(stdout);
    in.setEncoding(QStringConverter::Utf8);
    out.setEncoding(QStringConverter::Utf8);

    ApplicationController controller;

    out << "NBCAM CLI mode" << Qt::endl;
    out << "Type 'help' to view available commands." << Qt::endl << Qt::endl;

    while (true) {
        out << "nbcam> " << Qt::flush;
        const QString line = in.readLine();
        if (line.isNull()) {
            out << Qt::endl;
            break;
        }

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        QStringList args = tokenizeCliLine(trimmed);
        if (args.isEmpty()) {
            continue;
        }

        const QString command = args.takeFirst().toLower();

        if (command == "exit" || command == "quit") {
            break;
        }

        if (command == "help" || command == "?") {
            printCliHelp(out);
            continue;
        }

        if (command == "pwd") {
            out << QDir::currentPath() << Qt::endl;
            continue;
        }

        if (command == "cd") {
            if (args.size() != 1) {
                out << "Usage: cd <path>" << Qt::endl;
                continue;
            }

            const QString path = resolveCliPath(args.front());
            const QFileInfo info(path);
            if (!info.exists() || !info.isDir()) {
                out << "Directory not found: " << path << Qt::endl;
                continue;
            }

            if (!QDir::setCurrent(path)) {
                out << "Failed to change directory: " << path << Qt::endl;
                continue;
            }

            out << QDir::currentPath() << Qt::endl;
            continue;
        }

        if (command == "ls" || command == "dir") {
            if (args.size() > 1) {
                out << "Usage: ls [path]" << Qt::endl;
                continue;
            }

            printCliDirectoryListing(out, args.isEmpty() ? QString() : args.front());
            continue;
        }

        if (command == "status") {
            nbcam::LaserJob* job = controller.getCurrentJob();
            out << "model_loaded=" << (controller.getCurrentMesh() ? "yes" : "no")
                << ", parameterized=" << (controller.hasParameterization() ? "yes" : "no")
                << ", pattern=" << (controller.hasPattern() ? "yes" : "no")
                << ", job_ready=" << ((job && job->isValid()) ? "yes" : "no");
            if (job && job->isValid()) {
                out << ", segments=" << static_cast<qulonglong>(job->segments.size())
                    << ", points=" << static_cast<qulonglong>(job->getTotalPointCount());
            }
            out << Qt::endl;
            continue;
        }

        if (command == "load_model") {
            if (args.size() != 1) {
                out << "Usage: load_model <path>" << Qt::endl;
                continue;
            }
            const QString path = resolveCliPath(args.front());
            out << (controller.loadModel(path.toStdString()) ? "OK " : "FAIL ") << path << Qt::endl;
            continue;
        }

        if (command == "parameterize") {
            const QString algorithm = args.isEmpty() ? "LSCM" : args.front().trimmed().toUpper();
            out << (controller.parameterizeMesh(algorithm.toStdString()) ? "OK " : "FAIL ")
                << "parameterize " << algorithm << Qt::endl;
            continue;
        }

        if (command == "import_svg") {
            if (args.isEmpty() || args.size() > 3) {
                out << "Usage: import_svg <path> [tile_u] [tile_v]" << Qt::endl;
                continue;
            }
            const QString path = resolveCliPath(args.at(0));
            double tile_u = 0.05;
            double tile_v = 0.05;
            if (args.size() >= 2 && !parseCliDouble(args.at(1), &tile_u)) {
                out << "Invalid tile_u: " << args.at(1) << Qt::endl;
                continue;
            }
            if (args.size() >= 3 && !parseCliDouble(args.at(2), &tile_v)) {
                out << "Invalid tile_v: " << args.at(2) << Qt::endl;
                continue;
            }
            out << (controller.importSVGWithTiling(path.toStdString(), tile_u, tile_v, {}) ? "OK " : "FAIL ")
                << "import_svg " << path << Qt::endl;
            continue;
        }

        if (command == "generate_pattern") {
            if (args.size() < 2 || args.size() > 3) {
                out << "Usage: generate_pattern <strategy> <spacing_mm> [angle_deg]" << Qt::endl;
                continue;
            }
            double spacing = 0.0;
            double angle = 0.0;
            if (!parseCliDouble(args.at(1), &spacing)) {
                out << "Invalid spacing: " << args.at(1) << Qt::endl;
                continue;
            }
            if (args.size() >= 3 && !parseCliDouble(args.at(2), &angle)) {
                out << "Invalid angle: " << args.at(2) << Qt::endl;
                continue;
            }
            out << (controller.generatePattern(args.at(0).toStdString(), spacing, angle) ? "OK " : "FAIL ")
                << "generate_pattern " << args.at(0) << Qt::endl;
            continue;
        }

        if (command == "map_to_xyz") {
            out << (controller.mapToXYZ() ? "OK" : "FAIL") << " map_to_xyz" << Qt::endl;
            continue;
        }

        if (command == "assign_params") {
            const QString model_type = args.isEmpty() ? "curvature" : args.front().trimmed();
            out << (controller.assignProcessParams(model_type.toStdString()) ? "OK " : "FAIL ")
                << "assign_params " << model_type << Qt::endl;
            continue;
        }

        if (command == "plan_path") {
            out << (controller.planPath() ? "OK" : "FAIL") << " plan_path" << Qt::endl;
            continue;
        }

        if (command == "save_job") {
            if (args.size() != 1) {
                out << "Usage: save_job <path>" << Qt::endl;
                continue;
            }
            const QString path = resolveCliPath(args.front());
            out << (controller.saveJob(path.toStdString()) ? "OK " : "FAIL ") << path << Qt::endl;
            continue;
        }

        if (command == "load_job") {
            if (args.size() != 1) {
                out << "Usage: load_job <path>" << Qt::endl;
                continue;
            }
            const QString path = resolveCliPath(args.front());
            out << (controller.loadJob(path.toStdString()) ? "OK " : "FAIL ") << path << Qt::endl;
            continue;
        }

        if (command == "estimate_time") {
            nbcam::LaserJob* job = controller.getCurrentJob();
            if (!job || !job->isValid()) {
                out << "No valid job loaded." << Qt::endl;
                continue;
            }
            out << "Estimated time: " << job->estimateTotalTime() << " s" << Qt::endl;
            continue;
        }

        out << "Unknown command: " << command << Qt::endl;
    }

    return 0;
}

}  // namespace

int main(int argc, char *argv[])
{
    bool cli_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]) == "--cli") {
            cli_mode = true;
            break;
        }
    }

#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    configureQtPluginPath((argc > 0) ? argv[0] : nullptr);

#ifdef _WIN32
    SetUnhandledExceptionFilter(topLevelExceptionFilter);
#endif

    if (cli_mode) {
        QCoreApplication app(argc, argv);
        initializeLogging();
        qInstallMessageHandler(qtMessageHandler);
        app.setApplicationName("NBCAM");
        app.setApplicationVersion("0.0.1");
        app.setOrganizationName("NBCAM");
        spdlog::info("NBCAM CLI mode started");
        return runCliMode();
    }

    configureOpenGLForVTK();
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/app_icons/chiikawa.ico"));
    initializeLogging();
    qInstallMessageHandler(qtMessageHandler);
    app.setApplicationName("NBCAM");
    app.setApplicationVersion("0.0.1");
    app.setOrganizationName("NBCAM");
    spdlog::info("QApplication ready, instance={}", static_cast<const void*>(QCoreApplication::instance()));
    if (!QCoreApplication::instance()) {
        spdlog::critical("QCoreApplication::instance() is null before creating MainWindow");
        return -1;
    }

    MainWindow* window = nullptr;
    QTimer::singleShot(0, [&window]() {
        spdlog::info("Creating MainWindow, instance={}", static_cast<const void*>(QCoreApplication::instance()));
        window = new MainWindow();
        window->showMaximized();
    });

    const int exit_code = app.exec();
    delete window;
    return exit_code;
}
