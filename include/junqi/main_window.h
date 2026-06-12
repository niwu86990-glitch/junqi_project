#pragma once

#include "result.h"
#include "battle_judge.h"

#include <QMainWindow>
#include <QThread>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace junqi {
class ProcessingWorker;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString& templateDir,
                        const QString& cameraDevice,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onCaptureClicked();
    void onResultReady(junqi::RecognitionOutput output,
                       junqi::BattleResult battleResult);
    void onError(const QString& message);
    void onStatusChanged(const QString& status);

private:
    void setupUi();
    void setupWorker(const QString& templateDir, const QString& cameraDevice);
    static QString colorText(junqi::PieceColor c);
    static QString battleText(junqi::BattleResult r);

    // UI 控件
    QLabel* titleLabel_       = nullptr;
    QPushButton* captureBtn_  = nullptr;
    QLabel* leftPieceLabel_   = nullptr;
    QLabel* rightPieceLabel_  = nullptr;
    QLabel* battleResultLabel_ = nullptr;
    QLabel* elapsedLabel_     = nullptr;
    QLabel* statusLabel_      = nullptr;

    // 工作线程
    QThread* workerThread_ = nullptr;
    junqi::ProcessingWorker* worker_ = nullptr;
};
