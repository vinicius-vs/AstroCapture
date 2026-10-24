#include "infrastructure/persistence/sqlite_repositories.h"

#include "domain/repositories/repository_errors.h"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace infrastructure {
namespace {

using namespace domain;

[[noreturn]] void throwQueryError(const QSqlQuery& query, const QString& operation)
{
    throw RepositoryError(
        QStringLiteral("%1: %2").arg(operation, query.lastError().text()).toStdString());
}

bool next(QSqlQuery& query, const QString& operation)
{
    if (query.next()) {
        return true;
    }
    if (query.lastError().isValid()) {
        throwQueryError(query, operation);
    }
    return false;
}

void execute(SqliteDatabase& database, QSqlQuery& query, const QString& operation)
{
    try {
        database.execute(query);
    } catch (const std::exception& error) {
        throw RepositoryError(
            QStringLiteral("%1: %2").arg(operation, QString::fromUtf8(error.what())).toStdString());
    }
}

void beginTransaction(QSqlDatabase& connection)
{
    if (!connection.transaction()) {
        throw RepositoryError(
            QStringLiteral("Starting repository transaction failed: %1")
                .arg(connection.lastError().text())
                .toStdString());
    }
}

void commitTransaction(QSqlDatabase& connection)
{
    if (!connection.commit()) {
        throw RepositoryError(
            QStringLiteral("Committing repository transaction failed: %1")
                .arg(connection.lastError().text())
                .toStdString());
    }
}

void rollbackTransaction(QSqlDatabase& connection)
{
    if (!connection.rollback()) {
        throw RepositoryError(
            QStringLiteral("Rolling back repository transaction failed: %1")
                .arg(connection.lastError().text())
                .toStdString());
    }
}

QString text(const std::string& value)
{
    return QString::fromUtf8(value);
}

QString sessionStageName(SessionStage stage)
{
    switch (stage) {
    case SessionStage::Capture:
        return QStringLiteral("Capture");
    case SessionStage::Stacking:
        return QStringLiteral("Stacking");
    case SessionStage::Editing:
        return QStringLiteral("Editing");
    case SessionStage::Finished:
        return QStringLiteral("Finished");
    }
    throw std::invalid_argument("Invalid session stage");
}

SessionStage sessionStageFromName(const QString& value)
{
    if (value == QStringLiteral("Capture")) {
        return SessionStage::Capture;
    }
    if (value == QStringLiteral("Stacking")) {
        return SessionStage::Stacking;
    }
    if (value == QStringLiteral("Editing")) {
        return SessionStage::Editing;
    }
    if (value == QStringLiteral("Finished")) {
        return SessionStage::Finished;
    }
    throw RepositoryError("Database contains an unknown session stage");
}

QString frameTypeName(FrameType frameType)
{
    switch (frameType) {
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

FrameType frameTypeFromName(const QString& value)
{
    if (value == QStringLiteral("Light")) {
        return FrameType::Light;
    }
    if (value == QStringLiteral("Dark")) {
        return FrameType::Dark;
    }
    if (value == QStringLiteral("Flat")) {
        return FrameType::Flat;
    }
    if (value == QStringLiteral("Bias")) {
        return FrameType::Bias;
    }
    if (value == QStringLiteral("DarkFlat")) {
        return FrameType::DarkFlat;
    }
    throw RepositoryError("Database contains an unknown frame type");
}

FrameState frameStateFromName(const QString& value)
{
    if (value == QStringLiteral("Pending")) {
        return FrameState::Pending;
    }
    if (value == QStringLiteral("Exposing")) {
        return FrameState::Exposing;
    }
    if (value == QStringLiteral("Transferring")) {
        return FrameState::Transferring;
    }
    if (value == QStringLiteral("Validating")) {
        return FrameState::Validating;
    }
    if (value == QStringLiteral("Completed")) {
        return FrameState::Completed;
    }
    if (value == QStringLiteral("Failed")) {
        return FrameState::Failed;
    }
    throw RepositoryError("Database contains an unknown frame state");
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

CaptureMode captureModeFromName(const QString& value)
{
    if (value == QStringLiteral("Finite")) {
        return CaptureMode::Finite;
    }
    if (value == QStringLiteral("Continuous")) {
        return CaptureMode::Continuous;
    }
    throw RepositoryError("Database contains an unknown capture mode");
}

QString captureRunStateName(CaptureRunState state)
{
    switch (state) {
    case CaptureRunState::Ready:
        return QStringLiteral("Ready");
    case CaptureRunState::Running:
        return QStringLiteral("Running");
    case CaptureRunState::StopRequested:
        return QStringLiteral("StopRequested");
    case CaptureRunState::Paused:
        return QStringLiteral("Paused");
    case CaptureRunState::Completed:
        return QStringLiteral("Completed");
    case CaptureRunState::Failed:
        return QStringLiteral("Failed");
    }
    throw std::invalid_argument("Invalid capture run state");
}

CaptureRunState captureRunStateFromName(const QString& value)
{
    if (value == QStringLiteral("Ready")) {
        return CaptureRunState::Ready;
    }
    if (value == QStringLiteral("Running")) {
        return CaptureRunState::Running;
    }
    if (value == QStringLiteral("StopRequested")) {
        return CaptureRunState::StopRequested;
    }
    if (value == QStringLiteral("Paused")) {
        return CaptureRunState::Paused;
    }
    if (value == QStringLiteral("Completed")) {
        return CaptureRunState::Completed;
    }
    if (value == QStringLiteral("Failed")) {
        return CaptureRunState::Failed;
    }
    throw RepositoryError("Database contains an unknown capture run state");
}

QString timestampToString(Timestamp timestamp)
{
    const auto elapsed = std::chrono::floor<std::chrono::microseconds>(timestamp.time_since_epoch());
    const auto seconds = std::chrono::floor<std::chrono::seconds>(elapsed);
    const auto fractionalMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        elapsed - seconds);
    const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(seconds.count()).toUTC();
    if (!dateTime.isValid()) {
        throw std::invalid_argument("Timestamp is outside the supported UTC date range");
    }
    return dateTime.toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss"))
        + QLatin1Char('.')
        + QStringLiteral("%1").arg(fractionalMicroseconds.count(), 6, 10, QLatin1Char('0'))
        + QLatin1Char('Z');
}

Timestamp timestampFromString(const QString& value)
{
    static const QRegularExpression format(
        QStringLiteral(R"(^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})\.(\d{6})Z$)"));
    const QRegularExpressionMatch match = format.match(value);
    if (!match.hasMatch()) {
        throw RepositoryError("Database contains an invalid UTC timestamp");
    }

    const QString dateAndTime = match.captured(1);
    const QDate date = QDate::fromString(dateAndTime.left(10), Qt::ISODate);
    const int hour = dateAndTime.mid(11, 2).toInt();
    const int minute = dateAndTime.mid(14, 2).toInt();
    const int second = dateAndTime.mid(17, 2).toInt();
    if (!date.isValid() || hour > 23 || minute > 59 || second > 59) {
        throw RepositoryError("Database contains an invalid UTC timestamp");
    }

    const std::int64_t fractionalMicroseconds = match.captured(2).toLongLong();
    const auto day = std::chrono::sys_days{
        std::chrono::year{date.year()}
        / static_cast<unsigned int>(date.month())
        / static_cast<unsigned int>(date.day())};
    const auto totalMicroseconds = day
        + std::chrono::hours{hour}
        + std::chrono::minutes{minute}
        + std::chrono::seconds{second}
        + std::chrono::microseconds{fractionalMicroseconds};
    return Timestamp{std::chrono::duration_cast<Timestamp::duration>(totalMicroseconds.time_since_epoch())};
}

QString acquisitionDateToString(std::chrono::year_month_day date)
{
    if (!date.ok()) {
        throw std::invalid_argument("Invalid capture acquisition date");
    }
    const QDate value(
        static_cast<int>(date.year()),
        static_cast<int>(static_cast<unsigned int>(date.month())),
        static_cast<int>(static_cast<unsigned int>(date.day())));
    return value.toString(Qt::ISODate);
}

std::chrono::year_month_day acquisitionDateFromString(const QString& value)
{
    const QDate date = QDate::fromString(value, Qt::ISODate);
    if (!date.isValid() || date.toString(Qt::ISODate) != value) {
        throw RepositoryError("Database contains an invalid capture acquisition date");
    }
    return std::chrono::year{date.year()} / static_cast<unsigned int>(date.month())
        / static_cast<unsigned int>(date.day());
}

SessionRecord sessionRecordFromQuery(const QSqlQuery& query)
{
    Session session(
        query.value(0).toString().toStdString(),
        query.value(1).toString().toStdString(),
        query.value(2).toString().toStdString(),
        sessionStageFromName(query.value(3).toString()),
        timestampFromString(query.value(4).toString()),
        timestampFromString(query.value(5).toString()));
    return {std::move(session), query.value(6).toInt() != 0};
}

CaptureRun captureRunFromQuery(const QSqlQuery& query)
{
    const auto exposureValue = query.value(4).toLongLong();
    const auto iso = query.value(5).isNull()
        ? std::optional<unsigned int>{}
        : std::optional<unsigned int>{query.value(5).toUInt()};
    const CaptureMode mode = captureModeFromName(query.value(6).toString());
    const auto plannedFrameCount = query.value(7).isNull()
        ? std::optional<std::uint32_t>{}
        : std::optional<std::uint32_t>{query.value(7).toUInt()};

    return CaptureRun::restore(
        query.value(0).toString().toStdString(),
        query.value(1).toString().toStdString(),
        acquisitionDateFromString(query.value(2).toString()),
        CaptureSettings{
            frameTypeFromName(query.value(3).toString()),
            std::chrono::microseconds{exposureValue},
            iso,
            mode,
            plannedFrameCount},
        captureRunStateFromName(query.value(8).toString()));
}

Frame frameFromQuery(const QSqlQuery& query)
{
    const auto exposure = query.value(4).isNull()
        ? std::optional<std::chrono::microseconds>{}
        : std::optional<std::chrono::microseconds>{
              std::chrono::microseconds{query.value(4).toLongLong()}};
    const auto iso = query.value(5).isNull()
        ? std::optional<unsigned int>{}
        : std::optional<unsigned int>{query.value(5).toUInt()};
    const auto capturedAt = query.value(6).isNull()
        ? std::optional<Timestamp>{}
        : std::optional<Timestamp>{timestampFromString(query.value(6).toString())};
    const auto filePath = query.value(7).isNull()
        ? std::optional<std::filesystem::path>{}
        : std::optional<std::filesystem::path>{
              std::filesystem::path{query.value(7).toString().toStdString()}};
    const auto fileSize = query.value(8).isNull()
        ? std::optional<std::uint64_t>{}
        : std::optional<std::uint64_t>{static_cast<std::uint64_t>(query.value(8).toLongLong())};
    const auto errorMessage = query.value(9).isNull()
        ? std::optional<std::string>{}
        : std::optional<std::string>{query.value(9).toString().toStdString()};

    return Frame::restore(
        query.value(0).toString().toStdString(),
        query.value(1).toString().toStdString(),
        frameTypeFromName(query.value(2).toString()),
        frameStateFromName(query.value(3).toString()),
        exposure,
        iso,
        capturedAt,
        filePath,
        fileSize,
        errorMessage);
}

std::optional<SessionRecord> findSessionById(SqliteDatabase& database, const SessionId& id)
{
    QSqlQuery query(database.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, target_id, name, stage, created_at, updated_at, is_open "
            "FROM sessions WHERE id = :id"))) {
        throwQueryError(query, QStringLiteral("Preparing session query failed"));
    }
    query.bindValue(QStringLiteral(":id"), text(id));
    execute(database, query, QStringLiteral("Querying session failed"));
    if (!next(query, QStringLiteral("Reading session query result failed"))) {
        return std::nullopt;
    }
    return sessionRecordFromQuery(query);
}

std::optional<CaptureRun> findCaptureRunById(SqliteDatabase& database, const CaptureRunId& id)
{
    QSqlQuery query(database.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, session_id, acquisition_date, frame_type, exposure_microseconds, "
            "iso, mode, planned_frame_count, state "
            "FROM capture_runs WHERE id = :id"))) {
        throwQueryError(query, QStringLiteral("Preparing capture run query failed"));
    }
    query.bindValue(QStringLiteral(":id"), text(id));
    execute(database, query, QStringLiteral("Querying capture run failed"));
    if (!next(query, QStringLiteral("Reading capture run query result failed"))) {
        return std::nullopt;
    }
    return captureRunFromQuery(query);
}

void insertSession(SqliteDatabase& database, const Session& session)
{
    QSqlQuery query(database.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO sessions "
            "(id, target_id, name, stage, created_at, updated_at, is_open) "
            "VALUES (:id, :target_id, :name, :stage, :created_at, :updated_at, 1)"))) {
        throwQueryError(query, QStringLiteral("Preparing session insert failed"));
    }
    query.bindValue(QStringLiteral(":id"), text(session.id()));
    query.bindValue(QStringLiteral(":target_id"), text(session.targetId()));
    query.bindValue(QStringLiteral(":name"), text(session.name()));
    query.bindValue(QStringLiteral(":stage"), sessionStageName(session.stage()));
    query.bindValue(QStringLiteral(":created_at"), timestampToString(session.createdAt()));
    query.bindValue(QStringLiteral(":updated_at"), timestampToString(session.updatedAt()));
    execute(database, query, QStringLiteral("Creating session failed"));
}

void ensureTarget(SqliteDatabase& database, const Target& target)
{
    QSqlQuery query(database.connection());
    if (!query.prepare(QStringLiteral("SELECT name FROM targets WHERE id = :id"))) {
        throwQueryError(query, QStringLiteral("Preparing target query failed"));
    }
    query.bindValue(QStringLiteral(":id"), text(target.id()));
    execute(database, query, QStringLiteral("Querying target failed"));
    if (next(query, QStringLiteral("Reading target query result failed"))) {
        if (query.value(0).toString() != text(target.name())) {
            throw RepositoryConflictError("Target identifier already exists with a different name");
        }
        return;
    }

    QSqlQuery insertQuery(database.connection());
    if (!insertQuery.prepare(QStringLiteral("INSERT INTO targets (id, name) VALUES (:id, :name)"))) {
        throwQueryError(insertQuery, QStringLiteral("Preparing target insert failed"));
    }
    insertQuery.bindValue(QStringLiteral(":id"), text(target.id()));
    insertQuery.bindValue(QStringLiteral(":name"), text(target.name()));
    execute(database, insertQuery, QStringLiteral("Creating target failed"));
}

void setSessionOpen(SqliteDatabase& database, const SessionId& id, bool isOpen)
{
    QSqlQuery query(database.connection());
    if (!query.prepare(QStringLiteral("UPDATE sessions SET is_open = :is_open WHERE id = :id"))) {
        throwQueryError(query, QStringLiteral("Preparing session open-state update failed"));
    }
    query.bindValue(QStringLiteral(":is_open"), isOpen ? 1 : 0);
    query.bindValue(QStringLiteral(":id"), text(id));
    execute(database, query, QStringLiteral("Updating session open state failed"));

    if (query.numRowsAffected() == 0 && !findSessionById(database, id)) {
        throw RepositoryNotFoundError("Session does not exist");
    }
}

CaptureRun applyStateTransition(CaptureRun captureRun, CaptureRunState desiredState)
{
    if (captureRun.state() == desiredState) {
        return captureRun;
    }

    switch (desiredState) {
    case CaptureRunState::Ready:
        throw std::logic_error("Capture run cannot transition back to Ready");
    case CaptureRunState::Running:
        if (captureRun.state() == CaptureRunState::Paused) {
            captureRun.resume();
        } else {
            captureRun.start();
        }
        break;
    case CaptureRunState::StopRequested:
        captureRun.requestStop();
        break;
    case CaptureRunState::Paused:
        captureRun.pause();
        break;
    case CaptureRunState::Completed:
        captureRun.complete();
        break;
    case CaptureRunState::Failed:
        captureRun.fail();
        break;
    }
    return captureRun;
}

} // namespace

SqliteSessionRepository::SqliteSessionRepository(SqliteDatabase& database)
    : database_(database)
{
}

void SqliteSessionRepository::create(const Target& target, const Session& session)
{
    if (target.id() != session.targetId()) {
        throw std::invalid_argument("Session target identifier does not match target");
    }

    QSqlDatabase connection = database_.connection();
    beginTransaction(connection);
    try {
        ensureTarget(database_, target);
        insertSession(database_, session);
        commitTransaction(connection);
    } catch (const std::exception& error) {
        const std::string operationError = error.what();
        try {
            rollbackTransaction(connection);
        } catch (const std::exception& rollbackError) {
            throw RepositoryError(operationError + "; " + rollbackError.what());
        }
        throw;
    }
}

std::optional<SessionRecord> SqliteSessionRepository::findById(const SessionId& id) const
{
    return findSessionById(database_, id);
}

std::vector<SessionRecord> SqliteSessionRepository::findByTargetId(const TargetId& targetId) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, target_id, name, stage, created_at, updated_at, is_open "
            "FROM sessions WHERE target_id = :target_id ORDER BY created_at, id"))) {
        throwQueryError(query, QStringLiteral("Preparing sessions-by-target query failed"));
    }
    query.bindValue(QStringLiteral(":target_id"), text(targetId));
    execute(database_, query, QStringLiteral("Querying sessions by target failed"));

    std::vector<SessionRecord> sessions;
    while (next(query, QStringLiteral("Reading sessions-by-target result failed"))) {
        sessions.push_back(sessionRecordFromQuery(query));
    }
    return sessions;
}

void SqliteSessionRepository::updateStage(
    const SessionId& id,
    SessionStage stage,
    Timestamp updatedAt)
{
    QSqlDatabase connection = database_.connection();
    beginTransaction(connection);
    try {
        const auto record = findSessionById(database_, id);
        if (!record) {
            throw RepositoryNotFoundError("Session does not exist");
        }

        Session updatedSession = record->session;
        updatedSession.setStage(stage, updatedAt);

        QSqlQuery query(database_.connection());
        if (!query.prepare(QStringLiteral(
                "UPDATE sessions SET stage = :stage, updated_at = :updated_at "
                "WHERE id = :id AND updated_at = :previous_updated_at"))) {
            throwQueryError(query, QStringLiteral("Preparing session stage update failed"));
        }
        query.bindValue(QStringLiteral(":stage"), sessionStageName(updatedSession.stage()));
        query.bindValue(QStringLiteral(":updated_at"), timestampToString(updatedSession.updatedAt()));
        query.bindValue(QStringLiteral(":id"), text(id));
        query.bindValue(
            QStringLiteral(":previous_updated_at"),
            timestampToString(record->session.updatedAt()));
        execute(database_, query, QStringLiteral("Updating session stage failed"));
        if (query.numRowsAffected() != 1) {
            throw RepositoryConflictError("Session changed concurrently while updating its stage");
        }
        commitTransaction(connection);
    } catch (const std::exception& error) {
        const std::string operationError = error.what();
        try {
            rollbackTransaction(connection);
        } catch (const std::exception& rollbackError) {
            throw RepositoryError(operationError + "; " + rollbackError.what());
        }
        throw;
    }
}

void SqliteSessionRepository::open(const SessionId& id)
{
    setSessionOpen(database_, id, true);
}

void SqliteSessionRepository::close(const SessionId& id)
{
    setSessionOpen(database_, id, false);
}

SqliteCaptureRunRepository::SqliteCaptureRunRepository(SqliteDatabase& database)
    : database_(database)
{
}

void SqliteCaptureRunRepository::create(const CaptureRun& captureRun)
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO capture_runs "
            "(id, session_id, acquisition_date, frame_type, exposure_microseconds, "
            "iso, mode, planned_frame_count, state) "
            "VALUES (:id, :session_id, :acquisition_date, :frame_type, :exposure, "
            ":iso, :mode, :planned_frame_count, :state)"))) {
        throwQueryError(query, QStringLiteral("Preparing capture run insert failed"));
    }
    query.bindValue(QStringLiteral(":id"), text(captureRun.id()));
    query.bindValue(QStringLiteral(":session_id"), text(captureRun.sessionId()));
    query.bindValue(QStringLiteral(":acquisition_date"), acquisitionDateToString(captureRun.acquisitionDate()));
    query.bindValue(QStringLiteral(":frame_type"), frameTypeName(captureRun.settings().frameType));
    query.bindValue(
        QStringLiteral(":exposure"),
        static_cast<qlonglong>(captureRun.settings().exposure.count()));
    if (captureRun.settings().iso) {
        query.bindValue(QStringLiteral(":iso"), *captureRun.settings().iso);
    } else {
        query.bindValue(QStringLiteral(":iso"), QVariant{});
    }
    query.bindValue(QStringLiteral(":mode"), captureModeName(captureRun.settings().mode));
    if (captureRun.settings().plannedFrameCount) {
        query.bindValue(QStringLiteral(":planned_frame_count"), *captureRun.settings().plannedFrameCount);
    } else {
        query.bindValue(QStringLiteral(":planned_frame_count"), QVariant{});
    }
    query.bindValue(QStringLiteral(":state"), captureRunStateName(captureRun.state()));
    execute(database_, query, QStringLiteral("Creating capture run failed"));
}

std::optional<CaptureRun> SqliteCaptureRunRepository::findById(const CaptureRunId& id) const
{
    return findCaptureRunById(database_, id);
}

std::vector<CaptureRun> SqliteCaptureRunRepository::findBySessionId(const SessionId& sessionId) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, session_id, acquisition_date, frame_type, exposure_microseconds, "
            "iso, mode, planned_frame_count, state "
            "FROM capture_runs WHERE session_id = :session_id ORDER BY acquisition_date, id"))) {
        throwQueryError(query, QStringLiteral("Preparing capture runs-by-session query failed"));
    }
    query.bindValue(QStringLiteral(":session_id"), text(sessionId));
    execute(database_, query, QStringLiteral("Querying capture runs by session failed"));

    std::vector<CaptureRun> captureRuns;
    while (next(query, QStringLiteral("Reading capture runs-by-session result failed"))) {
        captureRuns.push_back(captureRunFromQuery(query));
    }
    return captureRuns;
}

void SqliteCaptureRunRepository::updateState(
    const CaptureRunId& id,
    CaptureRunState desiredState)
{
    QSqlDatabase connection = database_.connection();
    beginTransaction(connection);
    try {
        const auto captureRun = findCaptureRunById(database_, id);
        if (!captureRun) {
            throw RepositoryNotFoundError("Capture run does not exist");
        }
        const CaptureRun updatedRun = applyStateTransition(*captureRun, desiredState);
        if (updatedRun.state() == captureRun->state()) {
            commitTransaction(connection);
            return;
        }

        QSqlQuery query(database_.connection());
        if (!query.prepare(QStringLiteral(
                "UPDATE capture_runs SET state = :state WHERE id = :id AND state = :previous_state"))) {
            throwQueryError(query, QStringLiteral("Preparing capture run state update failed"));
        }
        query.bindValue(QStringLiteral(":state"), captureRunStateName(updatedRun.state()));
        query.bindValue(QStringLiteral(":id"), text(id));
        query.bindValue(QStringLiteral(":previous_state"), captureRunStateName(captureRun->state()));
        execute(database_, query, QStringLiteral("Updating capture run state failed"));
        if (query.numRowsAffected() != 1) {
            throw RepositoryConflictError("Capture run changed concurrently while updating its state");
        }
        commitTransaction(connection);
    } catch (const std::exception& error) {
        const std::string operationError = error.what();
        try {
            rollbackTransaction(connection);
        } catch (const std::exception& rollbackError) {
            throw RepositoryError(operationError + "; " + rollbackError.what());
        }
        throw;
    }
}

SqliteFrameRepository::SqliteFrameRepository(SqliteDatabase& database)
    : database_(database)
{
}

std::vector<Frame> SqliteFrameRepository::findByCaptureRunId(
    const CaptureRunId& captureRunId) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, capture_run_id, type, state, exposure_microseconds, iso, captured_at, "
            "file_path, file_size, error_message "
            "FROM frames WHERE capture_run_id = :capture_run_id ORDER BY id"))) {
        throwQueryError(query, QStringLiteral("Preparing frames-by-capture-run query failed"));
    }
    query.bindValue(QStringLiteral(":capture_run_id"), text(captureRunId));
    execute(database_, query, QStringLiteral("Querying frames by capture run failed"));

    std::vector<Frame> frames;
    while (next(query, QStringLiteral("Reading frames-by-capture-run result failed"))) {
        frames.push_back(frameFromQuery(query));
    }
    return frames;
}

} // namespace infrastructure
