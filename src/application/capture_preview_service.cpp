#include "application/capture_preview_service.h"

namespace application {

CapturePreviewService::CapturePreviewService(
    domain::CaptureRunRepository& captureRunRepository,
    domain::FrameRepository& frameRepository)
    : captureRunRepository_(captureRunRepository)
    , frameRepository_(frameRepository)
{
}

std::optional<std::filesystem::path> CapturePreviewService::latestCompletedImagePath(
    const domain::SessionId& sessionId) const
{
    std::optional<domain::Timestamp> latestCaptureTime;
    std::optional<std::filesystem::path> latestImagePath;
    for (const domain::CaptureRun& captureRun : captureRunRepository_.findBySessionId(sessionId)) {
        for (const domain::Frame& frame : frameRepository_.findByCaptureRunId(captureRun.id())) {
            if (frame.state() != domain::FrameState::Completed
                || !frame.capturedAt()
                || !frame.filePath()) {
                continue;
            }
            if (!latestCaptureTime || *latestCaptureTime < *frame.capturedAt()) {
                latestCaptureTime = frame.capturedAt();
                latestImagePath = frame.filePath();
            }
        }
    }
    return latestImagePath;
}

} // namespace application
