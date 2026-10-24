#include "infrastructure/logging/json_lines_logging_service.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <stdexcept>
#include <utility>

namespace infrastructure {
namespace {

using namespace domain;

QString severityName(LogSeverity severity)
{
    switch (severity) {
    case LogSeverity::Info:
        return QStringLiteral("INFO");
    case LogSeverity::Warning:
        return QStringLiteral("WARNING");
    case LogSeverity::Error:
        return QStringLiteral("ERROR");
    }
    throw std::invalid_argument("Invalid log severity");
}

QString timestampToString(Timestamp timestamp)
{
    const auto elapsed = std::chrono::floor<std::chrono::microseconds>(timestamp.time_since_epoch());
    const auto seconds = std::chrono::floor<std::chrono::seconds>(elapsed);
    const auto fractionalMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        elapsed - seconds);
    const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(seconds.count()).toUTC();
    if (!dateTime.isValid()) {
        throw std::invalid_argument("Log timestamp is outside the supported UTC date range");
    }
    return dateTime.toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss"))
        + QLatin1Char('.')
        + QStringLiteral("%1").arg(fractionalMicroseconds.count(), 6, 10, QLatin1Char('0'))
        + QLatin1Char('Z');
}

void insertOptional(QJsonObject& object, const QString& key, const std::optional<QString>& value)
{
    if (value) {
        object.insert(key, *value);
    }
}

QString captureModeName(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Finite:
        return QStringLiteral("Finite");
    case CaptureMode::Continuous:
        return QStringLiteral("Continuous");
    }
    throw std::invalid_argument("Invalid capture mode");
}

QString frameTypeName(FrameType type)
{
    switch (type) {
    case FrameType::Light:
        return QStringLiteral("Light");
    case FrameType::Dark:
        return QStringLiteral("Dark");
    case FrameType::Flat:
        return QStringLiteral("Flat");
    case FrameType::Bias:
        return QStringLiteral("Bias");
    case FrameType::DarkFlat:
        return QStringLiteral("DarkFlat");
    }
    throw std::invalid_argument("Invalid frame type");
}

QString safeRelativePath(const QString& path)
{
    if (path.isEmpty() || QDir::isAbsolutePath(path)) {
        throw std::invalid_argument("Log file path must be relative to the application data directory");
    }

    const QString normalizedSeparators = QDir::fromNativeSeparators(path);
    const QStringList components = normalizedSeparators.split(QLatin1Char('/'));
    if (components.contains(QStringLiteral(".."))) {
        throw std::invalid_argument("Log file path must not escape the application data directory");
    }
    return QDir::cleanPath(normalizedSeparators);
}

[[noreturn]] void throwFileError(const QString& operation, const QFile& file)
{
    throw std::runtime_error(
        QStringLiteral("%1: %2").arg(operation, file.errorString()).toStdString());
}

} // namespace

JsonLinesLoggingService::JsonLinesLoggingService(QString applicationDataDirectory)
{
    if (applicationDataDirectory.isEmpty()) {
        throw std::invalid_argument("Application data directory must not be empty");
    }

    logDirectory_ = QDir(applicationDataDirectory).filePath(QStringLiteral("logs"));
    activeLogPath_ = QDir(logDirectory_).filePath(QStringLiteral("astrocapture.jsonl"));
}

void JsonLinesLoggingService::log(const domain::LogEvent& event)
{
    if (event.event.trimmed().isEmpty()) {
        throw std::invalid_argument("Log event name must not be empty");
    }
    if (event.message.trimmed().isEmpty()) {
        throw std::invalid_argument("Log message must not be empty");
    }

    const QByteArray record = serialize(event);
    if (static_cast<std::uint64_t>(record.size()) > maximumFileSizeBytes) {
        throw std::invalid_argument("Log event exceeds the maximum log file size");
    }

    QMutexLocker lock(&mutex_);
    if (!QDir().mkpath(logDirectory_)) {
        throw std::runtime_error(
            QStringLiteral("Creating log directory failed: %1").arg(logDirectory_).toStdString());
    }
    rotateIfNeeded(record.size());

    QFile file(activeLogPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        throwFileError(QStringLiteral("Opening log file failed"), file);
    }
    const qint64 originalSize = file.size();
    qint64 written = 0;
    while (written < record.size()) {
        const qint64 writeResult = file.write(record.constData() + written, record.size() - written);
        if (writeResult <= 0) {
            const QString writeError = file.errorString();
            if (!file.resize(originalSize) || !file.flush()) {
                throw std::runtime_error(
                    QStringLiteral("Writing log event failed: %1; partial record rollback failed: %2")
                        .arg(writeError, file.errorString())
                        .toStdString());
            }
            throw std::runtime_error(
                QStringLiteral("Writing log event failed: %1").arg(writeError).toStdString());
        }
        written += writeResult;
    }
    if (!file.flush()) {
        throwFileError(QStringLiteral("Flushing log event failed"), file);
    }
}

QString JsonLinesLoggingService::activeLogPath() const
{
    return activeLogPath_;
}

QByteArray JsonLinesLoggingService::serialize(const domain::LogEvent& event) const
{
    QJsonObject object;
    object.insert(QStringLiteral("timestamp"), timestampToString(event.timestamp));
    object.insert(QStringLiteral("severity"), severityName(event.severity));
    object.insert(QStringLiteral("event"), event.event);
    object.insert(QStringLiteral("message"), event.message);
    if (event.sessionId) {
        object.insert(QStringLiteral("session_id"), QString::fromUtf8(*event.sessionId));
    }
    if (event.captureRunId) {
        object.insert(QStringLiteral("capture_run_id"), QString::fromUtf8(*event.captureRunId));
    }
    if (event.frameId) {
        object.insert(QStringLiteral("frame_id"), QString::fromUtf8(*event.frameId));
    }
    insertOptional(object, QStringLiteral("operation"), event.operation);
    insertOptional(object, QStringLiteral("previous_state"), event.previousState);
    insertOptional(object, QStringLiteral("state"), event.state);
    insertOptional(object, QStringLiteral("error_code"), event.errorCode);
    insertOptional(object, QStringLiteral("technical_details"), event.technicalDetails);
    if (event.relativeFilePath) {
        object.insert(QStringLiteral("file_path"), safeRelativePath(*event.relativeFilePath));
    }
    if (event.captureSettings) {
        event.captureSettings->validate();
        QJsonObject settings;
        settings.insert(QStringLiteral("frame_type"), frameTypeName(event.captureSettings->frameType));
        settings.insert(
            QStringLiteral("exposure_microseconds"),
            static_cast<qint64>(event.captureSettings->exposure.count()));
        if (event.captureSettings->iso) {
            settings.insert(
                QStringLiteral("iso"),
                static_cast<qint64>(*event.captureSettings->iso));
        }
        settings.insert(QStringLiteral("mode"), captureModeName(event.captureSettings->mode));
        if (event.captureSettings->plannedFrameCount) {
            settings.insert(
                QStringLiteral("planned_frame_count"),
                static_cast<qint64>(*event.captureSettings->plannedFrameCount));
        }
        object.insert(QStringLiteral("capture_settings"), settings);
    }

    QByteArray record = QJsonDocument(object).toJson(QJsonDocument::Compact);
    record.append('\n');
    return record;
}

void JsonLinesLoggingService::rotateIfNeeded(qsizetype recordSize)
{
    const QFileInfo activeFile(activeLogPath_);
    if (activeFile.exists()
        && static_cast<std::uint64_t>(activeFile.size()) + static_cast<std::uint64_t>(recordSize)
            > maximumFileSizeBytes) {
        rotate();
    }
}

void JsonLinesLoggingService::rotate()
{
    const QString oldestPath = QStringLiteral("%1.%2")
                                   .arg(activeLogPath_)
                                   .arg(retainedFileCount);
    if (QFileInfo::exists(oldestPath) && !QFile::remove(oldestPath)) {
        throw std::runtime_error(
            QStringLiteral("Removing oldest rotated log failed: %1").arg(oldestPath).toStdString());
    }

    for (std::size_t index = retainedFileCount; index > 1; --index) {
        const QString source = QStringLiteral("%1.%2").arg(activeLogPath_).arg(index - 1);
        const QString destination = QStringLiteral("%1.%2").arg(activeLogPath_).arg(index);
        if (QFileInfo::exists(source) && !QFile::rename(source, destination)) {
            throw std::runtime_error(
                QStringLiteral("Rotating log file failed: %1").arg(source).toStdString());
        }
    }

    const QString firstBackup = QStringLiteral("%1.1").arg(activeLogPath_);
    if (QFileInfo::exists(activeLogPath_) && !QFile::rename(activeLogPath_, firstBackup)) {
        throw std::runtime_error(
            QStringLiteral("Rotating active log file failed: %1").arg(activeLogPath_).toStdString());
    }
}

} // namespace infrastructure
