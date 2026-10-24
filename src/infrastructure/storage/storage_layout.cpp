#include "infrastructure/storage/storage_layout.h"

#include <QChar>
#include <QDate>
#include <QDir>
#include <QFileInfo>

#include <array>
#include <limits>
#include <stdexcept>

namespace infrastructure {
namespace {

QString encodePathBytes(const QByteArray& utf8)
{
    constexpr std::array<char, 16> hexDigits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    QByteArray encoded;
    encoded.reserve(utf8.size());

    for (const unsigned char byte : utf8) {
        const bool isAsciiLetterOrDigit = (byte >= 'a' && byte <= 'z')
            || (byte >= 'A' && byte <= 'Z')
            || (byte >= '0' && byte <= '9');
        if (isAsciiLetterOrDigit || byte == '-' || byte == '_') {
            encoded.append(static_cast<char>(byte));
        } else {
            encoded.append('%');
            encoded.append(hexDigits[byte >> 4]);
            encoded.append(hexDigits[byte & 0x0f]);
        }
    }
    return QString::fromLatin1(encoded);
}

QString encodeIdentifier(const std::string& identifier)
{
    if (identifier.empty()) {
        throw std::invalid_argument("Storage path identifier must not be empty");
    }

    return encodePathBytes(QByteArray::fromStdString(identifier));
}

QString targetLabel(const QString& name)
{
    QString result;
    bool pendingSeparator = false;
    const QVector<uint> codePoints = name.normalized(QString::NormalizationForm_C).toUcs4();

    for (uint codePoint : codePoints) {
        if (QChar::isLetterOrNumber(codePoint)) {
            if (pendingSeparator && !result.isEmpty()) {
                result.append(QLatin1Char('-'));
            }
            const char32_t character = static_cast<char32_t>(codePoint);
            result.append(QString::fromUcs4(&character, 1));
            pendingSeparator = false;
        } else if (!result.isEmpty()) {
            pendingSeparator = true;
        }
    }

    return result.isEmpty() ? QStringLiteral("target") : result;
}

QString frameTypeName(domain::FrameType frameType)
{
    switch (frameType) {
    case domain::FrameType::Light:
        return QStringLiteral("Light");
    case domain::FrameType::Dark:
        return QStringLiteral("Dark");
    case domain::FrameType::Flat:
        return QStringLiteral("Flat");
    case domain::FrameType::Bias:
        return QStringLiteral("Bias");
    case domain::FrameType::DarkFlat:
        return QStringLiteral("DarkFlat");
    }
    throw std::invalid_argument("Invalid frame type");
}

QString acquisitionDate(std::chrono::year_month_day date)
{
    if (!date.ok()) {
        throw std::invalid_argument("Invalid capture acquisition date");
    }

    const QDate value{
        static_cast<int>(date.year()),
        static_cast<int>(static_cast<unsigned int>(date.month())),
        static_cast<int>(static_cast<unsigned int>(date.day()))};
    return value.toString(Qt::ISODate);
}

QString pathForCandidate(
    const QString& directory,
    const QString& encodedFrameId,
    const QString& extension,
    unsigned int suffix)
{
    QString fileName = encodedFrameId;
    if (suffix != 0) {
        fileName += QStringLiteral("_%1").arg(suffix, 3, 10, QLatin1Char('0'));
    }
    if (!extension.isEmpty()) {
        fileName += QLatin1Char('.') + extension;
    }
    return QDir(directory).filePath(fileName);
}

} // namespace

StorageLayout::StorageLayout(QString imageRoot)
    : imageRoot_(QDir::cleanPath(std::move(imageRoot)))
{
    if (imageRoot_.isEmpty() || imageRoot_ == QStringLiteral(".")) {
        throw std::invalid_argument("Image root directory must not be empty");
    }
}

QString StorageLayout::targetDirectory(const domain::Target& target) const
{
    const QString id = encodeIdentifier(target.id());
    const QString label = targetLabel(QString::fromUtf8(target.name()));
    return QDir(imageRoot_).filePath(
        QStringLiteral("targets/%1_%2").arg(id, label));
}

QString StorageLayout::sessionDirectory(
    const domain::Target& target,
    const domain::Session& session) const
{
    if (session.targetId() != target.id()) {
        throw std::invalid_argument("Session does not belong to the specified target");
    }

    return QDir(targetDirectory(target)).filePath(
        QStringLiteral("sessions/%1").arg(encodeIdentifier(session.id())));
}

QString StorageLayout::frameDirectory(
    const domain::Target& target,
    const domain::Session& session,
    const domain::CaptureRun& captureRun) const
{
    if (captureRun.sessionId() != session.id()) {
        throw std::invalid_argument("Capture run does not belong to the specified session");
    }

    return QDir(sessionDirectory(target, session)).filePath(
        QStringLiteral("nights/%1/%2")
            .arg(acquisitionDate(captureRun.acquisitionDate()), frameTypeName(captureRun.settings().frameType)));
}

QString StorageLayout::frameFilePath(
    const domain::Target& target,
    const domain::Session& session,
    const domain::CaptureRun& captureRun,
    const domain::FrameId& frameId,
    const QString& originalFileName,
    unsigned int firstSuffix) const
{
    const QString encodedFrameId = encodeIdentifier(frameId);
    const QString extension = QFileInfo(originalFileName).suffix();
    const QString directory = frameDirectory(target, session, captureRun);

    for (unsigned int suffix = firstSuffix;
         suffix < std::numeric_limits<unsigned int>::max();
         ++suffix) {
        const QString candidate = pathForCandidate(directory, encodedFrameId, extension, suffix);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    throw std::runtime_error("No available filename for frame");
}

} // namespace infrastructure
