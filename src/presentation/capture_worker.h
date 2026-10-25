#pragma once

#include "domain/camera.h"
#include "domain/capture_entities.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

namespace presentation {

class CaptureWorker final : public QObject
{
    Q_OBJECT

public:
    CaptureWorker(
        QString applicationDataDirectory,
        domain::ICamera& camera,
        domain::CaptureRun captureRun,
        std::shared_ptr<std::atomic_bool> stopRequested);

public slots:
    void execute();

signals:
    void progressChanged(
        quint32 completed,
        quint32 planned,
        quint32 failed,
        const QString& details,
        qint64 elapsedMilliseconds);
    void statisticsChanged(const QString& details);
    void captureCompleted(quint32 completed, bool paused);
    void captureFailed(const QString& message);
    void finished();

private:
    QString applicationDataDirectory_;
    domain::ICamera& camera_;
    domain::CaptureRun captureRun_;
    std::shared_ptr<std::atomic_bool> stopRequested_;
};

} // namespace presentation
