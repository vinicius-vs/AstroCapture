#include "domain/repositories/repository_errors.h"
#include "infrastructure/persistence/sqlite_database.h"
#include "infrastructure/persistence/sqlite_repositories.h"

#include <QtTest>
#include <QSqlError>
#include <QSqlQuery>

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;
using namespace domain;
using namespace infrastructure;

class SqliteRepositoriesTest final : public QObject
{
    Q_OBJECT

private:
    static Session makeSession(
        const Target& target,
        const SessionId& id,
        Timestamp createdAt = Timestamp{std::chrono::microseconds{1'000'000}})
    {
        return Session(
            id,
            target.id(),
            "Observation",
            SessionStage::Capture,
            createdAt,
            createdAt);
    }

    static int scalarInt(const QSqlDatabase& connection, const QString& sql)
    {
        QSqlQuery query(connection);
        if (!query.exec(sql) || !query.next()) {
            throw std::runtime_error(query.lastError().text().toStdString());
        }
        return query.value(0).toInt();
    }

private slots:
    void createsSessionsAndPersistsTheirOperationalState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = QDir(directory.path()).filePath(QStringLiteral("repository.sqlite3"));
        const Timestamp createdAt{std::chrono::microseconds{1'234'567}};
        const Target target("target-1", "M42");
        const Session session = makeSession(target, "session-1", createdAt);

        {
            SqliteDatabase database(databasePath);
            database.open();
            SqliteSessionRepository repository(database);
            repository.create(target, session);

            const auto record = repository.findById(session.id());
            QVERIFY(record.has_value());
            QCOMPARE(record->session.id(), session.id());
            QCOMPARE(record->session.targetId(), target.id());
            QCOMPARE(record->session.name(), session.name());
            QCOMPARE(record->session.stage(), SessionStage::Capture);
            QCOMPARE(record->session.createdAt(), createdAt);
            QCOMPARE(record->session.updatedAt(), createdAt);
            QVERIFY(record->isOpen);
            const Session earlierSession = makeSession(target, "session-0", createdAt - 1s);
            repository.create(target, earlierSession);
            const auto sessions = repository.findByTargetId(target.id());
            QCOMPARE(sessions.size(), std::size_t{2});
            QCOMPARE(sessions.front().session.id(), SessionId("session-0"));

            repository.close(session.id());
            repository.close(session.id());
            QVERIFY(!repository.findById(session.id())->isOpen);
            repository.open(session.id());
            repository.open(session.id());
            QVERIFY(repository.findById(session.id())->isOpen);

            repository.updateStage(
                session.id(),
                SessionStage::Stacking,
                createdAt + 1500us);
            const auto updated = repository.findById(session.id());
            QCOMPARE(updated->session.stage(), SessionStage::Stacking);
            QCOMPARE(updated->session.updatedAt(), createdAt + 1500us);
        }

        SqliteDatabase reopenedDatabase(databasePath);
        reopenedDatabase.open();
        SqliteSessionRepository reopenedRepository(reopenedDatabase);
        const auto restored = reopenedRepository.findById(session.id());
        QVERIFY(restored.has_value());
        QVERIFY(restored->isOpen);
        QCOMPARE(restored->session.stage(), SessionStage::Stacking);
        QCOMPARE(restored->session.updatedAt(), createdAt + 1500us);
    }

    void sessionCreateIsAtomicAndRejectsInconsistentTargetIds()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("repository.sqlite3")));
        database.open();
        SqliteSessionRepository repository(database);
        QVERIFY(!repository.findById("missing").has_value());
        QVERIFY(repository.findByTargetId("missing").empty());

        const Target target("target-1", "M42");
        const Session session = makeSession(target, "session-1");
        QVERIFY_EXCEPTION_THROWN(
            repository.create(Target("other-target", "Other"), session),
            std::invalid_argument);
        QVERIFY(!repository.findById(session.id()).has_value());

        repository.create(target, session);
        QVERIFY_EXCEPTION_THROWN(
            repository.create(Target(target.id(), "Different name"), makeSession(target, "session-2")),
            RepositoryConflictError);
        QVERIFY(!repository.findById("session-2").has_value());
        QCOMPARE(scalarInt(
                     database.connection(),
                     QStringLiteral("SELECT count(*) FROM targets WHERE id = 'target-1' AND name = 'M42'")),
            1);

        QVERIFY_EXCEPTION_THROWN(repository.create(target, session), RepositoryError);
        const Target secondTarget("target-2", "M31");
        QVERIFY_EXCEPTION_THROWN(
            repository.create(secondTarget, makeSession(secondTarget, "session-1")),
            RepositoryError);
        QCOMPARE(scalarInt(
                     database.connection(),
                     QStringLiteral("SELECT count(*) FROM targets WHERE id = 'target-2'")),
            0);
    }

    void sessionUpdatesRejectMissingIdsAndBackwardsTimestamps()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("repository.sqlite3")));
        database.open();
        SqliteSessionRepository repository(database);

        QVERIFY_EXCEPTION_THROWN(repository.open("missing"), RepositoryNotFoundError);
        QVERIFY_EXCEPTION_THROWN(repository.close("missing"), RepositoryNotFoundError);
        QVERIFY_EXCEPTION_THROWN(
            repository.updateStage("missing", SessionStage::Editing, Timestamp{2s}),
            RepositoryNotFoundError);

        const Target target("target-1", "M42");
        const Timestamp createdAt{2s};
        const Session session = makeSession(target, "session-1", createdAt);
        repository.create(target, session);
        QVERIFY_EXCEPTION_THROWN(
            repository.updateStage(session.id(), SessionStage::Finished, createdAt - 1us),
            std::invalid_argument);
        QCOMPARE(repository.findById(session.id())->session.stage(), SessionStage::Capture);
        QCOMPARE(repository.findById(session.id())->session.updatedAt(), createdAt);
    }

    void persistsCaptureRunsAndValidatesStateTransitions()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = QDir(directory.path()).filePath(QStringLiteral("repository.sqlite3"));
        const Target target("target-1", "M42");
        const Session session = makeSession(target, "session-1");

        {
            SqliteDatabase database(databasePath);
            database.open();
            SqliteSessionRepository sessions(database);
            sessions.create(target, session);
            SqliteCaptureRunRepository runs(database);
            QVERIFY(!runs.findById("missing").has_value());
            QVERIFY(runs.findBySessionId("missing").empty());
            const CaptureRun run(
                "run-1",
                session.id(),
                std::chrono::year{2026} / 10 / 24,
                CaptureSettings{
                    FrameType::DarkFlat,
                    5s,
                    800,
                    CaptureMode::Finite,
                    4});
            const CaptureRun earlierRun(
                "run-0",
                session.id(),
                std::chrono::year{2026} / 10 / 23,
                CaptureSettings{
                    FrameType::Light,
                    3s,
                    std::nullopt,
                    CaptureMode::Finite,
                    1});
            runs.create(earlierRun);
            runs.create(run);
            SqliteFrameRepository frames(database);

            const auto restored = runs.findById(run.id());
            QVERIFY(restored.has_value());
            QCOMPARE(restored->id(), run.id());
            QCOMPARE(restored->sessionId(), session.id());
            QCOMPARE(restored->acquisitionDate(), run.acquisitionDate());
            QCOMPARE(restored->settings().frameType, FrameType::DarkFlat);
            QCOMPARE(restored->settings().exposure, 5s);
            QCOMPARE(restored->settings().iso, std::optional<unsigned int>{800});
            QCOMPARE(restored->settings().mode, CaptureMode::Finite);
            QCOMPARE(restored->settings().plannedFrameCount, std::optional<std::uint32_t>{4});
            QCOMPARE(restored->state(), CaptureRunState::Ready);
            const auto sessionRuns = runs.findBySessionId(session.id());
            QCOMPARE(sessionRuns.size(), std::size_t{2});
            QCOMPARE(sessionRuns.front().id(), CaptureRunId("run-0"));

            runs.updateState(run.id(), CaptureRunState::Running);
            runs.updateState(run.id(), CaptureRunState::StopRequested);
            runs.updateState(run.id(), CaptureRunState::Completed);
            QCOMPARE(runs.findById(run.id())->state(), CaptureRunState::Completed);

            runs.updateState(earlierRun.id(), CaptureRunState::Running);
            runs.updateState(earlierRun.id(), CaptureRunState::StopRequested);
            runs.updateState(earlierRun.id(), CaptureRunState::Paused);
            runs.updateState(earlierRun.id(), CaptureRunState::Running);
            runs.updateState(earlierRun.id(), CaptureRunState::Completed);
            QCOMPARE(runs.findById(earlierRun.id())->state(), CaptureRunState::Completed);

            Frame frame("frame-1", run.id(), FrameType::DarkFlat, 5s, 800);
            frames.create(frame);
            QVERIFY_EXCEPTION_THROWN(frames.create(frame), RepositoryError);
            QCOMPARE(frames.findById(frame.id())->state(), FrameState::Pending);

            frame.startExposure(Timestamp{2s});
            frames.update(frame);
            QCOMPARE(frames.findById(frame.id())->state(), FrameState::Exposing);
            frame.startTransfer();
            frames.update(frame);
            QCOMPARE(frames.findById(frame.id())->state(), FrameState::Transferring);
            frame.startValidation();
            frames.update(frame);
            QCOMPARE(frames.findById(frame.id())->state(), FrameState::Validating);
            frame.completeValidation("images/frame-1.CR2", 4096);
            frames.update(frame);

            const auto completedFrame = frames.findById(frame.id());
            QVERIFY(completedFrame.has_value());
            QCOMPARE(completedFrame->state(), FrameState::Completed);
            QCOMPARE(completedFrame->filePath(), std::optional<std::filesystem::path>("images/frame-1.CR2"));
            QCOMPARE(completedFrame->fileSize(), std::optional<std::uint64_t>{4096});

            Frame failedFrame("frame-2", run.id(), FrameType::DarkFlat);
            frames.create(failedFrame);
            failedFrame.startExposure(Timestamp{3s});
            frames.update(failedFrame);
            failedFrame.fail("simulated exposure failure");
            frames.update(failedFrame);
            QCOMPARE(frames.findById(failedFrame.id())->state(), FrameState::Failed);
            failedFrame.retry();
            frames.update(failedFrame);
            QCOMPARE(frames.findById(failedFrame.id())->state(), FrameState::Pending);

            auto invalidTransition = Frame::restore(
                "frame-1",
                run.id(),
                FrameType::DarkFlat,
                FrameState::Failed,
                5s,
                800,
                Timestamp{2s},
                std::nullopt,
                std::nullopt,
                "invalid direct failure");
            QVERIFY_EXCEPTION_THROWN(frames.update(invalidTransition), std::logic_error);
            QCOMPARE(frames.findById(frame.id())->state(), FrameState::Completed);
        }

        SqliteDatabase reopenedDatabase(databasePath);
        reopenedDatabase.open();
        SqliteCaptureRunRepository reopenedRuns(reopenedDatabase);
        const auto restored = reopenedRuns.findById("run-1");
        QVERIFY(restored.has_value());
        QCOMPARE(restored->state(), CaptureRunState::Completed);
        QCOMPARE(reopenedRuns.findBySessionId(session.id()).size(), std::size_t{2});
        SqliteFrameRepository reopenedFrames(reopenedDatabase);
        const auto restoredFrame = reopenedFrames.findById("frame-1");
        QVERIFY(restoredFrame.has_value());
        QCOMPARE(restoredFrame->state(), FrameState::Completed);
        QCOMPARE(restoredFrame->fileSize(), std::optional<std::uint64_t>{4096});
    }

    void supportsContinuousRunsAndRejectsInvalidStateChanges()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("repository.sqlite3")));
        database.open();
        SqliteSessionRepository sessions(database);
        const Target target("target-1", "M42");
        const Session session = makeSession(target, "session-1");
        sessions.create(target, session);
        SqliteCaptureRunRepository runs(database);

        const CaptureRun continuousRun(
            "run-1",
            session.id(),
            std::chrono::year{2026} / 10 / 24,
            CaptureSettings{
                FrameType::Light,
                1s,
                std::nullopt,
                CaptureMode::Continuous,
                std::nullopt});
        runs.create(continuousRun);
        QVERIFY(!runs.findById("run-1")->settings().iso.has_value());
        QVERIFY(!runs.findById("run-1")->settings().plannedFrameCount.has_value());

        QVERIFY_EXCEPTION_THROWN(runs.updateState("missing", CaptureRunState::Running), RepositoryNotFoundError);
        QVERIFY_EXCEPTION_THROWN(
            runs.updateState("run-1", static_cast<CaptureRunState>(99)),
            std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(runs.updateState("run-1", CaptureRunState::Completed), std::logic_error);
        QCOMPARE(runs.findById("run-1")->state(), CaptureRunState::Ready);
        runs.updateState("run-1", CaptureRunState::Running);
        runs.updateState("run-1", CaptureRunState::Running);
        runs.updateState("run-1", CaptureRunState::Failed);
        QVERIFY_EXCEPTION_THROWN(runs.updateState("run-1", CaptureRunState::Running), std::logic_error);
        QCOMPARE(runs.findById("run-1")->state(), CaptureRunState::Failed);
    }
};

QTEST_GUILESS_MAIN(SqliteRepositoriesTest)

#include "test_sqlite_repositories.moc"
