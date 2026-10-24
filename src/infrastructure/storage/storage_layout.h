#pragma once

#include "domain/capture_entities.h"

#include <QString>

namespace infrastructure {

class StorageLayout final
{
public:
    explicit StorageLayout(QString imageRoot);

    QString targetDirectory(const domain::Target& target) const;
    QString sessionDirectory(
        const domain::Target& target,
        const domain::Session& session) const;
    QString frameDirectory(
        const domain::Target& target,
        const domain::Session& session,
        const domain::CaptureRun& captureRun) const;
    QString frameFilePath(
        const domain::Target& target,
        const domain::Session& session,
        const domain::CaptureRun& captureRun,
        const domain::FrameId& frameId,
        const QString& originalFileName) const;

private:
    QString imageRoot_;
};

} // namespace infrastructure
