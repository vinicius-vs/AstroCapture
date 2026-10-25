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
    std::optional<domain::CameraError> cameraError)
    : std::runtime_error(std::move(message))
    , stage_(stage)
    , frameId_(std::move(frameId))
    , cameraError_(std::move(cameraError))
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

CaptureFrameService::CaptureFrameService(
    domain::ICamera& camera,
    domain::FrameRepository& frameRepository,
    infrastructure::ImageStorage& imageStorage,
    domain::LoggingService& loggingService,
    const domain::Target& target,
    const domain::Session& session,
    const domain::CaptureRun& captureRun)
    : camera_(camera)
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
    CaptureFrameProgressCallback progress)
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

    const CameraResult<CameraCapabilities> capabilitiesResult = camera_.capabilities();
    if (const auto* cameraError = std::get_if<CameraError>(&capabilitiesResult)) {
        throw CaptureFrameError(
            CaptureFrameStage::Validate,
            frameId,
            cameraErrorMessage(*cameraError),
            *cameraError);
    }
    const CameraResult<CameraSettings> currentSettingsResult = camera_.settings();
    if (const auto* cameraError = std::get_if<CameraError>(&currentSettingsResult)) {
        throw CaptureFrameError(
            CaptureFrameStage::Validate,
            frameId,
            cameraErrorMessage(*cameraError),
            *cameraError);
    }
    try {
        captureRun_.settings().validate(
            std::get<CameraCapabilities>(capabilitiesResult),
            std::get<CameraSettings>(currentSettingsResult));
    } catch (const std::invalid_argument& error) {
        throw CaptureFrameError(
            CaptureFrameStage::Validate,
            frameId,
            error.what());
    }

    Frame frame(
        std::move(frameId),
        captureRun_.id(),
        captureRun_.settings().frameType,
        captureRun_.settings().exposure,
        captureRun_.settings().iso);
    frameRepository_.create(frame);
    reportState(frame, std::nullopt, progress);

    frame.startExposure(
        std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now()));
    frameRepository_.update(frame);
    reportState(frame, FrameState::Pending, progress);

    const CameraSettings requestedSettings{
        captureRun_.settings().exposure,
        captureRun_.settings().iso
    };
    const CameraOperationResult applyResult = camera_.applySettings(requestedSettings);
    if (const auto* cameraError = std::get_if<CameraError>(&applyResult)) {
        fail(
            frame,
            CaptureFrameStage::ApplySettings,
            cameraErrorMessage(*cameraError),
            progress,
            *cameraError);
    }

    const CameraResult<CameraCapture> captureResult = camera_.capture(
        CaptureRequest{requestedSettings});
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
    frameRepository_.update(frame);
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
    frameRepository_.update(frame);
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
    } catch (const infrastructure::ImageStorageError& error) {
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
            error.what());
    }
    reportState(frame, FrameState::Validating, progress);
    return frame;
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
    loggingService_.log(event);
    if (progress) {
        progress(CaptureFrameProgress{frame.id(), frame.state(), std::nullopt});
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
    frame.fail(message);
    frameRepository_.update(frame);

    LogEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.severity = LogSeverity::Error;
    event.event = QStringLiteral("capture.frame.failed");
    event.message = QString::fromStdString(message);
    event.sessionId = session_.id();
    event.captureRunId = captureRun_.id();
    event.frameId = frame.id();
    event.operation = QStringLiteral("captureFrame");
    event.previousState = frameStateName(previousState);
    event.state = QStringLiteral("Failed");
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
    throw CaptureFrameError(stage, frame.id(), std::move(message), std::move(cameraError));
}

} // namespace application
