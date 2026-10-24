#include "infrastructure/recovery/session_file_inspector.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <stdexcept>

namespace infrastructure {

std::vector<InspectedSessionFile> LocalSessionFileInspector::inspect(
    const QString& sessionDirectory) const
{
    if (sessionDirectory.isEmpty() || !QDir::isAbsolutePath(sessionDirectory)) {
        throw std::invalid_argument("Session recovery directory must be an absolute path");
    }

    const QFileInfo sessionInfo(sessionDirectory);
    if (!sessionInfo.exists()) {
        return {};
    }
    if (!sessionInfo.isDir() || !sessionInfo.isReadable()) {
        throw std::runtime_error(
            QStringLiteral("Session recovery directory is not readable: %1")
                .arg(sessionDirectory)
                .toStdString());
    }

    const QString nightsDirectory = QDir(sessionDirectory).filePath(QStringLiteral("nights"));
    const QFileInfo nightsInfo(nightsDirectory);
    if (!nightsInfo.exists()) {
        return {};
    }
    if (!nightsInfo.isDir() || !nightsInfo.isReadable()) {
        throw std::runtime_error(
            QStringLiteral("Session images directory is not readable: %1")
                .arg(nightsDirectory)
                .toStdString());
    }

    std::vector<InspectedSessionFile> files;
    QDirIterator iterator(
        nightsDirectory,
        QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info = iterator.fileInfo();
        files.push_back({
            info.absoluteFilePath(),
            static_cast<std::uint64_t>(info.size()),
            info.fileName().startsWith(QStringLiteral(".astrocapture-"))});
    }
    return files;
}

} // namespace infrastructure
