#include "application/session_service.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include <chrono>
#include <stdexcept>
#include <utility>

namespace application {
namespace {

domain::SessionId newIdentifier()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

} // namespace

SessionService::SessionService(
    domain::SessionRepository& sessionRepository,
    const infrastructure::StorageLayout& storageLayout)
    : sessionRepository_(sessionRepository)
    , storageLayout_(storageLayout)
{
}

std::vector<domain::SessionRecord> SessionService::listSessions() const
{
    return sessionRepository_.findAll();
}

domain::SessionRecord SessionService::createSession(
    const QString& targetName,
    const QString& sessionName,
    domain::SessionStage stage)
{
    const QString normalizedTargetName = targetName.trimmed();
    const QString normalizedSessionName = sessionName.trimmed();
    if (normalizedTargetName.isEmpty()) {
        throw std::invalid_argument("Target name must not be empty");
    }
    if (normalizedSessionName.isEmpty()) {
        throw std::invalid_argument("Session name must not be empty");
    }

    const auto now = std::chrono::system_clock::now();
    const domain::Target target(
        newIdentifier(),
        normalizedTargetName.toUtf8().toStdString());
    const domain::Session session(
        newIdentifier(),
        target.id(),
        normalizedSessionName.toUtf8().toStdString(),
        stage,
        now,
        now);

    sessionRepository_.create(target, session);

    const QString sessionDirectory = storageLayout_.sessionDirectory(target, session);
    if (!QDir().mkpath(sessionDirectory)) {
        throw std::runtime_error(
            QStringLiteral("Could not create session directory: %1")
                .arg(sessionDirectory)
                .toStdString());
    }

    const auto record = sessionRepository_.findById(session.id());
    if (!record) {
        throw std::runtime_error("Created session could not be loaded");
    }
    return *record;
}

domain::SessionRecord SessionService::openSession(const domain::SessionId& sessionId)
{
    sessionRepository_.open(sessionId);
    const auto record = sessionRepository_.findById(sessionId);
    if (!record) {
        throw std::runtime_error("Opened session could not be loaded");
    }
    return *record;
}

domain::Target SessionService::targetForSession(const domain::SessionId& sessionId) const
{
    const auto target = sessionRepository_.findTargetBySessionId(sessionId);
    if (!target) {
        throw std::runtime_error("Session target could not be loaded");
    }
    return *target;
}

QString SessionService::sessionDirectory(const domain::SessionRecord& session) const
{
    const domain::Target target = targetForSession(session.session.id());
    return storageLayout_.sessionDirectory(target, session.session);
}

void SessionService::deleteSession(const domain::SessionId& sessionId)
{
    const auto record = sessionRepository_.findById(sessionId);
    if (!record) {
        throw std::runtime_error("Session does not exist");
    }
    const domain::Target target = targetForSession(sessionId);
    const QString sessionDirectory = storageLayout_.sessionDirectory(target, record->session);
    const QFileInfo directoryInfo(sessionDirectory);
    QString stagedDirectory;

    if (directoryInfo.exists()) {
        const QString parentDirectory = directoryInfo.absolutePath();
        const QString stagingName = directoryInfo.fileName()
            + QStringLiteral(".deleting-%1")
                  .arg(QString::fromStdString(newIdentifier()));
        stagedDirectory = QDir(parentDirectory).filePath(stagingName);
        if (!QDir(parentDirectory).rename(directoryInfo.fileName(), stagingName)) {
            throw std::runtime_error(
                QStringLiteral("Could not stage session files for deletion: %1")
                    .arg(sessionDirectory)
                    .toStdString());
        }
    }

    try {
        sessionRepository_.deleteSession(sessionId);
    } catch (const std::exception& error) {
        if (!stagedDirectory.isEmpty()
            && !QDir(QFileInfo(stagedDirectory).absolutePath())
                    .rename(QFileInfo(stagedDirectory).fileName(), directoryInfo.fileName())) {
            throw std::runtime_error(
                QStringLiteral("%1; session files could not be restored from %2")
                    .arg(QString::fromUtf8(error.what()), stagedDirectory)
                    .toStdString());
        }
        throw;
    }

    if (!stagedDirectory.isEmpty() && !QDir(stagedDirectory).removeRecursively()) {
        throw std::runtime_error(
            QStringLiteral("Session metadata was deleted, but files remain at: %1")
                .arg(stagedDirectory)
                .toStdString());
    }
}

} // namespace application
