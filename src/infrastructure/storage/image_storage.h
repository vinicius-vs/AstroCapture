#pragma once

#include "domain/capture_entities.h"
#include "infrastructure/storage/storage_layout.h"

#include <QIODevice>
#include <QString>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace infrastructure {

enum class ImageStorageOperation
{
    CreateDirectory,
    CreateTemporaryFile,
    ReadSource,
    WriteTemporaryFile,
    ValidateTemporaryFile,
    PublishFile
};

class ImageStorageError final : public std::runtime_error
{
public:
    ImageStorageError(ImageStorageOperation operation, std::string message);

    ImageStorageOperation operation() const noexcept;

private:
    ImageStorageOperation operation_;
};

struct StoredImage
{
    QString filePath;
    std::uint64_t fileSize;
};

class ImageStorage final
{
public:
    explicit ImageStorage(StorageLayout layout);

    StoredImage store(
        const domain::Target& target,
        const domain::Session& session,
        const domain::CaptureRun& captureRun,
        const domain::Frame& frame,
        const QString& originalFileName,
        QIODevice& source,
        std::optional<std::uint64_t> expectedSize = std::nullopt) const;

private:
    StorageLayout layout_;
};

} // namespace infrastructure
