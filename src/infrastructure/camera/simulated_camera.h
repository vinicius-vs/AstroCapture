#pragma once

#include "domain/camera.h"

#include <cstdint>

namespace infrastructure {

class SimulatedCamera final : public domain::ICamera
{
public:
    domain::CameraOperationResult connect() override;
    domain::CameraOperationResult disconnect() override;

    domain::CameraResult<domain::CameraCapabilities> capabilities() const override;
    domain::CameraResult<domain::CameraSettings> settings() const override;
    domain::CameraOperationResult applySettings(
        const domain::CameraSettings& settings) override;

    domain::CameraResult<domain::CameraCapture> capture(
        const domain::CaptureRequest& request) override;
    domain::CameraOperationResult transfer(
        const domain::CameraCapture& capture,
        std::ostream& destination,
        domain::TransferProgressCallback progress = {}) override;

    domain::CameraOperationResult startLiveView() override;
    domain::CameraOperationResult stopLiveView() override;

private:
    static domain::CameraCapabilities supportedCapabilities();

    bool connected_{};
    std::uint64_t nextCaptureId_{};
    domain::CameraSettings settings_{
        std::chrono::seconds{5},
        800
    };
};

} // namespace infrastructure
