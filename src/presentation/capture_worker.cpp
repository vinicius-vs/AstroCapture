#include "presentation/capture_worker.h"

#include "application/capture_frame_service.h"
#include "application/capture_sequence_service.h"
#include "application/capture_statistics_service.h"
#include "domain/repositories/frame_repository.h"
#include "domain/repositories/session_repository.h"
#include "infrastructure/logging/json_lines_logging_service.h"
#include "infrastructure/persistence/sqlite_database.h"
#include "infrastructure/persistence/sqlite_repositories.h"
#include "infrastructure/storage/image_storage.h"
#include "infrastructure/storage/storage_layout.h"

#include <QDir>
#include <QDate>
#include <QDateTime>
#include <QElapsedTimer>

#include <array>
#include <chrono>
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

QString frameTypeText(domain::FrameType type)
{
    switch (type) {
    case domain::FrameType::Light:
        return QStringLiteral("Light");
    case domain::FrameType::Dark:
        return QStringLiteral("Dark");
    case domain::FrameType::Flat:
        return QStringLiteral("Flat");
    case domain::FrameType::Bias:
        return QStringLiteral("Bias");
    case domain::FrameType::DarkFlat:
        return QStringLiteral("Dark Flat");
    }
    throw std::invalid_argument("Invalid frame type");
}

QString formatExposure(std::uint64_t exposureMicroseconds)
{
    return QStringLiteral("%1 s").arg(
        static_cast<double>(exposureMicroseconds) / 1'000'000.0,
        0,
        'f',
        2);
}

QString formatStatistics(
    const QString& title,
    const application::CaptureStatistics& statistics)
{
    QStringList byType;
    const std::array<domain::FrameType, 5> types{
        domain::FrameType::Light,
        domain::FrameType::Dark,
        domain::FrameType::Flat,
        domain::FrameType::Bias,
        domain::FrameType::DarkFlat
    };
    for (std::size_t index = 0; index < types.size(); ++index) {
        byType.push_back(QStringLiteral("%1: %2")
                             .arg(frameTypeText(types[index]))
                             .arg(statistics.completedFramesByType[index]));
    }

    QString lastCapture = QStringLiteral("nenhuma");
    if (statistics.lastCapturedAt) {
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            statistics.lastCapturedAt->time_since_epoch()).count();
        lastCapture = QDateTime::fromMSecsSinceEpoch(milliseconds)
                          .toLocalTime()
                          .toString(QStringLiteral("dd/MM/yyyy HH:mm:ss"));
    }
    return QStringLiteral(
               "%1: %2 concluídos (%3) · %4 de exposição · %5 sequência(s) · %6 falha(s) · "
               "última captura: %7")
        .arg(title)
        .arg(statistics.completedFrameCount)
        .arg(byType.join(QStringLiteral(", ")))
        .arg(formatExposure(statistics.exposureMicroseconds))
        .arg(statistics.sequenceCount)
        .arg(statistics.failedFrameCount)
        .arg(lastCapture);
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

        application::CaptureStatisticsService statisticsService(
            captureRunRepository,
            frameRepository);
        const QDate acquisitionDate(
            static_cast<int>(captureRun_.acquisitionDate().year()),
            static_cast<int>(static_cast<unsigned int>(captureRun_.acquisitionDate().month())),
            static_cast<int>(static_cast<unsigned int>(captureRun_.acquisitionDate().day())));
        QElapsedTimer elapsedTimer;
        elapsedTimer.start();
        const auto emitStatistics = [this, &statisticsService, &acquisitionDate] {
            const auto snapshot = statisticsService.forSession(
                captureRun_.sessionId(),
                std::chrono::year{acquisitionDate.year()}
                    / static_cast<unsigned int>(acquisitionDate.month())
                    / static_cast<unsigned int>(acquisitionDate.day()));
            emit statisticsChanged(
                formatStatistics(QStringLiteral("Sessão"), snapshot.session)
                + QLatin1Char('\n')
                + formatStatistics(
                    QStringLiteral("Noite %1").arg(acquisitionDate.toString(
                        QStringLiteral("dd/MM/yyyy"))),
                    snapshot.night));
        };
        emitStatistics();

        const auto initialFrames = frameRepository.findByCaptureRunId(captureRun_.id());
        quint32 failedCount = 0;
        quint32 completedCount = 0;
        for (const domain::Frame& frame : initialFrames) {
            if (frame.state() == domain::FrameState::Failed) {
                ++failedCount;
            } else if (frame.state() == domain::FrameState::Completed) {
                ++completedCount;
            }
        }
        emit progressChanged(
            completedCount,
            captureRun_.settings().plannedFrameCount.value_or(0),
            failedCount,
            QStringLiteral("Validando sequência"),
            elapsedTimer.elapsed());
        const auto completedFrames = captureSequenceService.execute(
            [this, &failedCount, &elapsedTimer, &emitStatistics](
                const application::CaptureSequenceProgress& progress) {
                QString details;
                if (progress.currentFrame) {
                    details = QStringLiteral("Estado: %1 · quadro %2")
                                  .arg(
                                      frameStateText(progress.currentFrame->state),
                                      QString::fromStdString(
                                          progress.currentFrame->frameId));
                    if (progress.currentFrame->transfer) {
                        const auto& transfer = *progress.currentFrame->transfer;
                        details += QStringLiteral(" · %1 bytes")
                                       .arg(transfer.bytesTransferred);
                        if (transfer.totalBytes) {
                            details += QStringLiteral(" de %1").arg(*transfer.totalBytes);
                        }
                    }
                    if (progress.currentFrame->state == domain::FrameState::Failed) {
                        ++failedCount;
                        emitStatistics();
                    } else if (progress.currentFrame->state == domain::FrameState::Completed) {
                        emitStatistics();
                    }
                }
                emit progressChanged(
                    progress.completedFrames,
                    progress.plannedFrameCount.value_or(0),
                    failedCount,
                    details,
                    elapsedTimer.elapsed());
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
