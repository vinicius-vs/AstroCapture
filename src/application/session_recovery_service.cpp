#include "application/session_recovery_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace application {
namespace {

using namespace domain;

QString normalizedAbsolutePath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString framePath(const Frame& frame)
{
    if (!frame.filePath()) {
        return {};
    }
    return QString::fromStdString(frame.filePath()->string());
}

bool isInterruptedFrameState(FrameState state)
{
    return state == FrameState::Exposing
        || state == FrameState::Transferring
        || state == FrameState::Validating;
}

QString decisionName(RecoveryDecision decision)
{
    switch (decision) {
    case RecoveryDecision::Continue:
        return QStringLiteral("Continue");
    case RecoveryDecision::NewSequence:
        return QStringLiteral("NewSequence");
    case RecoveryDecision::Cancel:
        return QStringLiteral("Cancel");
    }
    throw std::invalid_argument("Invalid session recovery decision");
}

} // namespace

bool SessionRecoveryReport::requiresDecision() const noexcept
{
    return !incompleteRuns.empty();
}

bool SessionRecoveryReport::canContinue(const CaptureRunId& captureRunId) const noexcept
{
    for (const RecoverableCaptureRun& run : incompleteRuns) {
        if (run.id == captureRunId) {
            return run.canContinue;
        }
    }
    return false;
}

SessionRecoveryService::SessionRecoveryService(
    const CaptureRunRepository& captureRunRepository,
    const FrameRepository& frameRepository,
    const infrastructure::SessionFileInspector& fileInspector,
    LoggingService& loggingService)
    : captureRunRepository_(captureRunRepository)
    , frameRepository_(frameRepository)
    , fileInspector_(fileInspector)
    , loggingService_(loggingService)
{
}

SessionRecoveryReport SessionRecoveryService::inspect(
    const SessionRecord& session,
    const QString& sessionDirectory) const
{
    if (sessionDirectory.isEmpty() || !QDir::isAbsolutePath(sessionDirectory)) {
        throw std::invalid_argument("Session recovery directory must be an absolute path");
    }

    SessionRecoveryReport report{session.session.id(), {}, {}, {}};
    std::vector<Frame> allFrames;

    for (const CaptureRun& captureRun :
         captureRunRepository_.findBySessionId(session.session.id())) {
        std::vector<Frame> runFrames = frameRepository_.findByCaptureRunId(captureRun.id());
        RecoverableCaptureRun recoverableRun{
            captureRun.id(), captureRun.state(), {}, {}, false};
        bool hasNonCompletedFrame = false;

        for (const Frame& frame : runFrames) {
            allFrames.push_back(frame);
            if (frame.state() != FrameState::Completed) {
                hasNonCompletedFrame = true;
            }
            if (frame.state() == FrameState::Pending) {
                recoverableRun.pendingFrameIds.push_back(frame.id());
                report.findings.push_back({
                    RecoveryFindingType::PendingFrame,
                    captureRun.id(),
                    frame.id(),
                    {},
                    QStringLiteral("Frame is pending and has not been captured.")});
            } else if (isInterruptedFrameState(frame.state())) {
                recoverableRun.interruptedFrameIds.push_back(frame.id());
                report.findings.push_back({
                    RecoveryFindingType::InterruptedFrame,
                    captureRun.id(),
                    frame.id(),
                    {},
                    QStringLiteral("Frame was interrupted before completion; it will not be retried automatically.")});
            } else if (frame.state() == FrameState::Failed) {
                report.findings.push_back({
                    RecoveryFindingType::FailedFrame,
                    captureRun.id(),
                    frame.id(),
                    {},
                    frame.errorMessage()
                        ? QString::fromStdString(*frame.errorMessage())
                        : QStringLiteral("Frame is marked as failed.")});
            }
        }

        if (captureRun.state() != CaptureRunState::Completed || hasNonCompletedFrame) {
            if (captureRun.state() != CaptureRunState::Ready || hasNonCompletedFrame) {
                recoverableRun.canContinue =
                    captureRun.state() != CaptureRunState::Completed
                    && captureRun.state() != CaptureRunState::Failed
                    && !recoverableRun.pendingFrameIds.empty();
                report.incompleteRuns.push_back(std::move(recoverableRun));
            }
        }
    }

    std::vector<infrastructure::InspectedSessionFile> diskFiles =
        fileInspector_.inspect(sessionDirectory);
    std::vector<QString> registeredCompletedFiles;
    for (const Frame& frame : allFrames) {
        if (frame.state() != FrameState::Completed || !frame.filePath()) {
            continue;
        }

        const QString path = framePath(frame);
        const QString normalizedPath = normalizedAbsolutePath(path);
        registeredCompletedFiles.push_back(normalizedPath);
        const QFileInfo fileInfo(path);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            report.findings.push_back({
                RecoveryFindingType::MissingCompletedFile,
                frame.captureRunId(),
                frame.id(),
                path,
                QStringLiteral("The completed frame record refers to a missing file.")});
            continue;
        }
        if (!frame.fileSize() || fileInfo.size() < 0
            || static_cast<std::uint64_t>(fileInfo.size()) != *frame.fileSize()) {
            report.findings.push_back({
                RecoveryFindingType::CompletedFileSizeMismatch,
                frame.captureRunId(),
                frame.id(),
                path,
                QStringLiteral("Expected %1 bytes but found %2 bytes.")
                    .arg(*frame.fileSize())
                    .arg(fileInfo.size())});
            continue;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)
            || (fileInfo.size() > 0 && file.read(1).isEmpty())) {
            report.findings.push_back({
                RecoveryFindingType::UnreadableCompletedFile,
                frame.captureRunId(),
                frame.id(),
                path,
                QStringLiteral("The completed frame file cannot be read.")});
        }
    }

    for (const infrastructure::InspectedSessionFile& diskFile : diskFiles) {
        if (diskFile.temporary) {
            report.findings.push_back({
                RecoveryFindingType::TemporaryFile,
                std::nullopt,
                std::nullopt,
                diskFile.absolutePath,
                QStringLiteral("An abandoned temporary image file was found.")});
        } else if (std::find(
                       registeredCompletedFiles.cbegin(),
                       registeredCompletedFiles.cend(),
                       normalizedAbsolutePath(diskFile.absolutePath))
                   == registeredCompletedFiles.cend()) {
            report.findings.push_back({
                RecoveryFindingType::OrphanFile,
                std::nullopt,
                std::nullopt,
                diskFile.absolutePath,
                QStringLiteral("No completed frame record references this file.")});
        }
    }

    if (report.requiresDecision()) {
        const bool anyRunCanContinue = std::any_of(
            report.incompleteRuns.cbegin(),
            report.incompleteRuns.cend(),
            [](const RecoverableCaptureRun& run) { return run.canContinue; });
        if (anyRunCanContinue) {
            report.availableDecisions.push_back(RecoveryDecision::Continue);
        }
        report.availableDecisions.push_back(RecoveryDecision::NewSequence);
        report.availableDecisions.push_back(RecoveryDecision::Cancel);
    }

    return report;
}

void SessionRecoveryService::recordDecision(
    const SessionRecoveryReport& report,
    RecoveryDecision decision,
    std::optional<CaptureRunId> captureRunId) const
{
    if (!report.requiresDecision()) {
        throw std::logic_error("No session recovery decision is required");
    }
    if (std::find(
            report.availableDecisions.cbegin(),
            report.availableDecisions.cend(),
            decision)
        == report.availableDecisions.cend()) {
        throw std::invalid_argument("Recovery decision is not available for this session");
    }
    if (decision == RecoveryDecision::Continue) {
        if (!captureRunId || !report.canContinue(*captureRunId)) {
            throw std::invalid_argument(
                "Continue requires an incomplete capture run with pending frames");
        }
    } else if (captureRunId) {
        throw std::invalid_argument("Only a Continue decision may specify a capture run");
    }

    LogEvent event;
    event.severity = LogSeverity::Info;
    event.event = QStringLiteral("session.recovery.decision");
    event.message = QStringLiteral("User selected a session recovery action");
    event.sessionId = report.sessionId;
    event.captureRunId = std::move(captureRunId);
    event.operation = QStringLiteral("session.recovery");
    event.state = decisionName(decision);
    loggingService_.log(event);
}

} // namespace application
