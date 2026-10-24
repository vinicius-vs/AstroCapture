#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace domain {

enum class CameraErrorCode
{
    ConnectionFailed,
    DisconnectionFailed,
    NotConnected,
    UnsupportedOperation,
    InvalidSettings,
    CaptureFailed,
    TransferFailed,
    DeviceDisconnected,
    Unknown
};

struct CameraError
{
    CameraErrorCode code{CameraErrorCode::Unknown};
    std::string message;
    std::string operation;
    std::optional<std::string> device;
};

template<typename T>
using CameraResult = std::variant<T, CameraError>;

using CameraOperationResult = CameraResult<std::monostate>;

enum class CapabilityState
{
    Supported,
    ReadOnly,
    Unavailable
};

template<typename T>
struct SettingCapability
{
    CapabilityState state{CapabilityState::Unavailable};
    std::optional<T> minimum;
    std::optional<T> maximum;
    std::vector<T> values;
};

struct CameraCapabilities
{
    SettingCapability<std::chrono::microseconds> exposure;
    SettingCapability<unsigned int> iso;
    CapabilityState liveView{CapabilityState::Unavailable};
};

struct CameraSettings
{
    std::optional<std::chrono::microseconds> exposure;
    std::optional<unsigned int> iso;
};

struct CaptureRequest
{
    CameraSettings settings;
};

struct CameraCapture
{
    std::string id;
    std::string fileExtension;
    std::optional<std::uint64_t> expectedSize;
};

struct TransferProgress
{
    std::uint64_t bytesTransferred{};
    std::optional<std::uint64_t> totalBytes;
};

using TransferProgressCallback = std::function<void(const TransferProgress&)>;

class ICamera
{
public:
    virtual ~ICamera() = default;

    virtual CameraOperationResult connect() = 0;
    virtual CameraOperationResult disconnect() = 0;

    virtual CameraResult<CameraCapabilities> capabilities() const = 0;
    virtual CameraResult<CameraSettings> settings() const = 0;
    virtual CameraOperationResult applySettings(const CameraSettings& settings) = 0;

    virtual CameraResult<CameraCapture> capture(const CaptureRequest& request) = 0;
    virtual CameraOperationResult transfer(
        const CameraCapture& capture,
        std::ostream& destination,
        TransferProgressCallback progress = {}) = 0;

    virtual CameraOperationResult startLiveView() = 0;
    virtual CameraOperationResult stopLiveView() = 0;
};

} // namespace domain
