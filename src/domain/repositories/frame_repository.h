#pragma once

#include "domain/capture_entities.h"

#include <optional>
#include <vector>

namespace domain {

class FrameRepository
{
public:
    virtual ~FrameRepository() = default;

    virtual void create(const Frame& frame) = 0;
    virtual std::optional<Frame> findById(const FrameId& id) const = 0;
    virtual std::vector<Frame> findByCaptureRunId(const CaptureRunId& captureRunId) const = 0;
    virtual void update(const Frame& frame) = 0;
};

} // namespace domain
