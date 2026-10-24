#pragma once

#include "domain/capture_entities.h"

#include <vector>

namespace domain {

class FrameRepository
{
public:
    virtual ~FrameRepository() = default;

    virtual std::vector<Frame> findByCaptureRunId(const CaptureRunId& captureRunId) const = 0;
};

} // namespace domain
