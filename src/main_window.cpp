#include "junqi/main_window.h"
#include "junqi/processing_worker.h"

#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

MainWindow::MainWindow(const QString& templateDir,
                       const QString& cameraDevice,
                       QWidget* parent)
    : QMainWindow(parent)
{
    qDebug() << "[DEBUG] MainWindow ctor: setupUi...";
    setupUi();
    qDebug() << "[DEBUG] MainWindow ctor: setupWorker...";
    setupWorker(templateDir, cameraDevice);
    qDebug() << "[DEBUG] MainWindow ctor: done";
}

MainWindow::~MainWindow() {
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait(3000);
    }
}

// ============================================================
//  UI 布局 — 极简版，无 stylesheet，无自定义字体
//  所有样式问题等窗口能正常显示后再加回来
// ============================================================

void MainWindow::setupUi() {
    fprintf(stderr, "[UI] setupUi: start\n");

    setWindowTitle(QStringLiteral("junqi"));
    resize(1024, 600);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    // 不设 margins/spacing，最小化布局计算

    // ---- 标题 ----
    titleLabel_ = new QLabel(QStringLiteral("军棋对战识别系统"), this);
    titleLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(titleLabel_);

    root->addSpacing(20);

    // ---- 截图按钮（无 stylesheet）----
    captureBtn_ = new QPushButton(QStringLiteral("截 图 判 定"), this);
    root->addWidget(captureBtn_);

    root->addSpacing(10);

    // ---- 识别结果 ----
    auto* resultTitle = new QLabel(QStringLiteral("识别结果"), this);
    root->addWidget(resultTitle);

    leftPieceLabel_ = new QLabel(QStringLiteral("红方: ----"), this);
    root->addWidget(leftPieceLabel_);

    rightPieceLabel_ = new QLabel(QStringLiteral("黑方: ----"), this);
    root->addWidget(rightPieceLabel_);

    root->addSpacing(6);

    // ---- 判定结果 ----
    battleResultLabel_ = new QLabel(QStringLiteral("判定: ----"), this);
    root->addWidget(battleResultLabel_);

    root->addSpacing(6);

    // ---- 耗时 ----
    elapsedLabel_ = new QLabel(QStringLiteral("耗时: -- ms"), this);
    root->addWidget(elapsedLabel_);

    root->addStretch(1);

    // ---- 状态栏 ----
    statusLabel_ = new QLabel(QStringLiteral("状态: 就绪"), this);
    root->addWidget(statusLabel_);

    fprintf(stderr, "[UI] setupUi: end\n");
}

// ============================================================
//  工作线程初始化
// ============================================================

void MainWindow::setupWorker(const QString& templateDir, const QString& cameraDevice) {
    workerThread_ = new QThread(this);
    worker_ = new junqi::ProcessingWorker();
    worker_->setConfig(templateDir, cameraDevice);
    worker_->moveToThread(workerThread_);

    connect(captureBtn_, &QPushButton::clicked,
            this, &MainWindow::onCaptureClicked);

    connect(worker_, &junqi::ProcessingWorker::resultReady,
            this, &MainWindow::onResultReady);
    connect(worker_, &junqi::ProcessingWorker::errorOccurred,
            this, &MainWindow::onError);
    connect(worker_, &junqi::ProcessingWorker::statusChanged,
            this, &MainWindow::onStatusChanged);

    connect(workerThread_, &QThread::finished,
            worker_, &QObject::deleteLater);

    workerThread_->start();
}

// ============================================================
//  槽函数
// ============================================================

void MainWindow::onCaptureClicked() {
    captureBtn_->setEnabled(false);
    captureBtn_->setText(QStringLiteral("处理中..."));
    QMetaObject::invokeMethod(worker_, "process", Qt::QueuedConnection);
}

void MainWindow::onResultReady(junqi::RecognitionOutput output,
                                junqi::BattleResult battleResult) {
    captureBtn_->setEnabled(true);
    captureBtn_->setText(QStringLiteral("截 图 判 定"));

    if (output.success) {
        const auto& left  = output.left_piece;
        const auto& right = output.right_piece;

        leftPieceLabel_->setText(
            QStringLiteral("红方: %1  (置信度 %2%)")
                .arg(QString::fromStdString(left.character))
                .arg(static_cast<int>(left.confidence * 100)));

        rightPieceLabel_->setText(
            QStringLiteral("黑方: %1  (置信度 %2%)")
                .arg(QString::fromStdString(right.character))
                .arg(static_cast<int>(right.confidence * 100)));
    } else {
        leftPieceLabel_->setText(QStringLiteral("红方: 识别失败"));
        rightPieceLabel_->setText(QStringLiteral("黑方: 识别失败"));
    }

    QString battleStr = battleText(battleResult);
    battleResultLabel_->setText(QStringLiteral("判定: %1").arg(battleStr));

    elapsedLabel_->setText(
        QStringLiteral("耗时: %1 ms").arg(
            static_cast<int>(output.elapsed_ms)));
}

void MainWindow::onError(const QString& message) {
    captureBtn_->setEnabled(true);
    captureBtn_->setText(QStringLiteral("截 图 判 定"));

    leftPieceLabel_->setText(QStringLiteral("红方: --"));
    rightPieceLabel_->setText(QStringLiteral("黑方: --"));
    battleResultLabel_->setText(QStringLiteral("判定: 出错"));
    elapsedLabel_->setText(QStringLiteral("耗时: --"));

    statusLabel_->setText(QStringLiteral("状态: 错误 — %1").arg(message));
}

void MainWindow::onStatusChanged(const QString& status) {
    statusLabel_->setText(QStringLiteral("状态: %1").arg(status));
}

// ============================================================
//  工具函数
// ============================================================

QString MainWindow::battleText(junqi::BattleResult r) {
    switch (r) {
        case junqi::BattleResult::LEFT_WINS:
            return QStringLiteral("红方获胜");
        case junqi::BattleResult::RIGHT_WINS:
            return QStringLiteral("黑方获胜");
        case junqi::BattleResult::DRAW:
            return QStringLiteral("平局（同归于尽）");
        case junqi::BattleResult::INVALID:
            return QStringLiteral("判定无效");
        default:
            return QStringLiteral("未知");
    }
}
