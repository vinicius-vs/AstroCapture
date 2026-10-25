#pragma once

#include "domain/camera.h"

#include <chrono>
#include <cstdint>
#include <optional>

namespace infrastructure {

struct SimulatedCameraFailurePlan
{
    std::optional<std::uint64_t> failApplySettingsOnCall;
    std::optional<std::uint64_t> failExposureOnCall;
    std::optional<std::uint64_t> failTransferOnCall;
    std::optional<std::uint64_t> disconnectOnTransferCall;
    std::optional<std::uint64_t> invalidFileOnCaptureCall;
    std::uint64_t failTransferAfterBytes{};
};

struct SimulatedCameraOptions
{
    std::chrono::milliseconds exposureDelay{};
    std::chrono::milliseconds transferChunkDelay{};
    std::uint64_t transferChunkSize{16};
    SimulatedCameraFailurePlan failures;
    bool liveViewSupported{};
};

class SimulatedCamera final : public domain::ICamera
{
public:
    explicit SimulatedCamera(SimulatedCameraOptions options = {});

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
    domain::CameraCapabilities supportedCapabilities() const;

    SimulatedCameraOptions options_;
    bool connected_{};
    std::uint64_t nextCaptureId_{};
    std::uint64_t applySettingsCallCount_{};
    std::uint64_t captureCallCount_{};
    std::uint64_t transferCallCount_{};
    bool liveViewActive_{};
    domain::CameraSettings settings_{
        std::chrono::seconds{5},
        800
    };
};

} // namespace infrastructure
