#include "application/capture_sequence_service.h"

#include <QUuid>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace application {

CaptureSequenceService::CaptureSequenceService(
    CaptureFrameService& captureFrameService,
    domain::CaptureRunRepository& captureRunRepository,
    domain::FrameRepository& frameRepository,
    domain::CaptureRun& captureRun)
    : captureFrameService_(captureFrameService)
    , captureRunRepository_(captureRunRepository)
    , frameRepository_(frameRepository)
    , captureRun_(captureRun)
{
}

std::vector<domain::Frame> CaptureSequenceService::execute(
    CaptureSequenceProgressCallback progress)
{
    const domain::CaptureSettings& settings = captureRun_.settings();
    if (settings.mode != domain::CaptureMode::Finite
        || !settings.plannedFrameCount
        || *settings.plannedFrameCount == 0) {
        throw std::invalid_argument(
            "Capture sequences require a finite mode and a positive planned frame count");
    }
    if (captureRun_.state() != domain::CaptureRunState::Ready
        && captureRun_.state() != domain::CaptureRunState::Running) {
        throw std::logic_error("Capture run must be Ready or Running to execute a sequence");
    }

    const std::uint32_t plannedFrameCount = *settings.plannedFrameCount;
    std::vector<domain::Frame> existingFrames =
        frameRepository_.findByCaptureRunId(captureRun_.id());
    if (existingFrames.size() > plannedFrameCount) {
        throw std::logic_error("Capture run contains more frames than its planned count");
    }

    std::vector<domain::Frame> completedFrames;
    std::vector<domain::FrameId> pendingFrameIds;
    for (const domain::Frame& frame : existingFrames) {
        if (frame.captureRunId() != captureRun_.id()
            || frame.type() != settings.frameType
            || frame.exposure() != settings.exposure
            || frame.iso() != settings.iso) {
            throw std::logic_error("Capture run contains a frame with inconsistent settings");
        }

        switch (frame.state()) {
        case domain::FrameState::Completed:
            completedFrames.push_back(frame);
            break;
        case domain::FrameState::Pending:
            pendingFrameIds.push_back(frame.id());
            break;
        case domain::FrameState::Exposing:
        case domain::FrameState::Transferring:
        case domain::FrameState::Validating:
        case domain::FrameState::Failed:
            throw std::logic_error(
                "Capture run contains a frame that requires recovery before continuing");
        }
    }

    std::uint32_t completedCount = static_cast<std::uint32_t>(completedFrames.size());
    const auto reportProgress =
        [&progress, plannedFrameCount](std::uint32_t count,
                                       std::optional<CaptureFrameProgress> currentFrame) {
            if (progress) {
                progress(CaptureSequenceProgress{
                    count,
                    plannedFrameCount,
                    std::move(currentFrame)
                });
            }
        };

    if (captureRun_.state() == domain::CaptureRunState::Ready) {
        captureRunRepository_.updateState(
            captureRun_.id(),
            domain::CaptureRunState::Running);
        captureRun_.start();
    }

    try {
        reportProgress(completedCount, std::nullopt);

        const auto executeFrame = [&](const domain::FrameId& frameId) {
            try {
                const domain::Frame frame = captureFrameService_.execute(
                    frameId,
                    [&](const CaptureFrameProgress& frameProgress) {
                        if (frameProgress.state == domain::FrameState::Completed) {
                            ++completedCount;
                        }
                        reportProgress(completedCount, frameProgress);
                    });
                completedFrames.push_back(frame);
                retryableFrameId_.reset();
            } catch (const CaptureFrameError& error) {
                retryableFrameId_.reset();
                const auto& recoveryOptions = error.recoveryOptions();
                if (std::find(
                        recoveryOptions.begin(),
                        recoveryOptions.end(),
                        CaptureRecoveryDecision::Retry)
                        != recoveryOptions.end()
                    && !frameRepository_.findById(frameId)) {
                    retryableFrameId_ = frameId;
                }
                throw;
            }
        };

        for (const domain::FrameId& frameId : pendingFrameIds) {
            executeFrame(frameId);
        }

        while (completedCount < plannedFrameCount) {
            domain::FrameId frameId;
            if (retryableFrameId_
                && !frameRepository_.findById(*retryableFrameId_)) {
                frameId = *retryableFrameId_;
            } else {
                retryableFrameId_.reset();
                do {
                    frameId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
                } while (frameRepository_.findById(frameId));
            }
            executeFrame(frameId);
        }

        captureRunRepository_.updateState(
            captureRun_.id(),
            domain::CaptureRunState::Completed);
        captureRun_.complete();
    } catch (...) {
        if (captureRun_.state() == domain::CaptureRunState::Running
            || captureRun_.state() == domain::CaptureRunState::StopRequested) {
            captureRunRepository_.updateState(
                captureRun_.id(),
                domain::CaptureRunState::Paused);
            captureRun_.pause();
        }
        throw;
    }

    return completedFrames;
}

} // namespace application
