#pragma once

#include "domain/capture_entities.h"

#include <QString>

#include <optional>

namespace domain {

enum class LogSeverity
{
    Info,
    Warning,
    Error
};

struct LogEvent final
{
    Timestamp timestamp{std::chrono::system_clock::now()};
    LogSeverity severity{LogSeverity::Info};
    QString event;
    QString message;
    std::optional<SessionId> sessionId;
    std::optional<CaptureRunId> captureRunId;
    std::optional<FrameId> frameId;
    std::optional<QString> operation;
    std::optional<QString> previousState;
    std::optional<QString> state;
    std::optional<QString> errorCode;
    std::optional<QString> technicalDetails;
    std::optional<QString> relativeFilePath;
    std::optional<CaptureSettings> captureSettings;
};

} // namespace domain
