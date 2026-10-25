#pragma once

#include "domain/repositories/session_repository.h"
#include "infrastructure/storage/storage_layout.h"

#include <QString>

#include <vector>

namespace application {

class SessionService final
{
public:
    SessionService(
        domain::SessionRepository& sessionRepository,
        const infrastructure::StorageLayout& storageLayout);

    std::vector<domain::SessionRecord> listSessions() const;
    domain::SessionRecord createSession(
        const QString& targetName,
        const QString& sessionName,
        domain::SessionStage stage);
    domain::SessionRecord openSession(const domain::SessionId& sessionId);
    domain::Target targetForSession(const domain::SessionId& sessionId) const;
    QString sessionDirectory(const domain::SessionRecord& session) const;
    void deleteSession(const domain::SessionId& sessionId);

private:
    domain::SessionRepository& sessionRepository_;
    const infrastructure::StorageLayout& storageLayout_;
};

} // namespace application
