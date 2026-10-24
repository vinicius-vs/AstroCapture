#pragma once

#include "domain/logging/logging_service.h"

#include <QMutex>
#include <QString>

#include <cstdint>
#include <cstddef>

namespace infrastructure {

class JsonLinesLoggingService final : public domain::LoggingService
{
public:
    explicit JsonLinesLoggingService(QString applicationDataDirectory);

    void log(const domain::LogEvent& event) override;

    QString activeLogPath() const;

    static constexpr std::uint64_t maximumFileSizeBytes = 10 * 1024 * 1024;
    static constexpr std::size_t retainedFileCount = 5;

private:
    QByteArray serialize(const domain::LogEvent& event) const;
    void rotateIfNeeded(qsizetype recordSize);
    void rotate();

    QString logDirectory_;
    QString activeLogPath_;
    mutable QMutex mutex_;
};

} // namespace infrastructure
