#pragma once

#include "pipeline.h"
#include "camera_capture.h"
#include <QObject>
#include <QString>
#include <memory>

namespace junqi {

/// 识别 + 判定的工作线程对象
/// 在单独的 QThread 中运行，避免阻塞 GUI 主线程
class ProcessingWorker : public QObject {
    Q_OBJECT

public:
    explicit ProcessingWorker(QObject* parent = nullptr);

    /// 设置运行时参数（必须在启动线程前调用）
    /// @param templateDir  模板库目录
    /// @param cameraDevice 摄像头设备路径，如 "/dev/video0"
    void setConfig(const QString& templateDir, const QString& cameraDevice = "/dev/video0");

    bool isReady() const;

public slots:
    /// Capture and recognize one piece. side: 0=red, 1=black.
    void processSide(int side);

signals:
    void pieceReady(int side, junqi::PieceResult piece, double elapsedMs);

    /// 处理失败：携带错误描述
    void errorOccurred(int side, const QString& message);

    /// 状态变更：用于更新状态栏文字
    void statusChanged(const QString& status);

private:
    CameraCapture camera_;
    Pipeline::Config pipelineConfig_;
    std::unique_ptr<Pipeline> pipeline_;
    QString cameraDevice_;
};

} // namespace junqi

// 注册自定义类型以支持跨线程信号槽（Qt queued connection）
Q_DECLARE_METATYPE(junqi::PieceResult)
