#include "application/session_recovery_service.h"
#include "domain/repositories/repository_errors.h"
#include "infrastructure/persistence/sqlite_database.h"
#include "infrastructure/persistence/sqlite_repositories.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <algorithm>
#include <chrono>
#include <optional>

using namespace std::chrono_literals;
using namespace domain;
using namespace infrastructure;
using namespace application;

namespace {

class RecordingLoggingService final : public LoggingService
{
public:
    void log(const LogEvent& event) override
    {
        events.push_back(event);
    }

    std::vector<LogEvent> events;
};

void insertFrame(
    SqliteDatabase& database,
    const QString& id,
    const QString& runId,
    const QString& state,
    const std::optional<QString>& capturedAt = std::nullopt,
    const std::optional<QString>& filePath = std::nullopt,
    const std::optional<qint64>& fileSize = std::nullopt,
    const std::optional<QString>& errorMessage = std::nullopt)
{
    QSqlQuery query(database.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO frames "
            "(id, capture_run_id, type, state, exposure_microseconds, iso, captured_at, "
            "file_path, file_size, error_message) "
            "VALUES (:id, :run_id, 'Light', :state, 5000000, 800, :captured_at, "
            ":file_path, :file_size, :error_message)"))) {
        throw std::runtime_error(query.lastError().text().toStdString());
    }
    query.bindValue(QStringLiteral(":id"), id);
    query.bindValue(QStringLiteral(":run_id"), runId);
    query.bindValue(QStringLiteral(":state"), state);
    query.bindValue(QStringLiteral(":captured_at"), capturedAt
            ? QVariant(*capturedAt)
            : QVariant{});
    query.bindValue(QStringLiteral(":file_path"), filePath ? QVariant(*filePath) : QVariant{});
    query.bindValue(QStringLiteral(":file_size"), fileSize ? QVariant(*fileSize) : QVariant{});
    query.bindValue(
        QStringLiteral(":error_message"),
        errorMessage ? QVariant(*errorMessage) : QVariant{});
    if (!query.exec()) {
        throw std::runtime_error(query.lastError().text().toStdString());
    }
}

void writeFile(const QString& path, const QByteArray& bytes)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), static_cast<qint64>(bytes.size()));
}

std::size_t findingCount(
    const SessionRecoveryReport& report,
    RecoveryFindingType type)
{
    return static_cast<std::size_t>(std::count_if(
        report.findings.cbegin(),
        report.findings.cend(),
        [type](const RecoveryFinding& finding) { return finding.type == type; }));
}

} // namespace

class SessionRecoveryTest final : public QObject
{
    Q_OBJECT

private slots:
    void detectsDatabaseAndFilesystemInconsistenciesAndAuditsChoices()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString databasePath =
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("recovery.sqlite3"));
        const QString sessionDirectory =
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("images/session-1"));
        const QString completedPath =
            QDir(sessionDirectory).filePath(QStringLiteral("nights/2026-10-24/Light/frame-complete.CR2"));
        const QString sizeMismatchPath =
            QDir(sessionDirectory).filePath(QStringLiteral("nights/2026-10-24/Light/frame-size.CR2"));
        const QString missingPath =
            QDir(sessionDirectory).filePath(QStringLiteral("nights/2026-10-24/Light/frame-missing.CR2"));
        const QString orphanPath =
            QDir(sessionDirectory).filePath(QStringLiteral("nights/2026-10-24/Light/orphan.FITS"));
        const QString temporaryPath =
            QDir(sessionDirectory)
                .filePath(QStringLiteral("nights/2026-10-24/Light/.astrocapture-abandoned"));
        writeFile(completedPath, QByteArray("data"));
        writeFile(sizeMismatchPath, QByteArray("bad"));
        writeFile(orphanPath, QByteArray("orphan"));
        writeFile(temporaryPath, QByteArray("partial"));

        SqliteDatabase database(databasePath);
        database.open();
        const Target target("target-1", "M42");
        const Timestamp timestamp{1s};
        const Session session(
            "session-1",
            target.id(),
            "Night",
            SessionStage::Capture,
            timestamp,
            timestamp);
        SqliteSessionRepository sessionRepository(database);
        sessionRepository.create(target, session);

        const CaptureRun run(
            "run-1",
            session.id(),
            std::chrono::year{2026} / 10 / 24,
            CaptureSettings{FrameType::Light, 5s, 800, CaptureMode::Finite, 4});
        SqliteCaptureRunRepository captureRunRepository(database);
        captureRunRepository.create(run);
        captureRunRepository.updateState(run.id(), CaptureRunState::Running);

        insertFrame(
            database,
            QStringLiteral("complete"),
            QStringLiteral("run-1"),
            QStringLiteral("Completed"),
            QStringLiteral("2026-10-24T12:00:00.000000Z"),
            completedPath,
            4);
        insertFrame(
            database,
            QStringLiteral("size-mismatch"),
            QStringLiteral("run-1"),
            QStringLiteral("Completed"),
            QStringLiteral("2026-10-24T12:01:00.000000Z"),
            sizeMismatchPath,
            10);
        insertFrame(
            database,
            QStringLiteral("missing"),
            QStringLiteral("run-1"),
            QStringLiteral("Completed"),
            QStringLiteral("2026-10-24T12:02:00.000000Z"),
            missingPath,
            12);
        insertFrame(
            database,
            QStringLiteral("pending"),
            QStringLiteral("run-1"),
            QStringLiteral("Pending"));
        insertFrame(
            database,
            QStringLiteral("interrupted"),
            QStringLiteral("run-1"),
            QStringLiteral("Transferring"),
            QStringLiteral("2026-10-24T12:03:00.000000Z"));
        insertFrame(
            database,
            QStringLiteral("failed"),
            QStringLiteral("run-1"),
            QStringLiteral("Failed"),
            QStringLiteral("2026-10-24T12:04:00.000000Z"),
            std::nullopt,
            std::nullopt,
            QStringLiteral("transfer failed"));

        SqliteFrameRepository frameRepository(database);
        const std::vector<Frame> frames = frameRepository.findByCaptureRunId(run.id());
        QCOMPARE(frames.size(), std::size_t{6});
        QCOMPARE(frames.front().id(), FrameId("complete"));

        LocalSessionFileInspector inspector;
        RecordingLoggingService logging;
        SessionRecoveryService recovery(
            captureRunRepository,
            frameRepository,
            inspector,
            logging);
        const auto record = sessionRepository.findById(session.id());
        QVERIFY(record.has_value());
        const SessionRecoveryReport report = recovery.inspect(*record, sessionDirectory);

        QVERIFY(report.requiresDecision());
        QCOMPARE(report.sessionId, session.id());
        QCOMPARE(report.incompleteRuns.size(), std::size_t{1});
        QCOMPARE(report.incompleteRuns.front().id, run.id());
        QCOMPARE(report.incompleteRuns.front().state, CaptureRunState::Running);
        QCOMPARE(report.incompleteRuns.front().pendingFrameIds, std::vector<FrameId>{"pending"});
        QCOMPARE(report.incompleteRuns.front().interruptedFrameIds, std::vector<FrameId>{"interrupted"});
        QVERIFY(report.incompleteRuns.front().canContinue);
        QCOMPARE(
            report.availableDecisions,
            (std::vector<RecoveryDecision>{
                RecoveryDecision::Continue,
                RecoveryDecision::NewSequence,
                RecoveryDecision::Cancel}));
        QCOMPARE(findingCount(report, RecoveryFindingType::PendingFrame), std::size_t{1});
        QCOMPARE(findingCount(report, RecoveryFindingType::InterruptedFrame), std::size_t{1});
        QCOMPARE(findingCount(report, RecoveryFindingType::FailedFrame), std::size_t{1});
        QCOMPARE(findingCount(report, RecoveryFindingType::MissingCompletedFile), std::size_t{1});
        QCOMPARE(findingCount(report, RecoveryFindingType::CompletedFileSizeMismatch), std::size_t{1});
        QCOMPARE(findingCount(report, RecoveryFindingType::OrphanFile), std::size_t{1});
        QCOMPARE(findingCount(report, RecoveryFindingType::TemporaryFile), std::size_t{1});
        const auto sizeFinding = std::find_if(
            report.findings.cbegin(),
            report.findings.cend(),
            [](const RecoveryFinding& finding) {
                return finding.type == RecoveryFindingType::CompletedFileSizeMismatch;
            });
        QVERIFY(sizeFinding != report.findings.cend());
        QCOMPARE(sizeFinding->details, QStringLiteral("Expected 10 bytes but found 3 bytes."));

        recovery.recordDecision(report, RecoveryDecision::Continue, run.id());
        recovery.recordDecision(report, RecoveryDecision::NewSequence);
        recovery.recordDecision(report, RecoveryDecision::Cancel);
        QCOMPARE(logging.events.size(), std::size_t{3});
        QCOMPARE(logging.events[0].event, QStringLiteral("session.recovery.decision"));
        QCOMPARE(logging.events[0].sessionId, std::optional<SessionId>{session.id()});
        QCOMPARE(logging.events[0].captureRunId, std::optional<CaptureRunId>{run.id()});
        QCOMPARE(logging.events[0].state, std::optional<QString>{QStringLiteral("Continue")});
        QCOMPARE(logging.events[1].state, std::optional<QString>{QStringLiteral("NewSequence")});
        QCOMPARE(logging.events[2].state, std::optional<QString>{QStringLiteral("Cancel")});

        QVERIFY_EXCEPTION_THROWN(
            recovery.recordDecision(report, RecoveryDecision::Continue, "unknown-run"),
            std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(
            recovery.recordDecision(report, RecoveryDecision::NewSequence, run.id()),
            std::invalid_argument);

        const auto persistedFrames = frameRepository.findByCaptureRunId(run.id());
        QCOMPARE(persistedFrames.size(), std::size_t{6});
        const auto persistedState = [&persistedFrames](const FrameId& id) {
            const auto frame = std::find_if(
                persistedFrames.cbegin(),
                persistedFrames.cend(),
                [&id](const Frame& value) { return value.id() == id; });
            if (frame == persistedFrames.cend()) {
                return FrameState::Failed;
            }
            return frame->state();
        };
        QCOMPARE(persistedState("complete"), FrameState::Completed);
        QCOMPARE(persistedState("size-mismatch"), FrameState::Completed);
        QCOMPARE(persistedState("pending"), FrameState::Pending);
        QCOMPARE(persistedState("interrupted"), FrameState::Transferring);
        QCOMPARE(captureRunRepository.findById(run.id())->state(), CaptureRunState::Running);
    }

    void doesNotRequireRecoveryDecisionForCompletedRuns()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        SqliteDatabase database(
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("completed.sqlite3")));
        database.open();
        const Target target("target-1", "M42");
        const Timestamp timestamp{1s};
        const Session session(
            "session-1",
            target.id(),
            "Night",
            SessionStage::Capture,
            timestamp,
            timestamp);
        SqliteSessionRepository sessionRepository(database);
        sessionRepository.create(target, session);
        SqliteCaptureRunRepository captureRunRepository(database);
        SqliteFrameRepository frameRepository(database);
        const CaptureRun run(
            "run-1",
            session.id(),
            std::chrono::year{2026} / 10 / 24,
            CaptureSettings{FrameType::Light, 5s, 800, CaptureMode::Finite, 1});
        captureRunRepository.create(run);
        captureRunRepository.updateState(run.id(), CaptureRunState::Running);
        captureRunRepository.updateState(run.id(), CaptureRunState::Completed);

        LocalSessionFileInspector inspector;
        RecordingLoggingService logging;
        SessionRecoveryService recovery(captureRunRepository, frameRepository, inspector, logging);
        const auto record = sessionRepository.findById(session.id());
        QVERIFY(record.has_value());
        const auto report = recovery.inspect(
            *record,
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("images/session-1")));
        QVERIFY(!report.requiresDecision());
        QVERIFY(report.incompleteRuns.empty());
        QVERIFY_EXCEPTION_THROWN(
            recovery.recordDecision(report, RecoveryDecision::Cancel),
            std::logic_error);
        QVERIFY(logging.events.empty());
    }
};

QTEST_GUILESS_MAIN(SessionRecoveryTest)

#include "test_session_recovery.moc"
