#pragma once

#include "domain/camera.h"
#include "domain/capture_entities.h"
#include "domain/logging/logging_service.h"
#include "domain/repositories/capture_run_repository.h"
#include "domain/repositories/frame_repository.h"
#include "infrastructure/storage/image_storage.h"

#include <QString>

#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace application {

enum class CaptureFrameStage
{
    Validate,
    ApplySettings,
    Exposure,
    Transfer,
    StoreImage,
    PersistMetadata
};

enum class CaptureRecoveryDecision
{
    Retry,
    Pause,
    Abort
};

class CaptureFrameError final : public std::runtime_error
{
public:
    CaptureFrameError(
        CaptureFrameStage stage,
        domain::FrameId frameId,
        std::string message,
        std::optional<domain::CameraError> cameraError = std::nullopt,
        std::vector<CaptureRecoveryDecision> recoveryOptions = {});

    CaptureFrameStage stage() const noexcept;
    const domain::FrameId& frameId() const noexcept;
    const std::optional<domain::CameraError>& cameraError() const noexcept;
    const std::vector<CaptureRecoveryDecision>& recoveryOptions() const noexcept;

private:
    CaptureFrameStage stage_;
    domain::FrameId frameId_;
    std::optional<domain::CameraError> cameraError_;
    std::vector<CaptureRecoveryDecision> recoveryOptions_;
};

struct CaptureFrameProgress
{
    domain::FrameId frameId;
    domain::FrameState state;
    std::optional<domain::TransferProgress> transfer;
    std::optional<std::filesystem::path> filePath;
};

using CaptureFrameProgressCallback = std::function<void(const CaptureFrameProgress&)>;

class CaptureFrameService final
{
public:
    CaptureFrameService(
        domain::ICamera& camera,
        domain::CaptureRunRepository& captureRunRepository,
        domain::FrameRepository& frameRepository,
        infrastructure::ImageStorage& imageStorage,
        domain::LoggingService& loggingService,
        const domain::Target& target,
        const domain::Session& session,
        domain::CaptureRun& captureRun);

    domain::Frame execute(
        domain::FrameId frameId,
        CaptureFrameProgressCallback progress = {},
        std::function<bool()> stopBeforeExposure = {});
    void applyRecoveryDecision(
        const domain::FrameId& frameId,
        CaptureRecoveryDecision decision);

private:
    void reportState(
        const domain::Frame& frame,
        std::optional<domain::FrameState> previousState,
        const CaptureFrameProgressCallback& progress);

    [[noreturn]] void fail(
        domain::Frame& frame,
        CaptureFrameStage stage,
        std::string message,
        const CaptureFrameProgressCallback& progress,
        std::optional<domain::CameraError> cameraError = std::nullopt);
    [[noreturn]] void failBeforeFrame(
        const domain::FrameId& frameId,
        CaptureFrameStage stage,
        std::string message,
        std::optional<domain::CameraError> cameraError = std::nullopt,
        bool retryAllowed = false);
    void pauseCaptureRun();

    domain::ICamera& camera_;
    domain::CaptureRunRepository& captureRunRepository_;
    domain::FrameRepository& frameRepository_;
    infrastructure::ImageStorage& imageStorage_;
    domain::LoggingService& loggingService_;
    const domain::Target& target_;
    const domain::Session& session_;
    domain::CaptureRun& captureRun_;
    std::set<domain::FrameId> recoverableUnstartedFrames_;
    std::set<domain::FrameId> retryableUnstartedFrames_;
};

} // namespace application
