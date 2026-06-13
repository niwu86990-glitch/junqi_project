#include "junqi/main_window.h"
#include "junqi/processing_worker.h"

#include <QDebug>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QWidget>
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
//  UI 布局
// ============================================================

void MainWindow::setupUi() {
    fprintf(stderr, "[UI] setupUi: start\n");

    setWindowTitle(QStringLiteral("junqi"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    resize(1024, 600);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("mainPanel"));
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(38, 22, 38, 18);
    root->setSpacing(10);

    central->setStyleSheet(QStringLiteral(
        "QWidget#mainPanel {"
        "  background-color: #edf2f7;"
        "}"
        "QLabel#titleLabel {"
        "  color: #17202a;"
        "  font-size: 30px;"
        "  font-weight: bold;"
        "  padding: 6px;"
        "}"
        "QLabel#instructionLabel {"
        "  color: #34495e;"
        "  font-size: 17px;"
        "  padding: 4px;"
        "}"
        "QPushButton {"
        "  min-height: 58px;"
        "  color: #17202a;"
        "  background-color: #ffffff;"
        "  border: 3px solid #52616b;"
        "  font-size: 21px;"
        "  font-weight: bold;"
        "  padding: 4px 12px;"
        "}"
        "QPushButton#redCaptureButton {"
        "  color: #a81818;"
        "  background-color: #fff5f5;"
        "  border-color: #c62828;"
        "}"
        "QPushButton#blackCaptureButton {"
        "  color: #111111;"
        "  background-color: #f7f7f7;"
        "  border-color: #222222;"
        "}"
        "QPushButton#resetButton {"
        "  color: #174a7e;"
        "  background-color: #f4f9ff;"
        "  border-color: #2e6da4;"
        "}"
        "QPushButton:disabled {"
        "  color: #7f8c8d;"
        "  background-color: #dfe6e9;"
        "  border-color: #aab7b8;"
        "}"
        "QPushButton#redStatus {"
        "  min-height: 52px;"
        "  color: #a81818;"
        "  background-color: #fff7f7;"
        "  border: 2px solid #c94a4a;"
        "  font-size: 18px;"
        "  font-weight: bold;"
        "  padding: 12px;"
        "}"
        "QPushButton#blackStatus {"
        "  min-height: 52px;"
        "  color: #111111;"
        "  background-color: #f8f8f8;"
        "  border: 2px solid #333333;"
        "  font-size: 18px;"
        "  font-weight: bold;"
        "  padding: 12px;"
        "}"
        "QPushButton#redStatus:disabled, QPushButton#blackStatus:disabled {"
        "  color: #7f8c8d;"
        "  background-color: #e8ecef;"
        "  border-color: #aab7b8;"
        "}"
        "QLabel#elapsedLabel, QLabel#statusLabel {"
        "  color: #52616b;"
        "  font-size: 14px;"
        "}"
    ));

    // ---- 标题 ----
    titleLabel_ = new QLabel(QStringLiteral("军棋对战识别系统"), this);
    titleLabel_->setObjectName(QStringLiteral("titleLabel"));
    titleLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(titleLabel_);

    auto* instructionLabel = new QLabel(
        QStringLiteral("请将一枚棋子放在摄像头下，再点击对应一方的识别按钮"), this);
    instructionLabel->setObjectName(QStringLiteral("instructionLabel"));
    instructionLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(instructionLabel);

    auto* captureLayout = new QHBoxLayout();
    captureLayout->setSpacing(22);
    redCaptureBtn_ = new QPushButton(QStringLiteral("红 方 识 别"), this);
    redCaptureBtn_->setObjectName(QStringLiteral("redCaptureButton"));
    blackCaptureBtn_ = new QPushButton(QStringLiteral("黑 方 识 别"), this);
    blackCaptureBtn_->setObjectName(QStringLiteral("blackCaptureButton"));
    captureLayout->addWidget(redCaptureBtn_);
    captureLayout->addWidget(blackCaptureBtn_);
    root->addLayout(captureLayout);

    // ---- 双方识别状态 ----
    auto* statusLayout = new QHBoxLayout();
    statusLayout->setSpacing(22);
    leftPieceButton_ = new QPushButton(QStringLiteral("红方: 等待识别"), this);
    leftPieceButton_->setObjectName(QStringLiteral("redStatus"));
    leftPieceButton_->setEnabled(false);
    rightPieceButton_ = new QPushButton(QStringLiteral("黑方: 等待识别"), this);
    rightPieceButton_->setObjectName(QStringLiteral("blackStatus"));
    rightPieceButton_->setEnabled(false);
    statusLayout->addWidget(leftPieceButton_);
    statusLayout->addWidget(rightPieceButton_);
    root->addLayout(statusLayout);

    root->addStretch(1);

    // ---- 耗时 ----
    elapsedLabel_ = new QLabel(QStringLiteral("耗时: -- ms"), this);
    elapsedLabel_->setObjectName(QStringLiteral("elapsedLabel"));
    elapsedLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(elapsedLabel_);

    // ---- 判定结果：固定在下一轮按钮上方 ----
    battleResultLabel_ = new QLabel(QStringLiteral("等待双方完成识别"), this);
    battleResultLabel_->setAlignment(Qt::AlignCenter);
    battleResultLabel_->setMinimumHeight(86);
    setBattleResultAppearance(junqi::BattleResult::INVALID);
    root->addWidget(battleResultLabel_);

    resetBtn_ = new QPushButton(QStringLiteral("开 始 下 一 轮"), this);
    resetBtn_->setObjectName(QStringLiteral("resetButton"));
    resetBtn_->setEnabled(false);
    root->addWidget(resetBtn_);

    // ---- 状态栏 ----
    statusLabel_ = new QLabel(QStringLiteral("状态: 就绪"), this);
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));
    statusLabel_->setAlignment(Qt::AlignCenter);
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

    connect(redCaptureBtn_, &QPushButton::clicked,
            this, &MainWindow::onRedCaptureClicked);
    connect(blackCaptureBtn_, &QPushButton::clicked,
            this, &MainWindow::onBlackCaptureClicked);
    connect(leftPieceButton_, &QPushButton::clicked,
            this, &MainWindow::onRedResultClicked);
    connect(rightPieceButton_, &QPushButton::clicked,
            this, &MainWindow::onBlackResultClicked);
    connect(resetBtn_, &QPushButton::clicked,
            this, &MainWindow::resetRound);

    connect(worker_, &junqi::ProcessingWorker::pieceReady,
            this, &MainWindow::onPieceReady);
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

void MainWindow::onRedCaptureClicked() {
    startCapture(0);
}

void MainWindow::onBlackCaptureClicked() {
    startCapture(1);
}

void MainWindow::onRedResultClicked() {
    showHiddenResult(0);
}

void MainWindow::onBlackResultClicked() {
    showHiddenResult(1);
}

void MainWindow::startCapture(int side) {
    if (activeSide_ >= 0 || (side == 0 ? redReady_ : blackReady_)) return;

    activeSide_ = side;
    redCaptureBtn_->setEnabled(false);
    blackCaptureBtn_->setEnabled(false);
    leftPieceButton_->setEnabled(false);
    rightPieceButton_->setEnabled(false);
    if (side == 0) {
        redCaptureBtn_->setText(QStringLiteral("红方处理中..."));
        leftPieceButton_->setText(QStringLiteral("红方: 正在识别"));
    } else {
        blackCaptureBtn_->setText(QStringLiteral("黑方处理中..."));
        rightPieceButton_->setText(QStringLiteral("黑方: 正在识别"));
    }
    QMetaObject::invokeMethod(worker_, "processSide", Qt::QueuedConnection,
                              Q_ARG(int, side));
}

void MainWindow::onPieceReady(int side, junqi::PieceResult piece,
                              double elapsedMs) {
    activeSide_ = -1;
    redCaptureBtn_->setText(QStringLiteral("红 方 识 别"));
    blackCaptureBtn_->setText(QStringLiteral("黑 方 识 别"));

    if (confirmPiece(side, piece)) {
        finishSide(side, piece, elapsedMs);
    } else {
        if (side == 0) leftPieceButton_->setText(QStringLiteral("红方: 等待重新识别"));
        else rightPieceButton_->setText(QStringLiteral("黑方: 等待重新识别"));
    }

    redCaptureBtn_->setEnabled(!redReady_);
    blackCaptureBtn_->setEnabled(!blackReady_);
    leftPieceButton_->setEnabled(redReady_);
    rightPieceButton_->setEnabled(blackReady_);
}

bool MainWindow::confirmPiece(int side, junqi::PieceResult& piece) {
    const QString sideText = side == 0 ? QStringLiteral("红方")
                                       : QStringLiteral("黑方");

    QString details = QStringLiteral(
        "%1识别结果\n\n棋子：%2\n置信度：%3%")
        .arg(sideText)
        .arg(QString::fromStdString(piece.character))
        .arg(static_cast<int>(piece.confidence * 100));
    details += QStringLiteral("\n\n确认后结果会从界面隐藏。");

    QMessageBox box(QMessageBox::Information,
                    QStringLiteral("请%1确认").arg(sideText),
                    details, QMessageBox::NoButton, this);
    styleDialog(&box);
    auto* confirmButton = box.addButton(QStringLiteral("确定"),
                                        QMessageBox::AcceptRole);
    auto* correctButton = box.addButton(QStringLiteral("识别失误"),
                                        QMessageBox::ActionRole);
    box.exec();

    if (box.clickedButton() == confirmButton) return true;
    if (box.clickedButton() != correctButton) return false;

    QStringList names;
    for (int id = 1; id <= 12; ++id) names << pieceName(id);
    int current = piece.character_id >= 1 && piece.character_id <= 12
                      ? piece.character_id - 1 : 0;
    QInputDialog correctionDialog(this);
    correctionDialog.setWindowTitle(QStringLiteral("更正识别结果"));
    correctionDialog.setLabelText(QStringLiteral("请选择正确的棋子："));
    correctionDialog.setComboBoxItems(names);
    correctionDialog.setComboBoxEditable(false);
    correctionDialog.setTextValue(names.at(current));
    styleDialog(&correctionDialog);
    if (correctionDialog.exec() != QDialog::Accepted) return false;

    const QString selected = correctionDialog.textValue();
    if (selected.isEmpty()) return false;

    const int id = names.indexOf(selected) + 1;
    piece.character_id = id;
    piece.character = selected.toStdString();
    piece.confidence = 1.0f;
    return true;
}

void MainWindow::showHiddenResult(int side) {
    const bool ready = side == 0 ? redReady_ : blackReady_;
    if (!ready || activeSide_ >= 0) return;

    const junqi::PieceResult& piece = side == 0 ? redPiece_ : blackPiece_;
    const QString sideText = side == 0 ? QStringLiteral("红方")
                                       : QStringLiteral("黑方");
    const QString details = QStringLiteral(
        "%1识别结果\n\n棋子：%2\n置信度：%3%\n\n"
        "点击确认后，结果将再次隐藏。")
        .arg(sideText)
        .arg(QString::fromStdString(piece.character))
        .arg(static_cast<int>(piece.confidence * 100));

    QMessageBox box(QMessageBox::Information,
                    QStringLiteral("%1已隐藏结果").arg(sideText),
                    details, QMessageBox::NoButton, this);
    styleDialog(&box);
    box.addButton(QStringLiteral("确认"), QMessageBox::AcceptRole);
    box.exec();
}

void MainWindow::finishSide(int side, const junqi::PieceResult& piece,
                            double elapsedMs) {
    if (side == 0) {
        redPiece_ = piece;
        redReady_ = true;
        leftPieceButton_->setText(QStringLiteral("红方: 已确认（结果已隐藏，点击查看）"));
        leftPieceButton_->setEnabled(true);
    } else {
        blackPiece_ = piece;
        blackReady_ = true;
        rightPieceButton_->setText(QStringLiteral("黑方: 已确认（结果已隐藏，点击查看）"));
        rightPieceButton_->setEnabled(true);
    }
    elapsedLabel_->setText(QStringLiteral("本次识别耗时: %1 ms")
                           .arg(static_cast<int>(elapsedMs)));
    judgeIfReady();
}

void MainWindow::judgeIfReady() {
    if (!redReady_ || !blackReady_) {
        battleResultLabel_->setText(QStringLiteral("等待双方完成识别"));
        setBattleResultAppearance(junqi::BattleResult::INVALID);
        return;
    }

    const auto result = judge_.judge(redPiece_, blackPiece_);
    const QString resultText = battleText(result);
    battleResultLabel_->setText(resultText);
    setBattleResultAppearance(result);
    redCaptureBtn_->setEnabled(false);
    blackCaptureBtn_->setEnabled(false);
    resetBtn_->setEnabled(true);
    statusLabel_->setText(QStringLiteral("状态: 本轮判定完成"));
}

void MainWindow::resetRound() {
    redPiece_ = junqi::PieceResult();
    blackPiece_ = junqi::PieceResult();
    redReady_ = false;
    blackReady_ = false;
    activeSide_ = -1;
    redCaptureBtn_->setText(QStringLiteral("红 方 识 别"));
    blackCaptureBtn_->setText(QStringLiteral("黑 方 识 别"));
    redCaptureBtn_->setEnabled(true);
    blackCaptureBtn_->setEnabled(true);
    resetBtn_->setEnabled(false);
    leftPieceButton_->setText(QStringLiteral("红方: 等待识别"));
    rightPieceButton_->setText(QStringLiteral("黑方: 等待识别"));
    leftPieceButton_->setEnabled(false);
    rightPieceButton_->setEnabled(false);
    battleResultLabel_->setText(QStringLiteral("等待双方完成识别"));
    setBattleResultAppearance(junqi::BattleResult::INVALID);
    elapsedLabel_->setText(QStringLiteral("耗时: -- ms"));
    statusLabel_->setText(QStringLiteral("状态: 就绪"));
}

void MainWindow::onError(int side, const QString& message) {
    activeSide_ = -1;
    redCaptureBtn_->setText(QStringLiteral("红 方 识 别"));
    blackCaptureBtn_->setText(QStringLiteral("黑 方 识 别"));
    redCaptureBtn_->setEnabled(!redReady_);
    blackCaptureBtn_->setEnabled(!blackReady_);
    leftPieceButton_->setEnabled(redReady_);
    rightPieceButton_->setEnabled(blackReady_);
    if (side == 0) leftPieceButton_->setText(QStringLiteral("红方: 识别失败，请重试"));
    else rightPieceButton_->setText(QStringLiteral("黑方: 识别失败，请重试"));
    statusLabel_->setText(QStringLiteral("状态: 错误 - %1").arg(message));
    QMessageBox errorBox(QMessageBox::Warning,
                         QStringLiteral("识别失败"),
                         message, QMessageBox::Ok, this);
    styleDialog(&errorBox);
    errorBox.exec();
}

void MainWindow::onStatusChanged(const QString& status) {
    statusLabel_->setText(QStringLiteral("状态: %1").arg(status));
}

// ============================================================
//  工具函数
// ============================================================

void MainWindow::setBattleResultAppearance(junqi::BattleResult result) {
    battleResultLabel_->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: %1;"
        "  background-color: #ffffff;"
        "  border: 4px solid %1;"
        "  font-size: 36px;"
        "  font-weight: bold;"
        "  padding: 8px;"
        "}").arg(battleColor(result)));
}

QString MainWindow::battleColor(junqi::BattleResult result) {
    switch (result) {
        case junqi::BattleResult::LEFT_WINS:
            return QStringLiteral("#d02020");
        case junqi::BattleResult::RIGHT_WINS:
            return QStringLiteral("#101010");
        case junqi::BattleResult::DRAW:
            return QStringLiteral("#138a36");
        default:
            return QStringLiteral("#6c7a89");
    }
}

void MainWindow::styleDialog(QWidget* dialog) {
    dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog->setMinimumWidth(520);
    dialog->setStyleSheet(QStringLiteral(
        "QDialog, QMessageBox {"
        "  background-color: #f4f4f4;"
        "  border: 4px solid #202020;"
        "}"
        "QLabel {"
        "  color: #101010;"
        "  background-color: transparent;"
        "  border: none;"
        "  padding: 8px;"
        "}"
        "QPushButton {"
        "  min-width: 150px;"
        "  min-height: 54px;"
        "  color: #101010;"
        "  background-color: #ffffff;"
        "  border: 2px solid #303030;"
        "  padding: 6px;"
        "}"
        "QComboBox {"
        "  min-height: 52px;"
        "  color: #101010;"
        "  background-color: #ffffff;"
        "  border: 2px solid #303030;"
        "  padding: 4px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  color: #101010;"
        "  background-color: #ffffff;"
        "  border: 2px solid #303030;"
        "  selection-background-color: #b8d8ff;"
        "}"
    ));
}

QString MainWindow::pieceName(int id) {
    static const char* names[] = {
        "", "司令", "军长", "师长", "旅长", "团长", "营长",
        "连长", "排长", "工兵", "地雷", "炸弹", "军旗"
    };
    if (id < 1 || id > 12) return QStringLiteral("未知");
    return QString::fromUtf8(names[id]);
}

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
