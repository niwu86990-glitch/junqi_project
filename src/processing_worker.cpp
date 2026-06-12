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

void ProcessingWorker::process() {
    // ---- 1. 确保摄像头打开 ----
    if (!camera_.isOpened()) {
        emit statusChanged(QStringLiteral("正在打开摄像头..."));
        auto devPath = cameraDevice_.toLocal8Bit();
        if (!camera_.open(devPath.constData())) {
            emit errorOccurred(QStringLiteral("无法打开摄像头 %1")
                               .arg(cameraDevice_));
            return;
        }
    }

    // ---- 2. 确保 Pipeline 初始化 ----
    if (!pipeline_) {
        emit statusChanged(QStringLiteral("正在加载模板库..."));
        pipeline_.reset(new Pipeline(pipelineConfig_));
        if (!pipeline_->is_ready()) {
            emit errorOccurred(QStringLiteral("模板库加载失败: %1")
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
        emit errorOccurred(QStringLiteral("摄像头采集失败"));
        return;
    }

    // ---- 4. 运行识别流水线 ----
    emit statusChanged(QStringLiteral("正在识别..."));
    auto output = pipeline_->process(frame);

    // ---- 5. 判定胜负 ----
    BattleResult battleResult = BattleResult::INVALID;
    if (output.success) {
        battleResult = judge_.judge(output.left_piece, output.right_piece);
    }

    // ---- 6. 返回结果 ----
    emit statusChanged(QStringLiteral("就绪"));
    emit resultReady(output, battleResult);
}

} // namespace junqi
