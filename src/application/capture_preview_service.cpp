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
    const auto preview = latestCompletedFrame(sessionId);
    if (!preview || !preview->frame.filePath()) {
        return std::nullopt;
    }
    return preview->frame.filePath();
}

std::optional<LatestCapturePreview> CapturePreviewService::latestCompletedFrame(
    const domain::SessionId& sessionId) const
{
    std::optional<domain::Timestamp> latestCaptureTime;
    std::optional<LatestCapturePreview> latestPreview;
    for (const domain::CaptureRun& captureRun : captureRunRepository_.findBySessionId(sessionId)) {
        for (const domain::Frame& frame : frameRepository_.findByCaptureRunId(captureRun.id())) {
            if (frame.state() != domain::FrameState::Completed
                || !frame.capturedAt()
                || !frame.filePath()) {
                continue;
            }
            if (!latestCaptureTime || *latestCaptureTime < *frame.capturedAt()) {
                latestCaptureTime = frame.capturedAt();
                latestPreview = LatestCapturePreview{frame};
            }
        }
    }
    return latestPreview;
}

} // namespace application
