#pragma once

#include "domain/repositories/capture_run_repository.h"
#include "domain/repositories/frame_repository.h"
#include "domain/repositories/session_repository.h"
#include "infrastructure/persistence/sqlite_database.h"

namespace infrastructure {

class SqliteSessionRepository final : public domain::SessionRepository
{
public:
    explicit SqliteSessionRepository(SqliteDatabase& database);

    void create(const domain::Target& target, const domain::Session& session) override;
    std::optional<domain::SessionRecord> findById(const domain::SessionId& id) const override;
    std::optional<domain::Target> findTargetBySessionId(const domain::SessionId& id) const override;
    std::vector<domain::SessionRecord> findAll() const override;
    std::vector<domain::SessionRecord> findByTargetId(const domain::TargetId& targetId) const override;
    void deleteSession(const domain::SessionId& id) override;
    void updateStage(
        const domain::SessionId& id,
        domain::SessionStage stage,
        domain::Timestamp updatedAt) override;
    void open(const domain::SessionId& id) override;
    void close(const domain::SessionId& id) override;

private:
    SqliteDatabase& database_;
};

class SqliteCaptureRunRepository final : public domain::CaptureRunRepository
{
public:
    explicit SqliteCaptureRunRepository(SqliteDatabase& database);

    void create(const domain::CaptureRun& captureRun) override;
    void createWithFrames(
        const domain::CaptureRun& captureRun,
        const std::vector<domain::Frame>& frames) override;
    std::optional<domain::CaptureRun> findById(const domain::CaptureRunId& id) const override;
    std::vector<domain::CaptureRun> findBySessionId(const domain::SessionId& sessionId) const override;
    void updateState(const domain::CaptureRunId& id, domain::CaptureRunState desiredState) override;

private:
    SqliteDatabase& database_;
};

class SqliteFrameRepository final : public domain::FrameRepository
{
public:
    explicit SqliteFrameRepository(SqliteDatabase& database);

    void create(const domain::Frame& frame) override;
    std::optional<domain::Frame> findById(const domain::FrameId& id) const override;
    std::vector<domain::Frame> findByCaptureRunId(
        const domain::CaptureRunId& captureRunId) const override;
    void update(const domain::Frame& frame) override;

private:
    SqliteDatabase& database_;
};

} // namespace infrastructure
