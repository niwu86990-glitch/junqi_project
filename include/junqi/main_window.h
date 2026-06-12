#pragma once

#include "result.h"
#include "battle_judge.h"

#include <QMainWindow>
#include <QThread>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

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
    void onRedCaptureClicked();
    void onBlackCaptureClicked();
    void onRedResultClicked();
    void onBlackResultClicked();
    void onPieceReady(int side, junqi::PieceResult piece, double elapsedMs);
    void onError(int side, const QString& message);
    void onStatusChanged(const QString& status);
    void resetRound();

private:
    void setupUi();
    void setupWorker(const QString& templateDir, const QString& cameraDevice);
    void startCapture(int side);
    bool confirmPiece(int side, junqi::PieceResult& piece);
    void showHiddenResult(int side);
    void finishSide(int side, const junqi::PieceResult& piece, double elapsedMs);
    void judgeIfReady();
    void setBattleResultAppearance(junqi::BattleResult result);
    static void styleDialog(QWidget* dialog);
    static QString battleColor(junqi::BattleResult result);
    static QString pieceName(int id);
    static QString colorText(junqi::PieceColor c);
    static QString battleText(junqi::BattleResult r);

    // UI 控件
    QLabel* titleLabel_       = nullptr;
    QPushButton* redCaptureBtn_ = nullptr;
    QPushButton* blackCaptureBtn_ = nullptr;
    QPushButton* resetBtn_ = nullptr;
    QPushButton* leftPieceButton_ = nullptr;
    QPushButton* rightPieceButton_ = nullptr;
    QLabel* battleResultLabel_ = nullptr;
    QLabel* elapsedLabel_     = nullptr;
    QLabel* statusLabel_      = nullptr;

    junqi::PieceResult redPiece_;
    junqi::PieceResult blackPiece_;
    bool redReady_ = false;
    bool blackReady_ = false;
    int activeSide_ = -1;
    junqi::BattleJudge judge_;

    // 工作线程
    QThread* workerThread_ = nullptr;
    junqi::ProcessingWorker* worker_ = nullptr;
};
