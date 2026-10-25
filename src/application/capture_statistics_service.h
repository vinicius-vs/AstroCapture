#pragma once

#include "domain/capture_entities.h"
#include "domain/repositories/capture_run_repository.h"
#include "domain/repositories/frame_repository.h"

#include <array>
#include <cstdint>
#include <optional>

namespace application {

struct CaptureStatistics
{
    std::uint64_t sequenceCount{};
    std::uint64_t completedFrameCount{};
    std::uint64_t failedFrameCount{};
    std::array<std::uint64_t, 5> completedFramesByType{};
    std::uint64_t exposureMicroseconds{};
    std::optional<domain::Timestamp> lastCapturedAt;
};

struct CaptureStatisticsSnapshot
{
    CaptureStatistics session;
    CaptureStatistics night;
};

class CaptureStatisticsService final
{
public:
    CaptureStatisticsService(
        domain::CaptureRunRepository& captureRunRepository,
        domain::FrameRepository& frameRepository);

    CaptureStatisticsSnapshot forSession(
        const domain::SessionId& sessionId,
        std::chrono::year_month_day acquisitionDate) const;

private:
    domain::CaptureRunRepository& captureRunRepository_;
    domain::FrameRepository& frameRepository_;
};

} // namespace application
