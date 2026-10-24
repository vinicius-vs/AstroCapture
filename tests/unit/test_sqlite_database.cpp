#include "infrastructure/persistence/sqlite_database.h"

#include <QtTest>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

using infrastructure::SqliteDatabase;

class SqliteDatabaseTest final : public QObject
{
    Q_OBJECT

private:
    static int scalarInt(const QSqlDatabase& database, const QString& sql)
    {
        QSqlQuery query(database);
        if (!query.exec(sql) || !query.next()) {
            throw std::runtime_error(query.lastError().text().toStdString());
        }
        return query.value(0).toInt();
    }

    static void executeSql(SqliteDatabase& database, const QString& sql)
    {
        QSqlQuery query(database.connection());
        query.prepare(sql);
        database.execute(query);
    }

private slots:
    void initializesVersionedSchemaAutomatically()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString databasePath = QDir(temporaryDirectory.path())
                                         .filePath(QStringLiteral("nested/astrocapture.sqlite3"));

        {
            SqliteDatabase database(databasePath);
            QVERIFY(!database.connection().isOpen());
            database.open();

            QCOMPARE(scalarInt(database.connection(), QStringLiteral("PRAGMA user_version")), 1);
            QCOMPARE(
                scalarInt(
                    database.connection(),
                    QStringLiteral(
                        "SELECT count(*) FROM sqlite_master WHERE type = 'table' "
                        "AND name IN ('targets', 'sessions', 'capture_runs', 'frames')")),
                4);
            QCOMPARE(
                scalarInt(
                    database.connection(),
                    QStringLiteral(
                        "SELECT count(*) FROM sqlite_master WHERE type = 'index' "
                        "AND name IN ('idx_sessions_target_id', 'idx_capture_runs_session_date', "
                        "'idx_frames_capture_run_id', 'idx_frames_state')")),
                4);
            QCOMPARE(scalarInt(database.connection(), QStringLiteral("PRAGMA foreign_keys")), 1);
        }

        SqliteDatabase reopenedDatabase(databasePath);
        reopenedDatabase.open();
        QCOMPARE(scalarInt(reopenedDatabase.connection(), QStringLiteral("PRAGMA user_version")), 1);
    }

    void enforcesDomainConstraintsAndRestrictsReferencedDeletes()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        SqliteDatabase database(
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("database.sqlite3")));
        database.open();

        executeSql(database, QStringLiteral("INSERT INTO targets(id, name) VALUES ('target-1', 'M42')"));
        executeSql(
            database,
            QStringLiteral(
                "INSERT INTO sessions(id, target_id, name, stage, created_at, updated_at) "
                "VALUES ('session-1', 'target-1', 'Orion', 'Capture', "
                "'2026-10-24T12:00:00Z', '2026-10-24T12:00:00Z')"));
        executeSql(
            database,
            QStringLiteral(
                "INSERT INTO capture_runs(id, session_id, acquisition_date, frame_type, "
                "exposure_microseconds, iso, mode, planned_frame_count, state) "
                "VALUES ('run-1', 'session-1', '2026-10-24', 'Light', 5000000, 800, "
                "'Finite', 2, 'Ready')"));

        QVERIFY_EXCEPTION_THROWN(
            executeSql(database, QStringLiteral("DELETE FROM targets WHERE id = 'target-1'")),
            std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(executeSql(
            database,
            QStringLiteral(
                "INSERT INTO capture_runs(id, session_id, acquisition_date, frame_type, "
                "exposure_microseconds, mode, planned_frame_count, state) "
                "VALUES ('run-2', 'session-1', '2026-10-24', 'Light', 5000000, "
                "'Continuous', 1, 'Ready')")), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(executeSql(
            database,
            QStringLiteral(
                "INSERT INTO frames(id, capture_run_id, type, state) "
                "VALUES ('frame-1', 'run-1', 'Light', 'Completed')")), std::runtime_error);
    }

    void rejectsDatabaseSchemasFromNewerVersions()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString databasePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("future.sqlite3"));

        {
            SqliteDatabase database(databasePath);
            database.open();
            QSqlQuery query(database.connection());
            query.prepare(QStringLiteral("PRAGMA user_version = 2"));
            database.execute(query);
        }

        SqliteDatabase database(databasePath);
        QVERIFY_EXCEPTION_THROWN(database.open(), std::runtime_error);
        QVERIFY(!database.connection().isOpen());
    }

    void rollsBackFailedInitialMigration()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString databasePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("conflict.sqlite3"));
        const QString connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);

        {
            QSqlDatabase seed = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            seed.setDatabaseName(databasePath);
            QVERIFY(seed.open());
            QSqlQuery query(seed);
            QVERIFY(query.exec(QStringLiteral("CREATE TABLE targets (id TEXT)")));
            query.clear();
            seed.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        SqliteDatabase database(databasePath);
        QVERIFY_EXCEPTION_THROWN(database.open(), std::runtime_error);
        QVERIFY(!database.connection().isOpen());

        const QString verificationConnectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
        {
            QSqlDatabase verification = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                verificationConnectionName);
            verification.setDatabaseName(databasePath);
            QVERIFY(verification.open());
            QCOMPARE(scalarInt(verification, QStringLiteral("PRAGMA user_version")), 0);
            QCOMPARE(
                scalarInt(
                    verification,
                    QStringLiteral("SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name = 'sessions'")),
                0);
            verification.close();
        }
        QSqlDatabase::removeDatabase(verificationConnectionName);
    }

    void reportsOpenAndMigrationFailures()
    {
        QVERIFY_EXCEPTION_THROWN(SqliteDatabase(QString{}), std::invalid_argument);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        SqliteDatabase database(temporaryDirectory.path());
        QVERIFY_EXCEPTION_THROWN(database.open(), std::runtime_error);
        QVERIFY(!database.connection().isOpen());
    }
};

QTEST_MAIN(SqliteDatabaseTest)

#include "test_sqlite_database.moc"
