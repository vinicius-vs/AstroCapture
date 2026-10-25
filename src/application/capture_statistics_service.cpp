#include "application/capture_statistics_service.h"

#include <limits>
#include <stdexcept>

namespace application {
namespace {

std::size_t frameTypeIndex(domain::FrameType frameType)
{
    switch (frameType) {
    case domain::FrameType::Light:
        return 0;
    case domain::FrameType::Dark:
        return 1;
    case domain::FrameType::Flat:
        return 2;
    case domain::FrameType::Bias:
        return 3;
    case domain::FrameType::DarkFlat:
        return 4;
    }
    throw std::invalid_argument("Invalid frame type");
}

void increment(std::uint64_t& value)
{
    if (value == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("Capture statistics counter overflow");
    }
    ++value;
}

void addExposure(CaptureStatistics& statistics, std::uint64_t exposureMicroseconds)
{
    if (exposureMicroseconds
        > std::numeric_limits<std::uint64_t>::max() - statistics.exposureMicroseconds) {
        throw std::overflow_error("Accumulated exposure time overflow");
    }
    statistics.exposureMicroseconds += exposureMicroseconds;
}

void collectFrame(CaptureStatistics& statistics, const domain::Frame& frame)
{
    if (frame.state() == domain::FrameState::Failed) {
        increment(statistics.failedFrameCount);
        return;
    }
    if (frame.state() != domain::FrameState::Completed) {
        return;
    }
    if (!frame.exposure() || !frame.capturedAt()) {
        throw std::runtime_error("A completed frame is missing exposure metadata");
    }

    increment(statistics.completedFrameCount);
    increment(statistics.completedFramesByType[frameTypeIndex(frame.type())]);
    addExposure(
        statistics,
        static_cast<std::uint64_t>(frame.exposure()->count()));
    if (!statistics.lastCapturedAt
        || *statistics.lastCapturedAt < *frame.capturedAt()) {
        statistics.lastCapturedAt = frame.capturedAt();
    }
}

} // namespace

CaptureStatisticsService::CaptureStatisticsService(
    domain::CaptureRunRepository& captureRunRepository,
    domain::FrameRepository& frameRepository)
    : captureRunRepository_(captureRunRepository)
    , frameRepository_(frameRepository)
{
}

CaptureStatisticsSnapshot CaptureStatisticsService::forSession(
    const domain::SessionId& sessionId,
    std::chrono::year_month_day acquisitionDate) const
{
    CaptureStatisticsSnapshot snapshot;
    const auto runs = captureRunRepository_.findBySessionId(sessionId);
    for (const domain::CaptureRun& run : runs) {
        increment(snapshot.session.sequenceCount);
        const bool sameNight = run.acquisitionDate() == acquisitionDate;
        if (sameNight) {
            increment(snapshot.night.sequenceCount);
        }

        for (const domain::Frame& frame : frameRepository_.findByCaptureRunId(run.id())) {
            collectFrame(snapshot.session, frame);
            if (sameNight) {
                collectFrame(snapshot.night, frame);
            }
        }
    }
    return snapshot;
}

} // namespace application
