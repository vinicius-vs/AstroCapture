#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace domain {

struct CameraCapabilities;
struct CameraSettings;

using TargetId = std::string;
using SessionId = std::string;
using CaptureRunId = std::string;
using FrameId = std::string;
using Timestamp = std::chrono::system_clock::time_point;

enum class SessionStage
{
    Capture,
    Stacking,
    Editing,
    Finished
};

enum class FrameType
{
    Light,
    Dark,
    Flat,
    Bias,
    DarkFlat
};

enum class CaptureMode
{
    Finite,
    Continuous
};

enum class CaptureRunState
{
    Ready,
    Running,
    StopRequested,
    Paused,
    Completed,
    Failed
};

enum class FrameState
{
    Pending,
    Exposing,
    Transferring,
    Validating,
    Completed,
    Failed
};

class Target final
{
public:
    Target(TargetId id, std::string name);

    const TargetId& id() const noexcept;
    const std::string& name() const noexcept;

private:
    TargetId id_;
    std::string name_;
};

class Session final
{
public:
    Session(
        SessionId id,
        TargetId targetId,
        std::string name,
        SessionStage stage,
        Timestamp createdAt,
        Timestamp updatedAt);

    const SessionId& id() const noexcept;
    const TargetId& targetId() const noexcept;
    const std::string& name() const noexcept;
    SessionStage stage() const noexcept;
    Timestamp createdAt() const noexcept;
    Timestamp updatedAt() const noexcept;

    void setStage(SessionStage stage, Timestamp updatedAt);

private:
    SessionId id_;
    TargetId targetId_;
    std::string name_;
    SessionStage stage_;
    Timestamp createdAt_;
    Timestamp updatedAt_;
};

struct CaptureSettings
{
    FrameType frameType;
    std::chrono::microseconds exposure;
    std::optional<unsigned int> iso;
    CaptureMode mode;
    std::optional<std::uint32_t> plannedFrameCount;

    void validate() const;
    void validate(
        const CameraCapabilities& capabilities,
        const CameraSettings& currentSettings) const;
};

class CaptureRun final
{
public:
    CaptureRun(
        CaptureRunId id,
        SessionId sessionId,
        std::chrono::year_month_day acquisitionDate,
        CaptureSettings settings);
    static CaptureRun restore(
        CaptureRunId id,
        SessionId sessionId,
        std::chrono::year_month_day acquisitionDate,
        CaptureSettings settings,
        CaptureRunState state);

    const CaptureRunId& id() const noexcept;
    const SessionId& sessionId() const noexcept;
    std::chrono::year_month_day acquisitionDate() const noexcept;
    const CaptureSettings& settings() const noexcept;
    CaptureRunState state() const noexcept;

    void start();
    void requestStop();
    void pause();
    void resume();
    void complete();
    void fail();

private:
    CaptureRunId id_;
    SessionId sessionId_;
    std::chrono::year_month_day acquisitionDate_;
    CaptureSettings settings_;
    CaptureRunState state_{CaptureRunState::Ready};
};

class Frame final
{
public:
    Frame(
        FrameId id,
        CaptureRunId captureRunId,
        FrameType type,
        std::optional<std::chrono::microseconds> exposure = std::nullopt,
        std::optional<unsigned int> iso = std::nullopt);
    static Frame restore(
        FrameId id,
        CaptureRunId captureRunId,
        FrameType type,
        FrameState state,
        std::optional<std::chrono::microseconds> exposure,
        std::optional<unsigned int> iso,
        std::optional<Timestamp> capturedAt,
        std::optional<std::filesystem::path> filePath,
        std::optional<std::uint64_t> fileSize,
        std::optional<std::string> errorMessage);

    const FrameId& id() const noexcept;
    const CaptureRunId& captureRunId() const noexcept;
    FrameType type() const noexcept;
    FrameState state() const noexcept;
    std::optional<std::chrono::microseconds> exposure() const noexcept;
    std::optional<unsigned int> iso() const noexcept;
    std::optional<Timestamp> capturedAt() const noexcept;
    const std::optional<std::filesystem::path>& filePath() const noexcept;
    std::optional<std::uint64_t> fileSize() const noexcept;
    const std::optional<std::string>& errorMessage() const noexcept;

    void startExposure(Timestamp capturedAt);
    void startTransfer();
    void startValidation();
    void completeValidation(std::filesystem::path filePath, std::uint64_t fileSize);
    void fail(std::string errorMessage);
    void retry();

private:
    FrameId id_;
    CaptureRunId captureRunId_;
    FrameType type_;
    FrameState state_{FrameState::Pending};
    std::optional<std::chrono::microseconds> exposure_;
    std::optional<unsigned int> iso_;
    std::optional<Timestamp> capturedAt_;
    std::optional<std::filesystem::path> filePath_;
    std::optional<std::uint64_t> fileSize_;
    std::optional<std::string> errorMessage_;
};

} // namespace domain
