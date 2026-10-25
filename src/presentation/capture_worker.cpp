#include "presentation/capture_worker.h"

#include "application/capture_frame_service.h"
#include "application/capture_sequence_service.h"
#include "domain/repositories/frame_repository.h"
#include "domain/repositories/session_repository.h"
#include "infrastructure/logging/json_lines_logging_service.h"
#include "infrastructure/persistence/sqlite_database.h"
#include "infrastructure/persistence/sqlite_repositories.h"
#include "infrastructure/storage/image_storage.h"
#include "infrastructure/storage/storage_layout.h"

#include <QDir>

#include <exception>
#include <stdexcept>
#include <utility>

namespace presentation {
namespace {

QString frameStateText(domain::FrameState state)
{
    switch (state) {
    case domain::FrameState::Pending:
        return QStringLiteral("Aguardando");
    case domain::FrameState::Exposing:
        return QStringLiteral("Expondo");
    case domain::FrameState::Transferring:
        return QStringLiteral("Transferindo");
    case domain::FrameState::Validating:
        return QStringLiteral("Validando");
    case domain::FrameState::Completed:
        return QStringLiteral("Concluído");
    case domain::FrameState::Failed:
        return QStringLiteral("Falhou");
    }
    throw std::invalid_argument("Invalid frame state");
}

} // namespace

CaptureWorker::CaptureWorker(
    QString applicationDataDirectory,
    domain::ICamera& camera,
    domain::CaptureRun captureRun,
    std::shared_ptr<std::atomic_bool> stopRequested)
    : applicationDataDirectory_(std::move(applicationDataDirectory))
    , camera_(camera)
    , captureRun_(std::move(captureRun))
    , stopRequested_(std::move(stopRequested))
{
    if (!stopRequested_) {
        throw std::invalid_argument("Capture worker requires a stop token");
    }
}

void CaptureWorker::execute()
{
    try {
        infrastructure::SqliteDatabase database(
            QDir(applicationDataDirectory_).filePath(QStringLiteral("astrocapture.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository sessionRepository(database);
        infrastructure::SqliteCaptureRunRepository captureRunRepository(database);
        infrastructure::SqliteFrameRepository frameRepository(database);

        const auto sessionRecord = sessionRepository.findById(captureRun_.sessionId());
        if (!sessionRecord) {
            throw std::runtime_error("Capture session no longer exists");
        }
        const auto target = sessionRepository.findTargetBySessionId(captureRun_.sessionId());
        if (!target) {
            throw std::runtime_error("Capture target no longer exists");
        }

        infrastructure::StorageLayout storageLayout(
            QDir(applicationDataDirectory_).filePath(QStringLiteral("images")));
        infrastructure::ImageStorage imageStorage(storageLayout);
        infrastructure::JsonLinesLoggingService loggingService(applicationDataDirectory_);
        application::CaptureFrameService captureFrameService(
            camera_,
            captureRunRepository,
            frameRepository,
            imageStorage,
            loggingService,
            *target,
            sessionRecord->session,
            captureRun_);
        application::CaptureSequenceService captureSequenceService(
            captureFrameService,
            captureRunRepository,
            frameRepository,
            captureRun_,
            stopRequested_);

        const auto completedFrames = captureSequenceService.execute(
            [this](const application::CaptureSequenceProgress& progress) {
                QString details = QStringLiteral("Quadros concluídos: %1")
                                      .arg(progress.completedFrames);
                if (progress.currentFrame) {
                    details += QStringLiteral(" · %1")
                                   .arg(frameStateText(progress.currentFrame->state));
                    if (progress.currentFrame->transfer) {
                        const auto& transfer = *progress.currentFrame->transfer;
                        details += QStringLiteral(" · %1 bytes")
                                       .arg(transfer.bytesTransferred);
                        if (transfer.totalBytes) {
                            details += QStringLiteral(" de %1").arg(*transfer.totalBytes);
                        }
                    }
                }
                emit progressChanged(
                    progress.completedFrames,
                    progress.plannedFrameCount.value_or(0),
                    details);
            });
        emit captureCompleted(
            static_cast<quint32>(completedFrames.size()),
            captureRun_.state() == domain::CaptureRunState::Paused);
    } catch (const std::exception& error) {
        emit captureFailed(QString::fromUtf8(error.what()));
    } catch (...) {
        emit captureFailed(QStringLiteral("Ocorreu um erro inesperado durante a captura."));
    }
    emit finished();
}

} // namespace presentation
