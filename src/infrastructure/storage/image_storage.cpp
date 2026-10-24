#include "infrastructure/storage/image_storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#include <algorithm>
#include <array>
#include <cerrno>
#include <limits>
#include <system_error>
#include <utility>

#include <unistd.h>

namespace infrastructure {
namespace {

constexpr qint64 bufferSize = 64 * 1024;

std::string errorMessage(const QString& operation, const QString& detail)
{
    const QString effectiveDetail = detail.isEmpty()
        ? QStringLiteral("unknown device error")
        : detail;
    return QStringLiteral("%1: %2").arg(operation, effectiveDetail).toStdString();
}

QString systemError(int error)
{
    return QString::fromStdString(
        std::error_code(error, std::generic_category()).message());
}

} // namespace

ImageStorageError::ImageStorageError(
    ImageStorageOperation operation,
    std::string message)
    : std::runtime_error(std::move(message))
    , operation_(operation)
{
}

ImageStorageOperation ImageStorageError::operation() const noexcept
{
    return operation_;
}

ImageStorage::ImageStorage(StorageLayout layout)
    : layout_(std::move(layout))
{
}

StoredImage ImageStorage::store(
    const domain::Target& target,
    const domain::Session& session,
    const domain::CaptureRun& captureRun,
    const domain::Frame& frame,
    const QString& originalFileName,
    QIODevice& source,
    std::optional<std::uint64_t> expectedSize) const
{
    if (frame.state() != domain::FrameState::Validating) {
        throw std::logic_error("Frame must be validating before image storage");
    }
    if (frame.captureRunId() != captureRun.id()) {
        throw std::invalid_argument("Frame does not belong to the specified capture run");
    }
    if (frame.type() != captureRun.settings().frameType) {
        throw std::invalid_argument("Frame type does not match the capture run");
    }
    if (!source.isOpen() || !source.isReadable() || source.isTextModeEnabled()) {
        throw ImageStorageError(
            ImageStorageOperation::ReadSource,
            "Image source must be open, readable, and in binary mode");
    }

    const QString directory = QFileInfo(
        layout_.frameDirectory(target, session, captureRun)).absoluteFilePath();
    errno = 0;
    if (!QDir().mkpath(directory)) {
        const int error = errno;
        const QString detail = error == 0
            ? QStringLiteral("filesystem rejected directory creation")
            : systemError(error);
        throw ImageStorageError(
            ImageStorageOperation::CreateDirectory,
            errorMessage(
                QStringLiteral("Unable to create image directory %1").arg(directory),
                detail));
    }

    QTemporaryFile temporary(QDir(directory).filePath(QStringLiteral(".astrocapture-XXXXXX")));
    temporary.setAutoRemove(true);
    if (!temporary.open()) {
        throw ImageStorageError(
            ImageStorageOperation::CreateTemporaryFile,
            errorMessage(
                QStringLiteral("Unable to create temporary image file"),
                temporary.errorString()));
    }

    std::array<char, static_cast<std::size_t>(bufferSize)> buffer{};
    std::uint64_t bytesWritten = 0;
    while (true) {
        if (source.isSequential() && source.atEnd()) {
            if (!source.waitForReadyRead(-1)) {
                if (source.atEnd()) {
                    break;
                }
                throw ImageStorageError(
                    ImageStorageOperation::ReadSource,
                    errorMessage(
                        QStringLiteral("Unable to wait for image source data"),
                        source.errorString()));
            }
        } else if (!source.isSequential() && source.atEnd()) {
            break;
        }

        const qint64 bytesRead = source.read(buffer.data(), bufferSize);
        if (bytesRead < 0) {
            throw ImageStorageError(
                ImageStorageOperation::ReadSource,
                errorMessage(QStringLiteral("Unable to read image source"), source.errorString()));
        }
        if (bytesRead == 0) {
            if (source.isSequential()) {
                if (source.waitForReadyRead(-1)) {
                    continue;
                }
                if (source.atEnd()) {
                    break;
                }
            } else if (source.atEnd()) {
                break;
            }
            throw ImageStorageError(
                ImageStorageOperation::ReadSource,
                errorMessage(QStringLiteral("Image source ended before EOF"), source.errorString()));
        }

        const auto unsignedBytesRead = static_cast<std::uint64_t>(bytesRead);
        if (unsignedBytesRead > std::numeric_limits<std::uint64_t>::max() - bytesWritten) {
            throw ImageStorageError(
                ImageStorageOperation::WriteTemporaryFile,
                "Image size exceeds the supported range");
        }
        if (expectedSize
            && (bytesWritten > *expectedSize
                || unsignedBytesRead > *expectedSize - bytesWritten)) {
            throw ImageStorageError(
                ImageStorageOperation::ReadSource,
                "Image source is larger than the expected size");
        }

        qint64 offset = 0;
        while (offset < bytesRead) {
            const qint64 bytesStored = temporary.write(buffer.data() + offset, bytesRead - offset);
            if (bytesStored <= 0) {
                throw ImageStorageError(
                    ImageStorageOperation::WriteTemporaryFile,
                    errorMessage(
                        QStringLiteral("Unable to write temporary image file"),
                        temporary.errorString()));
            }
            offset += bytesStored;
        }
        bytesWritten += unsignedBytesRead;
    }

    if (bytesWritten == 0) {
        throw ImageStorageError(
            ImageStorageOperation::ValidateTemporaryFile,
            "Image source is empty");
    }
    if (expectedSize && bytesWritten != *expectedSize) {
        throw ImageStorageError(
            ImageStorageOperation::ValidateTemporaryFile,
            "Image size does not match the expected size");
    }
    if (!temporary.flush()) {
        throw ImageStorageError(
            ImageStorageOperation::WriteTemporaryFile,
            errorMessage(
                QStringLiteral("Unable to flush temporary image file"),
                temporary.errorString()));
    }

    const QString temporaryPath = temporary.fileName();
    temporary.close();

    QFile verification(temporaryPath);
    if (!verification.open(QIODevice::ReadOnly)
        || verification.size() <= 0
        || static_cast<std::uint64_t>(verification.size()) != bytesWritten
        || verification.read(1).isEmpty()) {
        throw ImageStorageError(
            ImageStorageOperation::ValidateTemporaryFile,
            errorMessage(
                QStringLiteral("Unable to validate temporary image file"),
                verification.errorString()));
    }
    verification.close();

    unsigned int firstSuffix = 0;
    while (true) {
        const QString candidate = QFileInfo(layout_.frameFilePath(
            target,
            session,
            captureRun,
            frame.id(),
            originalFileName,
            firstSuffix)).absoluteFilePath();
        const QByteArray sourcePathBytes = QFile::encodeName(temporaryPath);
        const QByteArray destinationPathBytes = QFile::encodeName(candidate);
        if (::link(sourcePathBytes.constData(), destinationPathBytes.constData()) == 0) {
            return {candidate, bytesWritten};
        }

        const int error = errno;
        if (error == EEXIST) {
            if (firstSuffix == std::numeric_limits<unsigned int>::max() - 1) {
                throw ImageStorageError(
                    ImageStorageOperation::PublishFile,
                    "No available filename for frame");
            }
            ++firstSuffix;
            continue;
        }
        throw ImageStorageError(
            ImageStorageOperation::PublishFile,
            errorMessage(
                QStringLiteral("Unable to publish image file"),
                systemError(error)));
    }
}

} // namespace infrastructure
