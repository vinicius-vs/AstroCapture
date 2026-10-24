#pragma once

#include "domain/capture_entities.h"

#include <optional>
#include <vector>

namespace domain {

struct SessionRecord
{
    Session session;
    bool isOpen;
};

class SessionRepository
{
public:
    virtual ~SessionRepository() = default;

    virtual void create(const Target& target, const Session& session) = 0;
    virtual std::optional<SessionRecord> findById(const SessionId& id) const = 0;
    virtual std::vector<SessionRecord> findByTargetId(const TargetId& targetId) const = 0;
    virtual void updateStage(const SessionId& id, SessionStage stage, Timestamp updatedAt) = 0;
    virtual void open(const SessionId& id) = 0;
    virtual void close(const SessionId& id) = 0;
};

} // namespace domain
