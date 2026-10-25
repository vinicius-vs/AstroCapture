#include "application/capture_configuration_service.h"

#include <QDate>
#include <QUuid>

#include <chrono>
#include <stdexcept>
#include <utility>

namespace application {
namespace {

template<typename T>
T valueOrThrow(domain::CameraResult<T> result, const char* operation)
{
    if (auto* value = std::get_if<T>(&result)) {
        return *value;
    }
    const auto& error = std::get<domain::CameraError>(result);
    throw std::runtime_error(
        QStringLiteral("%1: %2")
            .arg(QString::fromLatin1(operation), QString::fromStdString(error.message))
            .toStdString());
}

domain::CaptureRunId newIdentifier()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

std::chrono::year_month_day today()
{
    const QDate date = QDate::currentDate();
    return std::chrono::year{date.year()}
        / static_cast<unsigned int>(date.month())
        / static_cast<unsigned int>(date.day());
}

} // namespace

CaptureConfigurationService::CaptureConfigurationService(
    domain::SessionRepository& sessionRepository,
    domain::CaptureRunRepository& captureRunRepository,
    domain::ICamera& camera)
    : sessionRepository_(sessionRepository)
    , captureRunRepository_(captureRunRepository)
    , camera_(camera)
{
}

void CaptureConfigurationService::connectCamera()
{
    if (cameraConnected_) {
        return;
    }
    const auto result = camera_.connect();
    if (const auto* error = std::get_if<domain::CameraError>(&result)) {
        throw std::runtime_error(
            QStringLiteral("Não foi possível conectar ao simulador: %1")
                .arg(QString::fromStdString(error->message))
                .toStdString());
    }
    cameraConnected_ = true;
}

domain::CameraCapabilities CaptureConfigurationService::cameraCapabilities()
{
    connectCamera();
    return valueOrThrow(camera_.capabilities(), "Não foi possível consultar os recursos da câmera");
}

domain::CameraSettings CaptureConfigurationService::currentCameraSettings()
{
    connectCamera();
    return valueOrThrow(camera_.settings(), "Não foi possível consultar as configurações da câmera");
}

domain::CaptureRun CaptureConfigurationService::configureSequence(
    const domain::SessionId& sessionId,
    domain::CaptureSettings settings)
{
    const auto session = sessionRepository_.findById(sessionId);
    if (!session) {
        throw std::invalid_argument("A sessão selecionada não existe mais.");
    }

    connectCamera();
    const auto capabilities =
        valueOrThrow(camera_.capabilities(), "Não foi possível consultar os recursos da câmera");
    const auto currentSettings =
        valueOrThrow(camera_.settings(), "Não foi possível consultar as configurações da câmera");
    settings.validate(capabilities, currentSettings);

    const domain::CameraSettings requestedSettings{settings.exposure, settings.iso};
    const auto appliedSettings = camera_.applySettings(requestedSettings);
    if (const auto* error = std::get_if<domain::CameraError>(&appliedSettings)) {
        throw std::invalid_argument(
            QStringLiteral("A câmera rejeitou as configurações: %1")
                .arg(QString::fromStdString(error->message))
                .toStdString());
    }

    domain::CaptureRun captureRun(
        newIdentifier(),
        sessionId,
        today(),
        settings);

    std::vector<domain::Frame> frames;
    if (settings.plannedFrameCount) {
        frames.reserve(*settings.plannedFrameCount);
        for (std::uint32_t index = 0; index < *settings.plannedFrameCount; ++index) {
            frames.emplace_back(
                newIdentifier(),
                captureRun.id(),
                settings.frameType,
                settings.exposure,
                settings.iso);
        }
    }
    captureRunRepository_.createWithFrames(captureRun, frames);
    return captureRun;
}

} // namespace application
