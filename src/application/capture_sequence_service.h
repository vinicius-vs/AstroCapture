#pragma once

#include "application/capture_frame_service.h"
#include "domain/capture_entities.h"
#include "domain/repositories/capture_run_repository.h"
#include "domain/repositories/frame_repository.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace application {

struct CaptureSequenceProgress
{
    std::uint32_t completedFrames;
    std::uint32_t plannedFrameCount;
    std::optional<CaptureFrameProgress> currentFrame;
};

using CaptureSequenceProgressCallback =
    std::function<void(const CaptureSequenceProgress&)>;

class CaptureSequenceService final
{
public:
    CaptureSequenceService(
        CaptureFrameService& captureFrameService,
        domain::CaptureRunRepository& captureRunRepository,
        domain::FrameRepository& frameRepository,
        domain::CaptureRun& captureRun);

    std::vector<domain::Frame> execute(
        CaptureSequenceProgressCallback progress = {});

private:
    CaptureFrameService& captureFrameService_;
    domain::CaptureRunRepository& captureRunRepository_;
    domain::FrameRepository& frameRepository_;
    domain::CaptureRun& captureRun_;
    std::optional<domain::FrameId> retryableFrameId_;
};

} // namespace application
