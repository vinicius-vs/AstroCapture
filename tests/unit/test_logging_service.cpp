#include "infrastructure/logging/json_lines_logging_service.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <atomic>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace domain;
using infrastructure::JsonLinesLoggingService;

class LoggingServiceTest final : public QObject
{
    Q_OBJECT

private:
    static LogEvent baseEvent()
    {
        LogEvent event;
        event.timestamp = Timestamp{std::chrono::microseconds{1'234'567}};
        event.severity = LogSeverity::Error;
        event.event = QStringLiteral("capture.transfer.failed");
        event.message = QStringLiteral("Image transfer failed");
        event.sessionId = "session-1";
        event.captureRunId = "run-2";
        event.frameId = "frame-3";
        event.operation = QStringLiteral("transfer");
        event.previousState = QStringLiteral("Transferring");
        event.state = QStringLiteral("Failed");
        event.errorCode = QStringLiteral("camera.timeout");
        event.technicalDetails = QStringLiteral("Device did not return data");
        event.relativeFilePath = QStringLiteral("images/targets/M42/frame-3.CR2");
        event.captureSettings = CaptureSettings{
            FrameType::Light,
            5s,
            800,
            CaptureMode::Finite,
            10};
        return event;
    }

    static QList<QJsonObject> readRecords(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            throw std::runtime_error(file.errorString().toStdString());
        }

        QList<QJsonObject> records;
        while (!file.atEnd()) {
            const QByteArray line = file.readLine();
            const QJsonDocument document = QJsonDocument::fromJson(line);
            if (!document.isObject()) {
                throw std::runtime_error("Log line is not a JSON object");
            }
            records.push_back(document.object());
        }
        return records;
    }

private slots:
    void writesStructuredJsonLinesWithCaptureContext()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonLinesLoggingService logger(directory.path());
        const LogEvent event = baseEvent();

        logger.log(event);
        const QList<QJsonObject> records = readRecords(logger.activeLogPath());
        QCOMPARE(records.size(), 1);

        const QJsonObject record = records.front();
        QCOMPARE(record.value(QStringLiteral("timestamp")).toString(), QStringLiteral("1970-01-01T00:00:01.234567Z"));
        QCOMPARE(record.value(QStringLiteral("severity")).toString(), QStringLiteral("ERROR"));
        QCOMPARE(record.value(QStringLiteral("event")).toString(), event.event);
        QCOMPARE(record.value(QStringLiteral("message")).toString(), event.message);
        QCOMPARE(record.value(QStringLiteral("session_id")).toString(), QStringLiteral("session-1"));
        QCOMPARE(record.value(QStringLiteral("capture_run_id")).toString(), QStringLiteral("run-2"));
        QCOMPARE(record.value(QStringLiteral("frame_id")).toString(), QStringLiteral("frame-3"));
        QCOMPARE(record.value(QStringLiteral("operation")).toString(), QStringLiteral("transfer"));
        QCOMPARE(record.value(QStringLiteral("previous_state")).toString(), QStringLiteral("Transferring"));
        QCOMPARE(record.value(QStringLiteral("state")).toString(), QStringLiteral("Failed"));
        QCOMPARE(record.value(QStringLiteral("error_code")).toString(), QStringLiteral("camera.timeout"));
        QCOMPARE(record.value(QStringLiteral("technical_details")).toString(), QStringLiteral("Device did not return data"));
        QCOMPARE(record.value(QStringLiteral("file_path")).toString(), *event.relativeFilePath);

        const QJsonObject settings = record.value(QStringLiteral("capture_settings")).toObject();
        QCOMPARE(settings.value(QStringLiteral("frame_type")).toString(), QStringLiteral("Light"));
        QCOMPARE(settings.value(QStringLiteral("exposure_microseconds")).toInteger(), qint64{5'000'000});
        QCOMPARE(settings.value(QStringLiteral("iso")).toInteger(), qint64{800});
        QCOMPARE(settings.value(QStringLiteral("mode")).toString(), QStringLiteral("Finite"));
        QCOMPARE(settings.value(QStringLiteral("planned_frame_count")).toInteger(), qint64{10});
        QVERIFY(!record.contains(QStringLiteral("environment")));
        QVERIFY(!record.contains(QStringLiteral("payload")));
    }

    void omitsUnspecifiedOptionalFields()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonLinesLoggingService logger(directory.path());
        LogEvent event;
        event.severity = LogSeverity::Info;
        event.event = QStringLiteral("capture.sequence.started");
        event.message = QStringLiteral("Sequence started");

        logger.log(event);
        const QJsonObject record = readRecords(logger.activeLogPath()).front();
        QCOMPARE(record.size(), 4);
        QVERIFY(!record.contains(QStringLiteral("session_id")));
        QVERIFY(!record.contains(QStringLiteral("capture_settings")));
    }

    void rejectsEmptyMessagesAndUnsafeFilePaths()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonLinesLoggingService logger(directory.path());
        LogEvent event = baseEvent();

        event.message.clear();
        QVERIFY_EXCEPTION_THROWN(logger.log(event), std::invalid_argument);

        event = baseEvent();
        event.relativeFilePath = QStringLiteral("../outside/image.CR2");
        QVERIFY_EXCEPTION_THROWN(logger.log(event), std::invalid_argument);

        event = baseEvent();
        event.relativeFilePath = QStringLiteral("/home/user/image.CR2");
        QVERIFY_EXCEPTION_THROWN(logger.log(event), std::invalid_argument);
        QVERIFY(!QFileInfo::exists(logger.activeLogPath()));
    }

    void rotatesAtConfiguredLimitAndRetainsOnlyFiveBackups()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonLinesLoggingService logger(directory.path());
        LogEvent event;
        event.event = QStringLiteral("diagnostics.rotation");
        event.message = QString(1024 * 1024, QLatin1Char('x'));

        for (int index = 0; index < 66; ++index) {
            logger.log(event);
        }

        QVERIFY(QFileInfo(logger.activeLogPath()).size() <= static_cast<qint64>(JsonLinesLoggingService::maximumFileSizeBytes));
        int backupCount = 0;
        for (std::size_t index = 1; index <= JsonLinesLoggingService::retainedFileCount + 1; ++index) {
            const QString backupPath = QStringLiteral("%1.%2").arg(logger.activeLogPath()).arg(index);
            if (QFileInfo::exists(backupPath)) {
                ++backupCount;
                QVERIFY(index <= JsonLinesLoggingService::retainedFileCount);
                QVERIFY(QFileInfo(backupPath).size() <= static_cast<qint64>(JsonLinesLoggingService::maximumFileSizeBytes));
            }
        }
        QCOMPARE(backupCount, static_cast<int>(JsonLinesLoggingService::retainedFileCount));

        event.message = QString(
            static_cast<qsizetype>(JsonLinesLoggingService::maximumFileSizeBytes),
            QLatin1Char('y'));
        QVERIFY_EXCEPTION_THROWN(logger.log(event), std::invalid_argument);
    }

    void serializesConcurrentWritersWithoutCorruptingRecords()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonLinesLoggingService logger(directory.path());
        constexpr int threadCount = 8;
        constexpr int recordsPerThread = 25;
        std::atomic<int> failures{};
        std::vector<std::thread> threads;
        threads.reserve(threadCount);

        for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
            threads.emplace_back([&logger, &failures, threadIndex] {
                for (int recordIndex = 0; recordIndex < recordsPerThread; ++recordIndex) {
                    LogEvent event;
                    event.event = QStringLiteral("concurrency.test");
                    event.message = QStringLiteral("%1:%2").arg(threadIndex).arg(recordIndex);
                    try {
                        logger.log(event);
                    } catch (...) {
                        ++failures;
                    }
                }
            });
        }
        for (std::thread& thread : threads) {
            thread.join();
        }

        QCOMPARE(failures.load(), 0);
        QCOMPARE(readRecords(logger.activeLogPath()).size(), threadCount * recordsPerThread);
    }

    void propagatesLogFileWriteFailures()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString logDirectory = QDir(directory.path()).filePath(QStringLiteral("logs"));
        QVERIFY(QDir().mkpath(QDir(logDirectory).filePath(QStringLiteral("astrocapture.jsonl"))));
        JsonLinesLoggingService logger(directory.path());
        LogEvent event;
        event.event = QStringLiteral("diagnostics.write.failed");
        event.message = QStringLiteral("Should report the storage error");

        QVERIFY_EXCEPTION_THROWN(logger.log(event), std::runtime_error);
    }
};

QTEST_GUILESS_MAIN(LoggingServiceTest)

#include "test_logging_service.moc"
