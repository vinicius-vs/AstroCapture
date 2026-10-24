#pragma once

#include "domain/logging/logging_service.h"
#include "domain/repositories/capture_run_repository.h"
#include "domain/repositories/frame_repository.h"
#include "domain/repositories/session_repository.h"
#include "infrastructure/recovery/session_file_inspector.h"

#include <QString>

#include <optional>
#include <vector>

namespace application {

enum class RecoveryFindingType
{
    PendingFrame,
    InterruptedFrame,
    FailedFrame,
    MissingCompletedFile,
    CompletedFileSizeMismatch,
    UnreadableCompletedFile,
    OrphanFile,
    TemporaryFile
};

struct RecoveryFinding
{
    RecoveryFindingType type;
    std::optional<domain::CaptureRunId> captureRunId;
    std::optional<domain::FrameId> frameId;
    QString filePath;
    QString details;
};

struct RecoverableCaptureRun
{
    domain::CaptureRunId id;
    domain::CaptureRunState state;
    std::vector<domain::FrameId> pendingFrameIds;
    std::vector<domain::FrameId> interruptedFrameIds;
    bool canContinue;
};

enum class RecoveryDecision
{
    Continue,
    NewSequence,
    Cancel
};

struct SessionRecoveryReport
{
    domain::SessionId sessionId;
    std::vector<RecoverableCaptureRun> incompleteRuns;
    std::vector<RecoveryFinding> findings;
    std::vector<RecoveryDecision> availableDecisions;

    bool requiresDecision() const noexcept;
    bool canContinue(const domain::CaptureRunId& captureRunId) const noexcept;
};

class SessionRecoveryService final
{
public:
    SessionRecoveryService(
        const domain::CaptureRunRepository& captureRunRepository,
        const domain::FrameRepository& frameRepository,
        const infrastructure::SessionFileInspector& fileInspector,
        domain::LoggingService& loggingService);

    SessionRecoveryReport inspect(
        const domain::SessionRecord& session,
        const QString& sessionDirectory) const;
    void recordDecision(
        const SessionRecoveryReport& report,
        RecoveryDecision decision,
        std::optional<domain::CaptureRunId> captureRunId = std::nullopt) const;

private:
    const domain::CaptureRunRepository& captureRunRepository_;
    const domain::FrameRepository& frameRepository_;
    const infrastructure::SessionFileInspector& fileInspector_;
    domain::LoggingService& loggingService_;
};

} // namespace application
