#include "junqi/processing_worker.h"
#include <QDebug>

namespace junqi {

ProcessingWorker::ProcessingWorker(QObject* parent)
    : QObject(parent) {}

void ProcessingWorker::setConfig(const QString& templateDir, const QString& cameraDevice) {
    pipelineConfig_.template_dir = templateDir.toStdString();
    pipelineConfig_.verbose = false;  // GUI 模式不需要终端输出
    cameraDevice_ = cameraDevice;
}

bool ProcessingWorker::isReady() const {
    return pipeline_ && pipeline_->is_ready();
}

void ProcessingWorker::processSide(int side) {
    // ---- 1. 确保摄像头打开 ----
    if (!camera_.isOpened()) {
        emit statusChanged(QStringLiteral("正在打开摄像头..."));
        auto devPath = cameraDevice_.toLocal8Bit();
        if (!camera_.open(devPath.constData())) {
            emit errorOccurred(side, QStringLiteral("无法打开摄像头 %1")
                               .arg(cameraDevice_));
            return;
        }
    }

    // ---- 2. 确保 Pipeline 初始化 ----
    if (!pipeline_) {
        emit statusChanged(QStringLiteral("正在加载模板库..."));
        pipeline_.reset(new Pipeline(pipelineConfig_));
        if (!pipeline_->is_ready()) {
            emit errorOccurred(side, QStringLiteral("模板库加载失败: %1")
                               .arg(QString::fromStdString(
                                   pipelineConfig_.template_dir)));
            pipeline_.reset();
            return;
        }
    }

    // ---- 3. 采集一帧 ----
    emit statusChanged(QStringLiteral("正在拍照..."));
    cv::Mat frame;
    if (!camera_.capture(frame) || frame.empty()) {
        emit errorOccurred(side, QStringLiteral("摄像头采集失败"));
        return;
    }

    // ---- 4. 运行单棋子识别流水线 ----
    emit statusChanged(QStringLiteral("正在识别..."));
    std::string error;
    double elapsedMs = 0.0;
    auto piece = pipeline_->process_single(frame, &error, &elapsedMs);
    if (piece.character_id < 0) {
        emit errorOccurred(side, QStringLiteral("棋子识别失败，请重新摆放后再试"));
        return;
    }

    // ---- 5. 返回结果；胜负由双方确认后在主线程判定 ----
    emit statusChanged(QStringLiteral("就绪"));
    emit pieceReady(side, piece, elapsedMs);
}

} // namespace junqi
