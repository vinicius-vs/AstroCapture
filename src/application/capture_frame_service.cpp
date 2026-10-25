#include "application/capture_frame_service.h"

#include <QFile>
#include <QTemporaryFile>

#include <array>
#include <chrono>
#include <ios>
#include <limits>
#include <ostream>
#include <streambuf>
#include <utility>

namespace application {
namespace {

using namespace domain;

QString frameStateName(FrameState state)
{
    switch (state) {
    case FrameState::Pending:
        return QStringLiteral("Pending");
    case FrameState::Exposing:
        return QStringLiteral("Exposing");
    case FrameState::Transferring:
        return QStringLiteral("Transferring");
    case FrameState::Validating:
        return QStringLiteral("Validating");
    case FrameState::Completed:
        return QStringLiteral("Completed");
    case FrameState::Failed:
        return QStringLiteral("Failed");
    }
    throw std::invalid_argument("Invalid frame state");
}

QString cameraErrorCodeName(CameraErrorCode code)
{
    switch (code) {
    case CameraErrorCode::ConnectionFailed:
        return QStringLiteral("ConnectionFailed");
    case CameraErrorCode::DisconnectionFailed:
        return QStringLiteral("DisconnectionFailed");
    case CameraErrorCode::NotConnected:
        return QStringLiteral("NotConnected");
    case CameraErrorCode::UnsupportedOperation:
        return QStringLiteral("UnsupportedOperation");
    case CameraErrorCode::InvalidSettings:
        return QStringLiteral("InvalidSettings");
    case CameraErrorCode::CaptureFailed:
        return QStringLiteral("CaptureFailed");
    case CameraErrorCode::TransferFailed:
        return QStringLiteral("TransferFailed");
    case CameraErrorCode::DeviceDisconnected:
        return QStringLiteral("DeviceDisconnected");
    case CameraErrorCode::Unknown:
        return QStringLiteral("Unknown");
    }
    throw std::invalid_argument("Invalid camera error code");
}

QString recoveryDecisionName(CaptureRecoveryDecision decision)
{
    switch (decision) {
    case CaptureRecoveryDecision::Retry:
        return QStringLiteral("Retry");
    case CaptureRecoveryDecision::Pause:
        return QStringLiteral("Pause");
    case CaptureRecoveryDecision::Abort:
        return QStringLiteral("Abort");
    }
    throw std::invalid_argument("Invalid capture recovery decision");
}

QString captureFrameStageName(CaptureFrameStage stage)
{
    switch (stage) {
    case CaptureFrameStage::Validate:
        return QStringLiteral("validate");
    case CaptureFrameStage::ApplySettings:
        return QStringLiteral("applySettings");
    case CaptureFrameStage::Exposure:
        return QStringLiteral("exposure");
    case CaptureFrameStage::Transfer:
        return QStringLiteral("transfer");
    case CaptureFrameStage::StoreImage:
        return QStringLiteral("storeImage");
    case CaptureFrameStage::PersistMetadata:
        return QStringLiteral("persistMetadata");
    }
    throw std::invalid_argument("Invalid capture frame failure stage");
}

std::string cameraErrorMessage(const CameraError& cameraError)
{
    if (!cameraError.message.empty()) {
        return cameraError.message;
    }
    if (!cameraError.operation.empty()) {
        return "Camera operation failed: " + cameraError.operation;
    }
    return "Camera operation failed";
}

bool hasValidImageExtension(const std::string& extension)
{
    if (extension.size() < 2 || extension.size() > 11 || extension.front() != '.') {
        return false;
    }
    for (std::size_t index = 1; index < extension.size(); ++index) {
        const char character = extension[index];
        if (!((character >= 'a' && character <= 'z')
              || (character >= 'A' && character <= 'Z')
              || (character >= '0' && character <= '9'))) {
            return false;
        }
    }
    return true;
}

class DeviceStreamBuffer final : public std::streambuf
{
public:
    explicit DeviceStreamBuffer(QIODevice& device)
        : device_(device)
    {
    }

protected:
    std::streamsize xsputn(const char* data, std::streamsize size) override
    {
        if (size <= 0) {
            return 0;
        }
        if (static_cast<std::uintmax_t>(size)
            > static_cast<std::uintmax_t>(std::numeric_limits<qint64>::max())) {
            return 0;
        }

        qint64 totalWritten = 0;
        const qint64 requested = static_cast<qint64>(size);
        while (totalWritten < requested) {
            const qint64 written = device_.write(data + totalWritten, requested - totalWritten);
            if (written <= 0) {
                break;
            }
            totalWritten += written;
        }
        return static_cast<std::streamsize>(totalWritten);
    }

    int_type overflow(int_type character) override
    {
        if (traits_type::eq_int_type(character, traits_type::eof())) {
            return traits_type::not_eof(character);
        }
        const char byte = traits_type::to_char_type(character);
        return xsputn(&byte, 1) == 1 ? character : traits_type::eof();
    }

private:
    QIODevice& device_;
};

std::filesystem::path filesystemPath(const QString& path)
{
    const QByteArray encodedPath = QFile::encodeName(path);
    return std::filesystem::path(encodedPath.constData());
}

} // namespace

CaptureFrameError::CaptureFrameError(
    CaptureFrameStage stage,
    domain::FrameId frameId,
    std::string message,
    std::optional<domain::CameraError> cameraError,
    std::vector<CaptureRecoveryDecision> recoveryOptions)
    : std::runtime_error(std::move(message))
    , stage_(stage)
    , frameId_(std::move(frameId))
    , cameraError_(std::move(cameraError))
    , recoveryOptions_(std::move(recoveryOptions))
{
}

CaptureFrameStage CaptureFrameError::stage() const noexcept
{
    return stage_;
}

const domain::FrameId& CaptureFrameError::frameId() const noexcept
{
    return frameId_;
}

const std::optional<domain::CameraError>& CaptureFrameError::cameraError() const noexcept
{
    return cameraError_;
}

const std::vector<CaptureRecoveryDecision>& CaptureFrameError::recoveryOptions() const noexcept
{
    return recoveryOptions_;
}

CaptureFrameService::CaptureFrameService(
    domain::ICamera& camera,
    domain::CaptureRunRepository& captureRunRepository,
    domain::FrameRepository& frameRepository,
    infrastructure::ImageStorage& imageStorage,
    domain::LoggingService& loggingService,
    const domain::Target& target,
    const domain::Session& session,
    domain::CaptureRun& captureRun)
    : camera_(camera)
    , captureRunRepository_(captureRunRepository)
    , frameRepository_(frameRepository)
    , imageStorage_(imageStorage)
    , loggingService_(loggingService)
    , target_(target)
    , session_(session)
    , captureRun_(captureRun)
{
}

domain::Frame CaptureFrameService::execute(
    domain::FrameId frameId,
    CaptureFrameProgressCallback progress,
    std::function<bool()> stopBeforeExposure)
{
    if (captureRun_.state() != CaptureRunState::Running) {
        throw CaptureFrameError(
            CaptureFrameStage::Validate,
            frameId,
            "Capture run must be Running before capturing a frame");
    }
    if (captureRun_.sessionId() != session_.id()
        || session_.targetId() != target_.id()) {
        throw CaptureFrameError(
            CaptureFrameStage::Validate,
            frameId,
            "Capture run, session, and target do not belong together");
    }

    CameraResult<CameraCapabilities> capabilitiesResult = CameraError{};
    try {
        capabilitiesResult = camera_.capabilities();
    } catch (const std::exception& error) {
        failBeforeFrame(
            frameId,
            CaptureFrameStage::Validate,
            error.what(),
            CameraError{CameraErrorCode::Unknown, error.what(), "capabilities", std::nullopt},
            true);
    }
    if (const auto* cameraError = std::get_if<CameraError>(&capabilitiesResult)) {
        failBeforeFrame(
            frameId,
            CaptureFrameStage::Validate,
            cameraErrorMessage(*cameraError),
            *cameraError,
            true);
    }
    CameraResult<CameraSettings> currentSettingsResult = CameraError{};
    try {
        currentSettingsResult = camera_.settings();
    } catch (const std::exception& error) {
        failBeforeFrame(
            frameId,
            CaptureFrameStage::Validate,
            error.what(),
            CameraError{CameraErrorCode::Unknown, error.what(), "settings", std::nullopt},
            true);
    }
    if (const auto* cameraError = std::get_if<CameraError>(&currentSettingsResult)) {
        failBeforeFrame(
            frameId,
            CaptureFrameStage::Validate,
            cameraErrorMessage(*cameraError),
            *cameraError,
            true);
    }
    try {
        captureRun_.settings().validate(
            std::get<CameraCapabilities>(capabilitiesResult),
            std::get<CameraSettings>(currentSettingsResult));
    } catch (const std::invalid_argument& error) {
        failBeforeFrame(
            frameId,
            CaptureFrameStage::Validate,
            error.what());
    }

    std::optional<Frame> existingFrame = frameRepository_.findById(frameId);
    Frame frame = [&]() {
        if (existingFrame) {
            if (existingFrame->state() != FrameState::Pending
                || existingFrame->captureRunId() != captureRun_.id()
                || existingFrame->type() != captureRun_.settings().frameType
                || existingFrame->exposure() != captureRun_.settings().exposure
                || existingFrame->iso() != captureRun_.settings().iso) {
                throw CaptureFrameError(
                    CaptureFrameStage::Validate,
                    frameId,
                    "Only the unchanged Pending frame can be captured or retried");
            }
            return *existingFrame;
        }
        Frame newFrame(
            std::move(frameId),
            captureRun_.id(),
            captureRun_.settings().frameType,
            captureRun_.settings().exposure,
            captureRun_.settings().iso);
        try {
            frameRepository_.create(newFrame);
        } catch (...) {
            pauseCaptureRun();
            throw;
        }
        return newFrame;
    }();
    if (!existingFrame) {
        recoverableUnstartedFrames_.erase(frame.id());
        retryableUnstartedFrames_.erase(frame.id());
        reportState(frame, std::nullopt, progress);
    } else {
        recoverableUnstartedFrames_.erase(frame.id());
        retryableUnstartedFrames_.erase(frame.id());
    }

    if (stopBeforeExposure && stopBeforeExposure()) {
        return frame;
    }

    LogEvent attemptEvent;
    attemptEvent.severity = LogSeverity::Info;
    attemptEvent.event = QStringLiteral("capture.frame.attempt_started");
    attemptEvent.message = QStringLiteral("Capture attempt started");
    attemptEvent.sessionId = session_.id();
    attemptEvent.captureRunId = captureRun_.id();
    attemptEvent.frameId = frame.id();
    attemptEvent.operation = QStringLiteral("captureFrame");
    attemptEvent.state = frameStateName(frame.state());
    attemptEvent.captureSettings = captureRun_.settings();
    try {
        loggingService_.log(attemptEvent);
    } catch (...) {
        pauseCaptureRun();
        throw;
    }

    const CameraSettings requestedSettings{
        captureRun_.settings().exposure,
        captureRun_.settings().iso
    };
    CameraOperationResult applyResult{std::monostate{}};
    try {
        applyResult = camera_.applySettings(requestedSettings);
    } catch (const std::exception& error) {
        fail(
            frame,
            CaptureFrameStage::ApplySettings,
            error.what(),
            progress,
            CameraError{CameraErrorCode::Unknown, error.what(), "applySettings", std::nullopt});
    }
    if (const auto* cameraError = std::get_if<CameraError>(&applyResult)) {
        fail(
            frame,
            CaptureFrameStage::ApplySettings,
            cameraErrorMessage(*cameraError),
            progress,
            *cameraError);
    }

    if (stopBeforeExposure && stopBeforeExposure()) {
        return frame;
    }

    frame.startExposure(
        std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now()));
    try {
        frameRepository_.update(frame);
    } catch (...) {
        pauseCaptureRun();
        throw;
    }
    reportState(frame, FrameState::Pending, progress);

    CameraResult<CameraCapture> captureResult = CameraError{};
    try {
        captureResult = camera_.capture(CaptureRequest{requestedSettings});
    } catch (const std::exception& error) {
        fail(
            frame,
            CaptureFrameStage::Exposure,
            error.what(),
            progress,
            CameraError{CameraErrorCode::Unknown, error.what(), "capture", std::nullopt});
    }
    if (const auto* cameraError = std::get_if<CameraError>(&captureResult)) {
        fail(
            frame,
            CaptureFrameStage::Exposure,
            cameraErrorMessage(*cameraError),
            progress,
            *cameraError);
    }
    const CameraCapture cameraCapture = std::get<CameraCapture>(captureResult);
    if (!hasValidImageExtension(cameraCapture.fileExtension)) {
        fail(
            frame,
            CaptureFrameStage::Exposure,
            "Camera returned an invalid image file extension",
            progress);
    }

    QTemporaryFile temporaryImage;
    temporaryImage.setAutoRemove(true);
    if (!temporaryImage.open()) {
        fail(
            frame,
            CaptureFrameStage::Transfer,
            "Unable to create a temporary image file: "
                + temporaryImage.errorString().toStdString(),
            progress);
    }

    frame.startTransfer();
    try {
        frameRepository_.update(frame);
    } catch (...) {
        pauseCaptureRun();
        throw;
    }
    reportState(frame, FrameState::Exposing, progress);

    DeviceStreamBuffer streamBuffer(temporaryImage);
    std::ostream destination(&streamBuffer);
    CameraOperationResult transferResult{std::monostate{}};
    try {
        transferResult = camera_.transfer(
            cameraCapture,
            destination,
            [&frame, &progress](const TransferProgress& transferProgress) {
                if (progress) {
                    progress(CaptureFrameProgress{
                        frame.id(),
                        frame.state(),
                        transferProgress
                    });
                }
            });
    } catch (const std::exception& error) {
        fail(
            frame,
            CaptureFrameStage::Transfer,
            error.what(),
            progress);
    }
    if (const auto* cameraError = std::get_if<CameraError>(&transferResult)) {
        fail(
            frame,
            CaptureFrameStage::Transfer,
            cameraErrorMessage(*cameraError),
            progress,
            *cameraError);
    }
    if (!destination || !temporaryImage.flush()) {
        fail(
            frame,
            CaptureFrameStage::Transfer,
            "Unable to write the transferred image to the temporary file",
            progress);
    }

    frame.startValidation();
    try {
        frameRepository_.update(frame);
    } catch (...) {
        pauseCaptureRun();
        throw;
    }
    reportState(frame, FrameState::Transferring, progress);

    if (!temporaryImage.seek(0)) {
        fail(
            frame,
            CaptureFrameStage::StoreImage,
            "Unable to rewind the transferred image: "
                + temporaryImage.errorString().toStdString(),
            progress);
    }

    infrastructure::StoredImage storedImage;
    try {
        storedImage = imageStorage_.store(
            target_,
            session_,
            captureRun_,
            frame,
            QString::fromStdString("frame" + cameraCapture.fileExtension),
            temporaryImage,
            cameraCapture.expectedSize);
    } catch (const std::exception& error) {
        fail(
            frame,
            CaptureFrameStage::StoreImage,
            error.what(),
            progress);
    }

    frame.completeValidation(
        filesystemPath(storedImage.filePath),
        storedImage.fileSize);
    try {
        frameRepository_.update(frame);
    } catch (const std::exception& error) {
        pauseCaptureRun();
        LogEvent event;
        event.severity = LogSeverity::Error;
        event.event = QStringLiteral("capture.frame.metadata_persistence_failed");
        event.message = QString::fromStdString(error.what());
        event.sessionId = session_.id();
        event.captureRunId = captureRun_.id();
        event.frameId = frame.id();
        event.operation = QStringLiteral("persistCompletedFrame");
        event.previousState = QStringLiteral("Validating");
        event.state = QStringLiteral("Validating");
        event.technicalDetails = storedImage.filePath;
        loggingService_.log(event);
        throw CaptureFrameError(
            CaptureFrameStage::PersistMetadata,
            frame.id(),
            error.what(),
            std::nullopt,
            {CaptureRecoveryDecision::Pause, CaptureRecoveryDecision::Abort});
    }
    reportState(frame, FrameState::Validating, progress);
    return frame;
}

void CaptureFrameService::applyRecoveryDecision(
    const domain::FrameId& frameId,
    CaptureRecoveryDecision decision)
{
    if (captureRun_.state() != CaptureRunState::Paused) {
        throw std::logic_error("Capture run must be Paused before applying a recovery decision");
    }
    const auto frame = frameRepository_.findById(frameId);
    if ((frame && frame->captureRunId() != captureRun_.id())
        || (!frame && !recoverableUnstartedFrames_.contains(frameId))) {
        throw std::invalid_argument("Recovery decision refers to an unknown frame");
    }

    switch (decision) {
    case CaptureRecoveryDecision::Retry:
        if ((frame && frame->state() != FrameState::Pending)
            || (!frame && !retryableUnstartedFrames_.contains(frameId))) {
            throw std::logic_error(
                "Retry is only safe when no exposure was started for this frame");
        }
        break;
    case CaptureRecoveryDecision::Pause:
    case CaptureRecoveryDecision::Abort:
        break;
    default:
        throw std::invalid_argument("Invalid capture recovery decision");
    }

    LogEvent event;
    event.severity = LogSeverity::Info;
    event.event = QStringLiteral("capture.recovery.decision");
    event.message = QStringLiteral("User selected a capture recovery action");
    event.sessionId = session_.id();
    event.captureRunId = captureRun_.id();
    event.frameId = frameId;
    event.operation = QStringLiteral("capture.recovery");
    event.state = recoveryDecisionName(decision);
    loggingService_.log(event);

    switch (decision) {
    case CaptureRecoveryDecision::Retry:
        captureRunRepository_.updateState(captureRun_.id(), CaptureRunState::Running);
        captureRun_.resume();
        break;
    case CaptureRecoveryDecision::Pause:
        break;
    case CaptureRecoveryDecision::Abort:
        captureRunRepository_.updateState(captureRun_.id(), CaptureRunState::Failed);
        captureRun_.fail();
        break;
    default:
        break;
    }
}

void CaptureFrameService::reportState(
    const domain::Frame& frame,
    std::optional<domain::FrameState> previousState,
    const CaptureFrameProgressCallback& progress)
{
    const QString stateName = frameStateName(frame.state());
    LogEvent event;
    event.severity = LogSeverity::Info;
    event.event = QStringLiteral("capture.frame.state_changed");
    event.message = QStringLiteral("Frame state changed to %1").arg(stateName);
    event.sessionId = session_.id();
    event.captureRunId = captureRun_.id();
    event.frameId = frame.id();
    event.operation = QStringLiteral("captureFrame");
    event.previousState = previousState
        ? std::optional<QString>{frameStateName(*previousState)}
        : std::nullopt;
    event.state = stateName;
    event.captureSettings = captureRun_.settings();
    try {
        loggingService_.log(event);
        if (progress) {
            progress(CaptureFrameProgress{frame.id(), frame.state(), std::nullopt});
        }
    } catch (...) {
        pauseCaptureRun();
        throw;
    }
}

[[noreturn]] void CaptureFrameService::fail(
    domain::Frame& frame,
    CaptureFrameStage stage,
    std::string message,
    const CaptureFrameProgressCallback& progress,
    std::optional<domain::CameraError> cameraError)
{
    const FrameState previousState = frame.state();
    std::vector<CaptureRecoveryDecision> recoveryOptions{
        CaptureRecoveryDecision::Pause,
        CaptureRecoveryDecision::Abort
    };
    if (frame.state() == FrameState::Pending) {
        recoveryOptions.insert(recoveryOptions.begin(), CaptureRecoveryDecision::Retry);
    } else {
        frame.fail(message);
    }

    pauseCaptureRun();
    if (frame.state() == FrameState::Failed) {
        frameRepository_.update(frame);
    }

    LogEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.severity = LogSeverity::Error;
    event.event = QStringLiteral("capture.frame.failed");
    event.message = QString::fromStdString(message);
    event.sessionId = session_.id();
    event.captureRunId = captureRun_.id();
    event.frameId = frame.id();
    event.operation = captureFrameStageName(stage);
    event.previousState = frameStateName(previousState);
    event.state = frameStateName(frame.state());
    event.errorCode = cameraError
        ? std::optional<QString>{cameraErrorCodeName(cameraError->code)}
        : std::nullopt;
    event.technicalDetails = cameraError
        ? std::optional<QString>{QString::fromStdString(cameraError->operation)}
        : std::nullopt;
    event.captureSettings = captureRun_.settings();
    loggingService_.log(event);
    if (progress) {
        progress(CaptureFrameProgress{frame.id(), frame.state(), std::nullopt});
    }
    throw CaptureFrameError(
        stage,
        frame.id(),
        std::move(message),
        std::move(cameraError),
        std::move(recoveryOptions));
}

[[noreturn]] void CaptureFrameService::failBeforeFrame(
    const domain::FrameId& frameId,
    CaptureFrameStage stage,
    std::string message,
    std::optional<domain::CameraError> cameraError,
    bool retryAllowed)
{
    pauseCaptureRun();
    recoverableUnstartedFrames_.insert(frameId);
    if (retryAllowed) {
        retryableUnstartedFrames_.insert(frameId);
    }

    LogEvent event;
    event.severity = LogSeverity::Error;
    event.event = QStringLiteral("capture.frame.failed_before_start");
    event.message = QString::fromStdString(message);
    event.sessionId = session_.id();
    event.captureRunId = captureRun_.id();
    event.frameId = frameId;
    event.operation = captureFrameStageName(stage);
    event.errorCode = cameraError
        ? std::optional<QString>{cameraErrorCodeName(cameraError->code)}
        : std::nullopt;
    event.technicalDetails = cameraError
        ? std::optional<QString>{QString::fromStdString(cameraError->operation)}
        : std::nullopt;
    event.captureSettings = captureRun_.settings();
    loggingService_.log(event);

    std::vector<CaptureRecoveryDecision> recoveryOptions{
        CaptureRecoveryDecision::Pause,
        CaptureRecoveryDecision::Abort
    };
    if (retryAllowed) {
        recoveryOptions.insert(recoveryOptions.begin(), CaptureRecoveryDecision::Retry);
    }
    throw CaptureFrameError(
        stage,
        frameId,
        std::move(message),
        std::move(cameraError),
        std::move(recoveryOptions));
}

void CaptureFrameService::pauseCaptureRun()
{
    if (captureRun_.state() == CaptureRunState::Running
        || captureRun_.state() == CaptureRunState::StopRequested) {
        captureRunRepository_.updateState(captureRun_.id(), CaptureRunState::Paused);
        captureRun_.pause();
    }
}

} // namespace application
