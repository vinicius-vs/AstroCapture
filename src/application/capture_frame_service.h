#pragma once

#include "domain/camera.h"
#include "domain/capture_entities.h"
#include "domain/logging/logging_service.h"
#include "domain/repositories/frame_repository.h"
#include "infrastructure/storage/image_storage.h"

#include <QString>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

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

class CaptureFrameError final : public std::runtime_error
{
public:
    CaptureFrameError(
        CaptureFrameStage stage,
        domain::FrameId frameId,
        std::string message,
        std::optional<domain::CameraError> cameraError = std::nullopt);

    CaptureFrameStage stage() const noexcept;
    const domain::FrameId& frameId() const noexcept;
    const std::optional<domain::CameraError>& cameraError() const noexcept;

private:
    CaptureFrameStage stage_;
    domain::FrameId frameId_;
    std::optional<domain::CameraError> cameraError_;
};

struct CaptureFrameProgress
{
    domain::FrameId frameId;
    domain::FrameState state;
    std::optional<domain::TransferProgress> transfer;
};

using CaptureFrameProgressCallback = std::function<void(const CaptureFrameProgress&)>;

class CaptureFrameService final
{
public:
    CaptureFrameService(
        domain::ICamera& camera,
        domain::FrameRepository& frameRepository,
        infrastructure::ImageStorage& imageStorage,
        domain::LoggingService& loggingService,
        const domain::Target& target,
        const domain::Session& session,
        const domain::CaptureRun& captureRun);

    domain::Frame execute(
        domain::FrameId frameId,
        CaptureFrameProgressCallback progress = {});

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

    domain::ICamera& camera_;
    domain::FrameRepository& frameRepository_;
    infrastructure::ImageStorage& imageStorage_;
    domain::LoggingService& loggingService_;
    const domain::Target& target_;
    const domain::Session& session_;
    const domain::CaptureRun& captureRun_;
};

} // namespace application
