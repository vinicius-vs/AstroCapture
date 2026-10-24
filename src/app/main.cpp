#include "app/application_metadata.h"
#include "infrastructure/persistence/sqlite_database.h"
#include "presentation/main_window.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

#include <cstdlib>
#include <exception>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("AstroCapture"));

    try {
        const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (dataDirectory.isEmpty()) {
            throw std::runtime_error("Could not determine the application data directory");
        }

        infrastructure::SqliteDatabase database(
            QDir(dataDirectory).filePath(QStringLiteral("astrocapture.sqlite3")));
        database.open();

        MainWindow window;
        window.show();
        return application.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("AstroCapture"),
            QString::fromUtf8(error.what()));
        return EXIT_FAILURE;
    }
}
