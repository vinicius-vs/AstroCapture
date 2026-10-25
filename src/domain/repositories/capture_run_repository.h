#pragma once

#include "domain/capture_entities.h"

#include <optional>
#include <vector>

namespace domain {

class CaptureRunRepository
{
public:
    virtual ~CaptureRunRepository() = default;

    virtual void create(const CaptureRun& captureRun) = 0;
    virtual void createWithFrames(
        const CaptureRun& captureRun,
        const std::vector<Frame>& frames) = 0;
    virtual std::optional<CaptureRun> findById(const CaptureRunId& id) const = 0;
    virtual std::vector<CaptureRun> findBySessionId(const SessionId& sessionId) const = 0;
    virtual void updateState(const CaptureRunId& id, CaptureRunState desiredState) = 0;
};

} // namespace domain
