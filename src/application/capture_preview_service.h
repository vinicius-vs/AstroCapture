#pragma once

#include "domain/capture_entities.h"
#include "domain/repositories/capture_run_repository.h"
#include "domain/repositories/frame_repository.h"

#include <filesystem>
#include <optional>

namespace application {

class CapturePreviewService final
{
public:
    CapturePreviewService(
        domain::CaptureRunRepository& captureRunRepository,
        domain::FrameRepository& frameRepository);

    std::optional<std::filesystem::path> latestCompletedImagePath(
        const domain::SessionId& sessionId) const;

private:
    domain::CaptureRunRepository& captureRunRepository_;
    domain::FrameRepository& frameRepository_;
};

} // namespace application
