#include "infrastructure/camera/simulated_camera.h"

#include <array>
#include <charconv>
#include <ios>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace infrastructure {
namespace {

using namespace domain;

constexpr std::string_view CaptureIdPrefix{"simulated-capture-"};
constexpr std::string_view TestImageHeader{"P5\n8 8\n255\n"};
constexpr std::array<unsigned char, 64> TestImagePixels{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 255, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

std::string testImage()
{
    std::string image(TestImageHeader);
    image.reserve(TestImageHeader.size() + TestImagePixels.size());
    for (const unsigned char pixel : TestImagePixels) {
        image.push_back(static_cast<char>(pixel));
    }
    return image;
}

CameraError error(
    CameraErrorCode code,
    std::string message,
    std::string operation)
{
    return {code, std::move(message), std::move(operation), "SimulatedCamera"};
}

bool isSupportedExposure(std::chrono::microseconds exposure)
{
    return exposure >= std::chrono::milliseconds{1}
        && exposure <= std::chrono::minutes{30};
}

bool isSupportedIso(unsigned int iso)
{
    constexpr std::array<unsigned int, 7> SupportedIsoValues{
        100, 200, 400, 800, 1600, 3200, 6400
    };
    for (const unsigned int value : SupportedIsoValues) {
        if (iso == value) {
            return true;
        }
    }
    return false;
}

bool isCaptureIdValid(const CameraCapture& capture, std::uint64_t lastCaptureId)
{
    if (capture.fileExtension != ".pgm"
        || !capture.id.starts_with(CaptureIdPrefix)) {
        return false;
    }

    const std::string_view suffix(
        capture.id.data() + CaptureIdPrefix.size(),
        capture.id.size() - CaptureIdPrefix.size());
    std::uint64_t captureId{};
    const auto [end, result] = std::from_chars(
        suffix.data(),
        suffix.data() + suffix.size(),
        captureId);
    return result == std::errc{}
        && end == suffix.data() + suffix.size()
        && captureId > 0
        && captureId <= lastCaptureId;
}

} // namespace

CameraOperationResult SimulatedCamera::connect()
{
    connected_ = true;
    return std::monostate{};
}

CameraOperationResult SimulatedCamera::disconnect()
{
    connected_ = false;
    return std::monostate{};
}

CameraResult<CameraCapabilities> SimulatedCamera::capabilities() const
{
    if (!connected_) {
        return error(
            CameraErrorCode::NotConnected,
            "Simulated camera is not connected",
            "capabilities");
    }
    return supportedCapabilities();
}

CameraResult<CameraSettings> SimulatedCamera::settings() const
{
    if (!connected_) {
        return error(
            CameraErrorCode::NotConnected,
            "Simulated camera is not connected",
            "settings");
    }
    return settings_;
}

CameraOperationResult SimulatedCamera::applySettings(const CameraSettings& settings)
{
    if (!connected_) {
        return error(
            CameraErrorCode::NotConnected,
            "Simulated camera is not connected",
            "applySettings");
    }
    if (settings.exposure && !isSupportedExposure(*settings.exposure)) {
        return error(
            CameraErrorCode::InvalidSettings,
            "Exposure must be between 1 ms and 30 minutes",
            "applySettings");
    }
    if (settings.iso && !isSupportedIso(*settings.iso)) {
        return error(
            CameraErrorCode::InvalidSettings,
            "ISO must be one of 100, 200, 400, 800, 1600, 3200, or 6400",
            "applySettings");
    }

    if (settings.exposure) {
        settings_.exposure = settings.exposure;
    }
    if (settings.iso) {
        settings_.iso = settings.iso;
    }
    return std::monostate{};
}

CameraResult<CameraCapture> SimulatedCamera::capture(const CaptureRequest& request)
{
    if (!connected_) {
        return error(
            CameraErrorCode::NotConnected,
            "Simulated camera is not connected",
            "capture");
    }

    CameraSettings requestedSettings = settings_;
    if (request.settings.exposure) {
        requestedSettings.exposure = request.settings.exposure;
    }
    if (request.settings.iso) {
        requestedSettings.iso = request.settings.iso;
    }
    const CameraOperationResult applyResult = applySettings(requestedSettings);
    if (const auto* cameraError = std::get_if<CameraError>(&applyResult)) {
        CameraError captureError = *cameraError;
        captureError.operation = "capture";
        return captureError;
    }
    if (nextCaptureId_ == std::numeric_limits<std::uint64_t>::max()) {
        return error(
            CameraErrorCode::CaptureFailed,
            "Simulated capture identifier space is exhausted",
            "capture");
    }

    ++nextCaptureId_;
    return CameraCapture{
        std::string(CaptureIdPrefix) + std::to_string(nextCaptureId_),
        ".pgm",
        static_cast<std::uint64_t>(testImage().size())
    };
}

CameraOperationResult SimulatedCamera::transfer(
    const CameraCapture& capture,
    std::ostream& destination,
    TransferProgressCallback progress)
{
    if (!connected_) {
        return error(
            CameraErrorCode::NotConnected,
            "Simulated camera is not connected",
            "transfer");
    }
    if (!isCaptureIdValid(capture, nextCaptureId_)) {
        return error(
            CameraErrorCode::TransferFailed,
            "Capture does not belong to this simulated camera",
            "transfer");
    }

    const std::string image = testImage();
    if (capture.expectedSize && *capture.expectedSize != image.size()) {
        return error(
            CameraErrorCode::TransferFailed,
            "Expected image size does not match the simulated image",
            "transfer");
    }

    try {
        destination.write(image.data(), static_cast<std::streamsize>(image.size()));
    } catch (const std::ios_base::failure&) {
        return error(
            CameraErrorCode::TransferFailed,
            "Unable to write the simulated image to the destination",
            "transfer");
    }
    if (!destination) {
        return error(
            CameraErrorCode::TransferFailed,
            "Unable to write the simulated image to the destination",
            "transfer");
    }

    if (progress) {
        const auto size = static_cast<std::uint64_t>(image.size());
        progress(TransferProgress{size, size});
    }
    return std::monostate{};
}

CameraOperationResult SimulatedCamera::startLiveView()
{
    if (!connected_) {
        return error(
            CameraErrorCode::NotConnected,
            "Simulated camera is not connected",
            "startLiveView");
    }
    return error(
        CameraErrorCode::UnsupportedOperation,
        "Live view is not supported by the basic simulated camera",
        "startLiveView");
}

CameraOperationResult SimulatedCamera::stopLiveView()
{
    if (!connected_) {
        return error(
            CameraErrorCode::NotConnected,
            "Simulated camera is not connected",
            "stopLiveView");
    }
    return error(
        CameraErrorCode::UnsupportedOperation,
        "Live view is not supported by the basic simulated camera",
        "stopLiveView");
}

CameraCapabilities SimulatedCamera::supportedCapabilities()
{
    CameraCapabilities result;
    result.exposure.state = CapabilityState::Supported;
    result.exposure.minimum = std::chrono::milliseconds{1};
    result.exposure.maximum = std::chrono::minutes{30};
    result.iso.state = CapabilityState::Supported;
    result.iso.values = {100, 200, 400, 800, 1600, 3200, 6400};
    result.liveView = CapabilityState::Unavailable;
    return result;
}

} // namespace infrastructure
