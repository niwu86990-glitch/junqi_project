#include "junqi/main_window.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFontDatabase>
#include <QDir>
#include <QFileInfo>

int main(int argc, char* argv[]) {
    fprintf(stderr, "[MAIN] 0: program start\n");

    // 注册自定义类型以支持跨线程信号槽
    qRegisterMetaType<junqi::PieceResult>("junqi::PieceResult");

    QApplication app(argc, argv);
    fprintf(stderr, "[MAIN] 1: QApplication created\n");

    app.setApplicationName(QStringLiteral("junqi_gui"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

   // ---- 显式加载中文字体 (Qt 5.6 兼容方式) ----
    // APP_ROOT = 可执行文件所在目录（如 /opt/junqi/）
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList fontPaths = {
        appDir + QStringLiteral("/fonts/AlibabaPuHuiTi-3-55-Regular.ttf"),
        appDir + QStringLiteral("/fonts/SourceHanSansSC-Regular.otf"),
        QStringLiteral("./fonts/AlibabaPuHuiTi-3-55-Regular.ttf"),
        QStringLiteral("fonts/AlibabaPuHuiTi-3-55-Regular.ttf"),
    };
    int fontId = -1;
    for (const auto& path : fontPaths) {
        if (QFileInfo::exists(path)) {
            fontId = QFontDatabase::addApplicationFont(path);
            qDebug() << "[FONT] Loaded:" << path << "id:" << fontId;
            break;
        }
    }
    if (fontId >= 0) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        qDebug() << "[FONT] Families:" << families;
        if (!families.isEmpty()) {
            QFont defaultFont(families.first(), 14);
            app.setFont(defaultFont);
            qDebug() << "[FONT] Set default font to:" << families.first();
        }
    } else {
        qWarning() << "[FONT] Chinese font not found, using default font";
        qWarning() << "[FONT] Searched paths:" << fontPaths;
        // 不致命：Qt 内置的 DejaVu Sans 可渲染英文界面，中文会显示为方块但不会崩溃
    }
    
    fprintf(stderr, "[MAIN] 2: font loading done\n");

    // ---- 命令行参数 ----
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("军棋对战识别系统 — Qt GUI"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption templateOpt(
        QStringLiteral("template-dir"),
        QStringLiteral("模板库目录路径"),
        QStringLiteral("dir"),
        QStringLiteral("templates/"));
    parser.addOption(templateOpt);

    QCommandLineOption cameraOpt(
        QStringLiteral("camera"),
        QStringLiteral("摄像头设备路径 (如 /dev/video0)"),
        QStringLiteral("device"),
        QStringLiteral("/dev/video0"));
    parser.addOption(cameraOpt);

    parser.process(app);
    fprintf(stderr, "[MAIN] 3: command line parsed\n");

    QString templateDir = parser.value(templateOpt);
    QString cameraDevice = parser.value(cameraOpt);

    qDebug() << "Template dir:" << templateDir;
    qDebug() << "Camera device:" << cameraDevice;
    fprintf(stderr, "[MAIN] 4: templateDir=%s, cameraDevice=%s\n",
            templateDir.toStdString().c_str(),
            cameraDevice.toStdString().c_str());

    // ---- 主窗口 ----
    fprintf(stderr, "[MAIN] 5: before MainWindow constructor\n");
    qDebug() << "[DEBUG] creating MainWindow...";
    MainWindow window(templateDir, cameraDevice);
    fprintf(stderr, "[MAIN] 6: after MainWindow constructor\n");

    fprintf(stderr, "[MAIN] 7: before show()\n");
    qDebug() << "[DEBUG] MainWindow created, calling show()...";
    // X210 LCD physical size. Use a borderless normal window instead of
    // showFullScreen(), which has triggered linuxfb mode-switch crashes.
    window.setGeometry(0, 0, 1024, 600);
    window.show();
    fprintf(stderr, "[MAIN] 8: after show()\n");

    qDebug() << "[DEBUG] show() done, entering event loop...";
    fprintf(stderr, "[MAIN] 9: before app.exec()\n");
    return app.exec();
}
