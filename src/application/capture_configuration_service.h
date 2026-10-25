#pragma once

#include "domain/camera.h"
#include "domain/capture_entities.h"
#include "domain/repositories/capture_run_repository.h"
#include "domain/repositories/session_repository.h"

#include <cstdint>

namespace application {

class CaptureConfigurationService final
{
public:
    CaptureConfigurationService(
        domain::SessionRepository& sessionRepository,
        domain::CaptureRunRepository& captureRunRepository,
        domain::ICamera& camera);

    domain::CameraCapabilities cameraCapabilities();
    domain::CameraSettings currentCameraSettings();
    domain::CaptureRun configureSequence(
        const domain::SessionId& sessionId,
        domain::CaptureSettings settings);
    std::optional<domain::CaptureRun> findCaptureRun(
        const domain::CaptureRunId& captureRunId) const;

private:
    void connectCamera();

    domain::SessionRepository& sessionRepository_;
    domain::CaptureRunRepository& captureRunRepository_;
    domain::ICamera& camera_;
    bool cameraConnected_{};
};

} // namespace application
