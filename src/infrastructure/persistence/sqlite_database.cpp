#include "infrastructure/persistence/sqlite_database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <stdexcept>
#include <utility>

namespace infrastructure {
namespace {

constexpr int CurrentSchemaVersion = 2;

void executeSql(QSqlDatabase& database, const QString& sql, const QString& operation)
{
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        throw std::runtime_error(
            QStringLiteral("%1: %2").arg(operation, query.lastError().text()).toStdString());
    }
}

int readSchemaVersion(QSqlDatabase& database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        throw std::runtime_error(
            QStringLiteral("Reading SQLite schema version failed: %1")
                .arg(query.lastError().text())
                .toStdString());
    }
    return query.value(0).toInt();
}

void applyInitialMigration(QSqlDatabase& database)
{
    const QStringList statements{
        QStringLiteral(R"sql(
            CREATE TABLE targets (
                id TEXT PRIMARY KEY NOT NULL CHECK (length(trim(id)) > 0),
                name TEXT NOT NULL CHECK (length(trim(name)) > 0)
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE sessions (
                id TEXT PRIMARY KEY NOT NULL CHECK (length(trim(id)) > 0),
                target_id TEXT NOT NULL,
                name TEXT NOT NULL CHECK (length(trim(name)) > 0),
                stage TEXT NOT NULL CHECK (stage IN ('Capture', 'Stacking', 'Editing', 'Finished')),
                created_at TEXT NOT NULL CHECK (length(trim(created_at)) > 0),
                updated_at TEXT NOT NULL CHECK (length(trim(updated_at)) > 0),
                CHECK (updated_at >= created_at),
                FOREIGN KEY (target_id) REFERENCES targets(id)
                    ON UPDATE RESTRICT ON DELETE RESTRICT
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE capture_runs (
                id TEXT PRIMARY KEY NOT NULL CHECK (length(trim(id)) > 0),
                session_id TEXT NOT NULL,
                acquisition_date TEXT NOT NULL CHECK (length(trim(acquisition_date)) > 0),
                frame_type TEXT NOT NULL CHECK (frame_type IN ('Light', 'Dark', 'Flat', 'Bias', 'DarkFlat')),
                exposure_microseconds INTEGER NOT NULL CHECK (exposure_microseconds > 0),
                iso INTEGER CHECK (iso IS NULL OR iso > 0),
                mode TEXT NOT NULL CHECK (mode IN ('Finite', 'Continuous')),
                planned_frame_count INTEGER,
                state TEXT NOT NULL CHECK (state IN ('Ready', 'Running', 'StopRequested', 'Paused', 'Completed', 'Failed')),
                CHECK (
                    (mode = 'Finite' AND planned_frame_count IS NOT NULL AND planned_frame_count > 0)
                    OR (mode = 'Continuous' AND planned_frame_count IS NULL)
                ),
                FOREIGN KEY (session_id) REFERENCES sessions(id)
                    ON UPDATE RESTRICT ON DELETE RESTRICT
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE frames (
                id TEXT PRIMARY KEY NOT NULL CHECK (length(trim(id)) > 0),
                capture_run_id TEXT NOT NULL,
                type TEXT NOT NULL CHECK (type IN ('Light', 'Dark', 'Flat', 'Bias', 'DarkFlat')),
                state TEXT NOT NULL CHECK (state IN ('Pending', 'Exposing', 'Transferring', 'Validating', 'Completed', 'Failed')),
                exposure_microseconds INTEGER CHECK (exposure_microseconds IS NULL OR exposure_microseconds > 0),
                iso INTEGER CHECK (iso IS NULL OR iso > 0),
                captured_at TEXT,
                file_path TEXT,
                file_size INTEGER,
                error_message TEXT,
                CHECK (
                    (state = 'Pending'
                        AND captured_at IS NULL AND file_path IS NULL AND file_size IS NULL AND error_message IS NULL)
                    OR (state IN ('Exposing', 'Transferring', 'Validating')
                        AND captured_at IS NOT NULL AND length(trim(captured_at)) > 0
                        AND file_path IS NULL AND file_size IS NULL AND error_message IS NULL)
                    OR (state = 'Completed'
                        AND captured_at IS NOT NULL AND length(trim(captured_at)) > 0
                        AND file_path IS NOT NULL AND length(trim(file_path)) > 0
                        AND file_size IS NOT NULL AND file_size > 0
                        AND error_message IS NULL)
                    OR (state = 'Failed'
                        AND captured_at IS NOT NULL AND length(trim(captured_at)) > 0
                        AND file_path IS NULL AND file_size IS NULL
                        AND error_message IS NOT NULL AND length(trim(error_message)) > 0)
                ),
                FOREIGN KEY (capture_run_id) REFERENCES capture_runs(id)
                    ON UPDATE RESTRICT ON DELETE RESTRICT
            )
        )sql"),
        QStringLiteral("CREATE INDEX idx_sessions_target_id ON sessions(target_id)"),
        QStringLiteral("CREATE INDEX idx_capture_runs_session_date ON capture_runs(session_id, acquisition_date)"),
        QStringLiteral("CREATE INDEX idx_frames_capture_run_id ON frames(capture_run_id)"),
        QStringLiteral("CREATE INDEX idx_frames_state ON frames(state)")
    };

    for (const QString& statement : statements) {
        executeSql(database, statement, QStringLiteral("Applying initial SQLite migration failed"));
    }
    executeSql(database, QStringLiteral("PRAGMA user_version = 1"), QStringLiteral("Recording SQLite schema version failed"));
}

void applyOperationalSessionStateMigration(QSqlDatabase& database)
{
    executeSql(
        database,
        QStringLiteral(
            "ALTER TABLE sessions ADD COLUMN is_open INTEGER NOT NULL DEFAULT 0 "
            "CHECK (is_open IN (0, 1))"),
        QStringLiteral("Adding operational session state failed"));
    executeSql(
        database,
        QStringLiteral("CREATE INDEX idx_sessions_open_target ON sessions(is_open, target_id)"),
        QStringLiteral("Creating operational session state index failed"));
    executeSql(
        database,
        QStringLiteral("PRAGMA user_version = 2"),
        QStringLiteral("Recording SQLite schema version failed"));
}

} // namespace

SqliteDatabase::SqliteDatabase(QString filePath)
    : filePath_(std::move(filePath))
    , connectionName_(QStringLiteral("astrocapture-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    if (filePath_.isEmpty()) {
        throw std::invalid_argument("SQLite database file path must not be empty");
    }
}

SqliteDatabase::~SqliteDatabase()
{
    close();
}

void SqliteDatabase::open()
{
    if (opened_) {
        throw std::logic_error("SQLite database is already open");
    }
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        throw std::runtime_error("Qt SQLite driver (QSQLITE) is not available");
    }

    const QString parentPath = QFileInfo(filePath_).absolutePath();
    if (!QDir().mkpath(parentPath)) {
        throw std::runtime_error(
            QStringLiteral("Creating SQLite database directory failed: %1").arg(parentPath).toStdString());
    }

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(filePath_);
    if (!database_.open()) {
        const QString error = database_.lastError().text();
        close();
        throw std::runtime_error(QStringLiteral("Opening SQLite database failed: %1").arg(error).toStdString());
    }
    opened_ = true;

    try {
        executeSql(database_, QStringLiteral("PRAGMA foreign_keys = ON"), QStringLiteral("Enabling SQLite foreign keys failed"));
        migrate();
    } catch (...) {
        close();
        throw;
    }
}

const QSqlDatabase& SqliteDatabase::connection() const noexcept
{
    return database_;
}

void SqliteDatabase::execute(QSqlQuery& query) const
{
    if (!opened_) {
        throw std::logic_error("SQLite database is not open");
    }
    if (!query.exec()) {
        throw std::runtime_error(
            QStringLiteral("Executing SQLite statement failed: %1")
                .arg(query.lastError().text())
                .toStdString());
    }
}

void SqliteDatabase::migrate()
{
    const int version = readSchemaVersion(database_);
    if (version > CurrentSchemaVersion) {
        throw std::runtime_error("SQLite database schema is newer than this application supports");
    }
    for (int migrationVersion = version; migrationVersion < CurrentSchemaVersion; ++migrationVersion) {
        if (migrationVersion != 0 && migrationVersion != 1) {
            throw std::runtime_error("Unsupported SQLite database schema version");
        }

        executeSql(database_, QStringLiteral("BEGIN IMMEDIATE"), QStringLiteral("Starting SQLite migration failed"));
        try {
            if (migrationVersion == 0) {
                applyInitialMigration(database_);
            } else {
                applyOperationalSessionStateMigration(database_);
            }
            executeSql(database_, QStringLiteral("COMMIT"), QStringLiteral("Committing SQLite migration failed"));
        } catch (const std::exception& error) {
            const std::string migrationError = error.what();
            try {
                executeSql(database_, QStringLiteral("ROLLBACK"), QStringLiteral("Rolling back SQLite migration failed"));
            } catch (const std::exception& rollbackError) {
                throw std::runtime_error(migrationError + "; " + rollbackError.what());
            }
            throw;
        }
    }
}

void SqliteDatabase::close()
{
    if (!database_.isValid()) {
        return;
    }
    database_.close();
    database_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName_);
    opened_ = false;
}

} // namespace infrastructure
