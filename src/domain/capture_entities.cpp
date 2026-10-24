#include "domain/capture_entities.h"

#include <stdexcept>
#include <utility>

namespace domain {
namespace {

void requireNonEmpty(const std::string& value, const char* fieldName)
{
    if (value.empty()) {
        throw std::invalid_argument(std::string(fieldName) + " must not be empty");
    }
}

bool isValid(SessionStage stage)
{
    switch (stage) {
    case SessionStage::Capture:
    case SessionStage::Stacking:
    case SessionStage::Editing:
    case SessionStage::Finished:
        return true;
    }
    return false;
}

bool isValid(FrameType type)
{
    switch (type) {
    case FrameType::Light:
    case FrameType::Dark:
    case FrameType::Flat:
    case FrameType::Bias:
    case FrameType::DarkFlat:
        return true;
    }
    return false;
}

void requireRunState(CaptureRunState actual, CaptureRunState expected)
{
    if (actual != expected) {
        throw std::logic_error("Invalid capture run state transition");
    }
}

void requireFrameState(FrameState actual, FrameState expected)
{
    if (actual != expected) {
        throw std::logic_error("Invalid frame state transition");
    }
}

} // namespace

Target::Target(TargetId id, std::string name)
    : id_(std::move(id))
    , name_(std::move(name))
{
    requireNonEmpty(id_, "Target id");
    requireNonEmpty(name_, "Target name");
}

const TargetId& Target::id() const noexcept
{
    return id_;
}

const std::string& Target::name() const noexcept
{
    return name_;
}

Session::Session(
    SessionId id,
    TargetId targetId,
    std::string name,
    SessionStage stage,
    Timestamp createdAt,
    Timestamp updatedAt)
    : id_(std::move(id))
    , targetId_(std::move(targetId))
    , name_(std::move(name))
    , stage_(stage)
    , createdAt_(createdAt)
    , updatedAt_(updatedAt)
{
    requireNonEmpty(id_, "Session id");
    requireNonEmpty(targetId_, "Session target id");
    requireNonEmpty(name_, "Session name");
    if (!isValid(stage_)) {
        throw std::invalid_argument("Invalid session stage");
    }
    if (updatedAt_ < createdAt_) {
        throw std::invalid_argument("Session updated time cannot precede creation time");
    }
}

const SessionId& Session::id() const noexcept
{
    return id_;
}

const TargetId& Session::targetId() const noexcept
{
    return targetId_;
}

const std::string& Session::name() const noexcept
{
    return name_;
}

SessionStage Session::stage() const noexcept
{
    return stage_;
}

Timestamp Session::createdAt() const noexcept
{
    return createdAt_;
}

Timestamp Session::updatedAt() const noexcept
{
    return updatedAt_;
}

void Session::setStage(SessionStage stage, Timestamp updatedAt)
{
    if (!isValid(stage)) {
        throw std::invalid_argument("Invalid session stage");
    }
    if (updatedAt < updatedAt_) {
        throw std::invalid_argument("Session update time cannot move backwards");
    }

    stage_ = stage;
    updatedAt_ = updatedAt;
}

void CaptureSettings::validate() const
{
    if (!isValid(frameType)) {
        throw std::invalid_argument("Invalid frame type");
    }
    if (exposure <= std::chrono::microseconds::zero()) {
        throw std::invalid_argument("Exposure must be greater than zero");
    }
    if (iso && *iso == 0) {
        throw std::invalid_argument("ISO must be greater than zero when specified");
    }
    if (mode != CaptureMode::Finite && mode != CaptureMode::Continuous) {
        throw std::invalid_argument("Invalid capture mode");
    }

    if (mode == CaptureMode::Finite) {
        if (!plannedFrameCount || *plannedFrameCount == 0) {
            throw std::invalid_argument("Finite captures require a positive planned frame count");
        }
    } else if (plannedFrameCount) {
        throw std::invalid_argument("Continuous captures must not have a planned frame count");
    }
}

CaptureRun::CaptureRun(
    CaptureRunId id,
    SessionId sessionId,
    std::chrono::year_month_day acquisitionDate,
    CaptureSettings settings)
    : id_(std::move(id))
    , sessionId_(std::move(sessionId))
    , acquisitionDate_(acquisitionDate)
    , settings_(std::move(settings))
{
    requireNonEmpty(id_, "Capture run id");
    requireNonEmpty(sessionId_, "Capture run session id");
    if (!acquisitionDate_.ok()) {
        throw std::invalid_argument("Invalid capture acquisition date");
    }
    settings_.validate();
}

CaptureRun CaptureRun::restore(
    CaptureRunId id,
    SessionId sessionId,
    std::chrono::year_month_day acquisitionDate,
    CaptureSettings settings,
    CaptureRunState state)
{
    CaptureRun run(std::move(id), std::move(sessionId), acquisitionDate, std::move(settings));
    switch (state) {
    case CaptureRunState::Ready:
        break;
    case CaptureRunState::Running:
        run.start();
        break;
    case CaptureRunState::StopRequested:
        run.start();
        run.requestStop();
        break;
    case CaptureRunState::Paused:
        run.start();
        run.pause();
        break;
    case CaptureRunState::Completed:
        run.start();
        run.complete();
        break;
    case CaptureRunState::Failed:
        run.start();
        run.fail();
        break;
    default:
        throw std::invalid_argument("Invalid capture run state");
    }
    return run;
}

const CaptureRunId& CaptureRun::id() const noexcept
{
    return id_;
}

const SessionId& CaptureRun::sessionId() const noexcept
{
    return sessionId_;
}

std::chrono::year_month_day CaptureRun::acquisitionDate() const noexcept
{
    return acquisitionDate_;
}

const CaptureSettings& CaptureRun::settings() const noexcept
{
    return settings_;
}

CaptureRunState CaptureRun::state() const noexcept
{
    return state_;
}

void CaptureRun::start()
{
    requireRunState(state_, CaptureRunState::Ready);
    state_ = CaptureRunState::Running;
}

void CaptureRun::requestStop()
{
    requireRunState(state_, CaptureRunState::Running);
    state_ = CaptureRunState::StopRequested;
}

void CaptureRun::pause()
{
    if (state_ != CaptureRunState::Running && state_ != CaptureRunState::StopRequested) {
        throw std::logic_error("Invalid capture run state transition");
    }
    state_ = CaptureRunState::Paused;
}

void CaptureRun::resume()
{
    requireRunState(state_, CaptureRunState::Paused);
    state_ = CaptureRunState::Running;
}

void CaptureRun::complete()
{
    const bool canComplete = state_ == CaptureRunState::Running
        || state_ == CaptureRunState::StopRequested
        || state_ == CaptureRunState::Paused;
    if (!canComplete) {
        throw std::logic_error("Invalid capture run state transition");
    }
    if (settings_.mode == CaptureMode::Continuous
        && state_ != CaptureRunState::StopRequested
        && state_ != CaptureRunState::Paused) {
        throw std::logic_error("Continuous capture runs cannot be completed without a completion condition");
    }
    state_ = CaptureRunState::Completed;
}

void CaptureRun::fail()
{
    if (state_ != CaptureRunState::Running
        && state_ != CaptureRunState::StopRequested
        && state_ != CaptureRunState::Paused) {
        throw std::logic_error("Invalid capture run state transition");
    }
    state_ = CaptureRunState::Failed;
}

Frame::Frame(
    FrameId id,
    CaptureRunId captureRunId,
    FrameType type,
    std::optional<std::chrono::microseconds> exposure,
    std::optional<unsigned int> iso)
    : id_(std::move(id))
    , captureRunId_(std::move(captureRunId))
    , type_(type)
    , exposure_(exposure)
    , iso_(iso)
{
    requireNonEmpty(id_, "Frame id");
    requireNonEmpty(captureRunId_, "Frame capture run id");
    if (!isValid(type_)) {
        throw std::invalid_argument("Invalid frame type");
    }
    if (exposure_ && *exposure_ <= std::chrono::microseconds::zero()) {
        throw std::invalid_argument("Frame exposure must be greater than zero when specified");
    }
    if (iso_ && *iso_ == 0) {
        throw std::invalid_argument("Frame ISO must be greater than zero when specified");
    }
}

Frame Frame::restore(
    FrameId id,
    CaptureRunId captureRunId,
    FrameType type,
    FrameState state,
    std::optional<std::chrono::microseconds> exposure,
    std::optional<unsigned int> iso,
    std::optional<Timestamp> capturedAt,
    std::optional<std::filesystem::path> filePath,
    std::optional<std::uint64_t> fileSize,
    std::optional<std::string> errorMessage)
{
    Frame frame(std::move(id), std::move(captureRunId), type, exposure, iso);

    switch (state) {
    case FrameState::Pending:
        if (capturedAt || filePath || fileSize || errorMessage) {
            throw std::invalid_argument("Pending frames cannot contain capture results");
        }
        break;
    case FrameState::Exposing:
        if (!capturedAt || filePath || fileSize || errorMessage) {
            throw std::invalid_argument("Invalid persisted data for an exposing frame");
        }
        frame.startExposure(*capturedAt);
        break;
    case FrameState::Transferring:
        if (!capturedAt || filePath || fileSize || errorMessage) {
            throw std::invalid_argument("Invalid persisted data for a transferring frame");
        }
        frame.startExposure(*capturedAt);
        frame.startTransfer();
        break;
    case FrameState::Validating:
        if (!capturedAt || filePath || fileSize || errorMessage) {
            throw std::invalid_argument("Invalid persisted data for a validating frame");
        }
        frame.startExposure(*capturedAt);
        frame.startTransfer();
        frame.startValidation();
        break;
    case FrameState::Completed:
        if (!capturedAt || !filePath || !fileSize || errorMessage) {
            throw std::invalid_argument("Invalid persisted data for a completed frame");
        }
        frame.startExposure(*capturedAt);
        frame.startTransfer();
        frame.startValidation();
        frame.completeValidation(std::move(*filePath), *fileSize);
        break;
    case FrameState::Failed:
        if (!capturedAt || !errorMessage || filePath || fileSize) {
            throw std::invalid_argument("Invalid persisted data for a failed frame");
        }
        frame.startExposure(*capturedAt);
        frame.fail(std::move(*errorMessage));
        break;
    default:
        throw std::invalid_argument("Invalid frame state");
    }

    return frame;
}

const FrameId& Frame::id() const noexcept
{
    return id_;
}

const CaptureRunId& Frame::captureRunId() const noexcept
{
    return captureRunId_;
}

FrameType Frame::type() const noexcept
{
    return type_;
}

FrameState Frame::state() const noexcept
{
    return state_;
}

std::optional<std::chrono::microseconds> Frame::exposure() const noexcept
{
    return exposure_;
}

std::optional<unsigned int> Frame::iso() const noexcept
{
    return iso_;
}

std::optional<Timestamp> Frame::capturedAt() const noexcept
{
    return capturedAt_;
}

const std::optional<std::filesystem::path>& Frame::filePath() const noexcept
{
    return filePath_;
}

std::optional<std::uint64_t> Frame::fileSize() const noexcept
{
    return fileSize_;
}

const std::optional<std::string>& Frame::errorMessage() const noexcept
{
    return errorMessage_;
}

void Frame::startExposure(Timestamp capturedAt)
{
    requireFrameState(state_, FrameState::Pending);
    capturedAt_ = capturedAt;
    errorMessage_.reset();
    state_ = FrameState::Exposing;
}

void Frame::startTransfer()
{
    requireFrameState(state_, FrameState::Exposing);
    state_ = FrameState::Transferring;
}

void Frame::startValidation()
{
    requireFrameState(state_, FrameState::Transferring);
    state_ = FrameState::Validating;
}

void Frame::completeValidation(std::filesystem::path filePath, std::uint64_t fileSize)
{
    requireFrameState(state_, FrameState::Validating);
    if (filePath.empty()) {
        throw std::invalid_argument("Completed frame file path must not be empty");
    }
    if (fileSize == 0) {
        throw std::invalid_argument("Completed frame file size must be greater than zero");
    }

    filePath_ = std::move(filePath);
    fileSize_ = fileSize;
    errorMessage_.reset();
    state_ = FrameState::Completed;
}

void Frame::fail(std::string errorMessage)
{
    if (state_ != FrameState::Exposing
        && state_ != FrameState::Transferring
        && state_ != FrameState::Validating) {
        throw std::logic_error("Invalid frame state transition");
    }
    requireNonEmpty(errorMessage, "Frame error message");
    errorMessage_ = std::move(errorMessage);
    filePath_.reset();
    fileSize_.reset();
    state_ = FrameState::Failed;
}

void Frame::retry()
{
    requireFrameState(state_, FrameState::Failed);
    capturedAt_.reset();
    filePath_.reset();
    fileSize_.reset();
    errorMessage_.reset();
    state_ = FrameState::Pending;
}

} // namespace domain
