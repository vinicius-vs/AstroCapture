#include "app/application_metadata.h"
#include "application/capture_configuration_service.h"
#include "application/session_service.h"
#include "domain/logging/log_event.h"
#include "infrastructure/logging/json_lines_logging_service.h"
#include "infrastructure/camera/simulated_camera.h"
#include "infrastructure/persistence/sqlite_database.h"
#include "infrastructure/persistence/sqlite_repositories.h"
#include "infrastructure/storage/storage_layout.h"
#include "presentation/main_window.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

#include <cstdlib>
#include <exception>
#include <memory>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("AstroCapture"));
    std::unique_ptr<infrastructure::JsonLinesLoggingService> loggingService;
    QString dataDirectory;

    try {
        dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (dataDirectory.isEmpty()) {
            throw std::runtime_error("Could not determine the application data directory");
        }

        loggingService = std::make_unique<infrastructure::JsonLinesLoggingService>(dataDirectory);
        domain::LogEvent startupEvent;
        startupEvent.severity = domain::LogSeverity::Info;
        startupEvent.event = QStringLiteral("application.started");
        startupEvent.message = QStringLiteral("AstroCapture started");
        loggingService->log(startupEvent);

        infrastructure::SqliteDatabase database(
            QDir(dataDirectory).filePath(QStringLiteral("astrocapture.sqlite3")));
        database.open();

        infrastructure::SqliteSessionRepository sessionRepository(database);
        infrastructure::SqliteCaptureRunRepository captureRunRepository(database);
        infrastructure::SimulatedCamera simulatedCamera;
        infrastructure::StorageLayout storageLayout(
            QDir(dataDirectory).filePath(QStringLiteral("images")));
        application::SessionService sessionService(sessionRepository, storageLayout);
        application::CaptureConfigurationService captureConfigurationService(
            sessionRepository,
            captureRunRepository,
            simulatedCamera);
        MainWindow window(sessionService, captureConfigurationService);
        window.show();
        return application.exec();
    } catch (const std::exception& error) {
        QString errorMessage = QString::fromUtf8(error.what());
        if (loggingService) {
            domain::LogEvent failureEvent;
            failureEvent.severity = domain::LogSeverity::Error;
            failureEvent.event = QStringLiteral("application.startup.failed");
            failureEvent.message = QStringLiteral("Application startup failed");
            failureEvent.errorCode = QStringLiteral("application.startup");
            failureEvent.technicalDetails = errorMessage;
            if (!dataDirectory.isEmpty()) {
                failureEvent.technicalDetails->replace(
                    QDir::cleanPath(dataDirectory),
                    QStringLiteral("<AppDataLocation>"));
                failureEvent.relativeFilePath = QStringLiteral("astrocapture.sqlite3");
            }
            try {
                loggingService->log(failureEvent);
            } catch (const std::exception& loggingError) {
                errorMessage += QStringLiteral("\nLogging failure: %1").arg(QString::fromUtf8(loggingError.what()));
            }
        }
        QMessageBox::critical(
            nullptr,
            QStringLiteral("AstroCapture"),
            errorMessage);
        return EXIT_FAILURE;
    }
}
