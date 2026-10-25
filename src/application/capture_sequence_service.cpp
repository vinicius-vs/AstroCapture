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
    bool expectedToBeExecuting = false;
    if (!executing_.compare_exchange_strong(expectedToBeExecuting, true)) {
        throw std::logic_error("Capture sequence is already executing");
    }
    struct ExecutionGuard final
    {
        std::atomic_bool& executing;
        ~ExecutionGuard()
        {
            executing.store(false);
        }
    } executionGuard{executing_};

    const domain::CaptureSettings& settings = captureRun_.settings();
    const bool finite = settings.mode == domain::CaptureMode::Finite;
    if (finite && (!settings.plannedFrameCount || *settings.plannedFrameCount == 0)) {
        throw std::invalid_argument("Finite capture sequences require a positive planned count");
    }
    if (captureRun_.state() != domain::CaptureRunState::Ready
        && captureRun_.state() != domain::CaptureRunState::Running
        && captureRun_.state() != domain::CaptureRunState::Paused) {
        throw std::logic_error(
            "Capture run must be Ready, Running, or safely Paused to execute a sequence");
    }
    if (captureRun_.state() == domain::CaptureRunState::Paused
        && retryableFrameId_) {
        throw std::logic_error(
            "A failed pre-exposure attempt requires an explicit recovery decision");
    }

    const std::optional<std::uint32_t> plannedFrameCount = settings.plannedFrameCount;
    std::vector<domain::Frame> existingFrames =
        frameRepository_.findByCaptureRunId(captureRun_.id());
    if (finite && existingFrames.size() > *plannedFrameCount) {
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
    } else if (captureRun_.state() == domain::CaptureRunState::Paused) {
        captureRunRepository_.updateState(
            captureRun_.id(),
            domain::CaptureRunState::Running);
        captureRun_.resume();
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
                    },
                    [this] {
                        return stopRequested_.load();
                    });
                if (frame.state() == domain::FrameState::Completed) {
                    completedFrames.push_back(frame);
                }
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
            if (stopRequested_.load()) {
                break;
            }
            executeFrame(frameId);
            if (stopRequested_.load()) {
                break;
            }
        }

        while (!stopRequested_.load()
               && (!finite || completedCount < *plannedFrameCount)) {
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

        if (stopRequested_.load()) {
            if (captureRun_.state() == domain::CaptureRunState::Running) {
                captureRunRepository_.updateState(
                    captureRun_.id(),
                    domain::CaptureRunState::StopRequested);
                captureRun_.requestStop();
            }
            if (captureRun_.state() == domain::CaptureRunState::StopRequested) {
                captureRunRepository_.updateState(
                    captureRun_.id(),
                    domain::CaptureRunState::Paused);
                captureRun_.pause();
            }
            stopRequested_.store(false);
        } else if (finite) {
            captureRunRepository_.updateState(
                captureRun_.id(),
                domain::CaptureRunState::Completed);
            captureRun_.complete();
        } else {
            throw std::logic_error("Continuous capture ended without a stop request");
        }
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

void CaptureSequenceService::requestStop() noexcept
{
    stopRequested_.store(true);
}

} // namespace application
