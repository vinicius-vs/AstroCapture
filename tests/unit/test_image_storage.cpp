#include "domain/capture_entities.h"
#include "infrastructure/storage/image_storage.h"

#include <QtTest>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <algorithm>
#include <chrono>
#include <utility>

using namespace std::chrono_literals;
using namespace domain;
using namespace infrastructure;

namespace {

CaptureSettings lightSettings()
{
    return {FrameType::Light, 5s, 800, CaptureMode::Finite, 1};
}

Frame validatingFrame(const CaptureRun& run, FrameId id = "frame-1")
{
    Frame frame(std::move(id), run.id(), FrameType::Light);
    frame.startExposure(Timestamp{1s});
    frame.startTransfer();
    frame.startValidation();
    return frame;
}

class FailedReadDevice final : public QIODevice
{
public:
    bool openForReading()
    {
        return open(QIODevice::ReadOnly);
    }

protected:
    qint64 readData(char* data, qint64 maxSize) override
    {
        if (!sent_ && maxSize > 0) {
            data[0] = 'x';
            sent_ = true;
            return 1;
        }
        setErrorString(QStringLiteral("simulated source read failure"));
        return -1;
    }

    qint64 bytesAvailable() const override
    {
        return QIODevice::bytesAvailable() + 1;
    }

    qint64 writeData(const char*, qint64) override
    {
        return -1;
    }

private:
    bool sent_{false};
};

class DelayedSequentialDevice final : public QIODevice
{
public:
    explicit DelayedSequentialDevice(QByteArray data)
        : data_(std::move(data))
    {
    }

    bool openForReading()
    {
        return open(QIODevice::ReadOnly);
    }

    bool isSequential() const override
    {
        return true;
    }

protected:
    qint64 readData(char* data, qint64 maxSize) override
    {
        if (!ready_ || offset_ == data_.size()) {
            return 0;
        }
        const qint64 count = std::min(maxSize, data_.size() - offset_);
        std::copy_n(data_.constData() + offset_, count, data);
        offset_ += count;
        return count;
    }

    qint64 writeData(const char*, qint64) override
    {
        return -1;
    }

    bool waitForReadyRead(int) override
    {
        if (!ready_) {
            ready_ = true;
            return true;
        }
        return false;
    }

    qint64 bytesAvailable() const override
    {
        return (ready_ ? data_.size() - offset_ : 0) + QIODevice::bytesAvailable();
    }

private:
    QByteArray data_;
    qint64 offset_{0};
    bool ready_{false};
};

} // namespace

class ImageStorageTest final : public QObject
{
    Q_OBJECT

private:
    static QString frameDirectory(
        const QString& root,
        const Target& target,
        const Session& session,
        const CaptureRun& run)
    {
        return StorageLayout(root).frameDirectory(target, session, run);
    }

private slots:
    void preservesBytesAndReturnsPathOnlyAfterSuccessfulStorage()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const Target target("target-1", "M42");
        const Session session(
            "session-1",
            target.id(),
            "Night",
            SessionStage::Capture,
            Timestamp{1s},
            Timestamp{1s});
        const CaptureRun run(
            "run-1", session.id(), std::chrono::year{2026} / 10 / 24, lightSettings());
        Frame frame = validatingFrame(run);
        const QByteArray original = QByteArray::fromHex("000102feff415354524f");
        QBuffer source;
        source.setData(original);
        QVERIFY(source.open(QIODevice::ReadOnly));
        const ImageStorage storage{StorageLayout(temporaryDirectory.path())};

        const StoredImage stored = storage.store(
            target, session, run, frame, QStringLiteral("camera.CR2"), source, original.size());

        QCOMPARE(stored.fileSize, static_cast<std::uint64_t>(original.size()));
        QVERIFY(QFileInfo(stored.filePath).isAbsolute());
        QVERIFY(stored.filePath.endsWith(QStringLiteral("/frame-1.CR2")));
        QFile result(stored.filePath);
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(result.readAll(), original);
        QCOMPARE(frame.state(), FrameState::Validating);
        QVERIFY(!frame.filePath());

        frame.completeValidation(stored.filePath.toStdString(), stored.fileSize);
        QCOMPARE(frame.state(), FrameState::Completed);
        QCOMPARE(frame.fileSize(), std::optional<std::uint64_t>{original.size()});
    }

    void selectsNumberedNameWithoutOverwritingExistingFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const Target target("target-1", "M42");
        const Session session(
            "session-1",
            target.id(),
            "Night",
            SessionStage::Capture,
            Timestamp{1s},
            Timestamp{1s});
        const CaptureRun run(
            "run-1", session.id(), std::chrono::year{2026} / 10 / 24, lightSettings());
        const StorageLayout layout(temporaryDirectory.path());
        const QString existingPath = layout.frameFilePath(
            target, session, run, "frame-1", QStringLiteral("camera.CR2"));
        QVERIFY(QDir().mkpath(QFileInfo(existingPath).absolutePath()));
        QFile existing(existingPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("old"), qint64{3});
        existing.close();

        Frame frame = validatingFrame(run);
        QBuffer source;
        source.setData("new");
        QVERIFY(source.open(QIODevice::ReadOnly));
        const ImageStorage storage{layout};
        const StoredImage stored = storage.store(
            target, session, run, frame, QStringLiteral("camera.CR2"), source);

        QVERIFY(stored.filePath.endsWith(QStringLiteral("/frame-1_001.CR2")));
        QVERIFY(existing.open(QIODevice::ReadOnly));
        QCOMPARE(existing.readAll(), QByteArray("old"));
    }

    void waitsForAndStreamsSequentialInputUntilEof()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const Target target("target-1", "M42");
        const Session session(
            "session-1",
            target.id(),
            "Night",
            SessionStage::Capture,
            Timestamp{1s},
            Timestamp{1s});
        const CaptureRun run(
            "run-1", session.id(), std::chrono::year{2026} / 10 / 24, lightSettings());
        const QByteArray original(100'000, 'r');
        DelayedSequentialDevice source(original);
        QVERIFY(source.openForReading());
        Frame frame = validatingFrame(run);
        const ImageStorage storage{StorageLayout(temporaryDirectory.path())};

        const StoredImage stored = storage.store(
            target, session, run, frame, QStringLiteral("stream.CR2"), source, original.size());

        QCOMPARE(stored.fileSize, static_cast<std::uint64_t>(original.size()));
        QFile result(stored.filePath);
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(result.readAll(), original);
    }

    void rejectsEmptyAndIncompleteSourcesAndRemovesTemporaryFiles()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const Target target("target-1", "M42");
        const Session session(
            "session-1",
            target.id(),
            "Night",
            SessionStage::Capture,
            Timestamp{1s},
            Timestamp{1s});
        const CaptureRun run(
            "run-1", session.id(), std::chrono::year{2026} / 10 / 24, lightSettings());
        const ImageStorage storage{StorageLayout(temporaryDirectory.path())};
        Frame emptyFrame = validatingFrame(run, "empty");
        QBuffer emptySource;
        QVERIFY(emptySource.open(QIODevice::ReadOnly));
        QVERIFY_EXCEPTION_THROWN(
            storage.store(target, session, run, emptyFrame, QStringLiteral("empty.CR2"), emptySource),
            ImageStorageError);

        Frame shortFrame = validatingFrame(run, "short");
        QBuffer shortSource;
        shortSource.setData("short");
        QVERIFY(shortSource.open(QIODevice::ReadOnly));
        QVERIFY_EXCEPTION_THROWN(
            storage.store(
                target,
                session,
                run,
                shortFrame,
                QStringLiteral("short.CR2"),
                shortSource,
                6),
            ImageStorageError);

        Frame largeFrame = validatingFrame(run, "large");
        QBuffer largeSource;
        largeSource.setData("longer");
        QVERIFY(largeSource.open(QIODevice::ReadOnly));
        QVERIFY_EXCEPTION_THROWN(
            storage.store(
                target,
                session,
                run,
                largeFrame,
                QStringLiteral("large.CR2"),
                largeSource,
                5),
            ImageStorageError);

        Frame failedFrame = validatingFrame(run, "failed");
        FailedReadDevice failedSource;
        QVERIFY(failedSource.openForReading());
        try {
            storage.store(
                target,
                session,
                run,
                failedFrame,
                QStringLiteral("failed.CR2"),
                failedSource);
            QFAIL("Expected a source read error");
        } catch (const ImageStorageError& error) {
            QCOMPARE(error.operation(), ImageStorageOperation::ReadSource);
        }

        const QString directory = frameDirectory(temporaryDirectory.path(), target, session, run);
        const QStringList files = QDir(directory).entryList(
            QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
        QVERIFY(files.isEmpty());
    }

    void rejectsInvalidFrameStateAndDirectoryFailures()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const Target target("target-1", "M42");
        const Session session(
            "session-1",
            target.id(),
            "Night",
            SessionStage::Capture,
            Timestamp{1s},
            Timestamp{1s});
        const CaptureRun run(
            "run-1", session.id(), std::chrono::year{2026} / 10 / 24, lightSettings());
        const ImageStorage storage{StorageLayout(temporaryDirectory.path())};
        Frame frame("frame-invalid", run.id(), FrameType::Light);
        QBuffer source;
        source.setData("image");
        QVERIFY(source.open(QIODevice::ReadOnly));
        QVERIFY_EXCEPTION_THROWN(
            storage.store(target, session, run, frame, QStringLiteral("image.CR2"), source),
            std::logic_error);

        const QString blocker = QDir(temporaryDirectory.path()).filePath(QStringLiteral("not-a-directory"));
        QFile file(blocker);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const ImageStorage blockedStorage{StorageLayout(blocker)};
        Frame blockedFrame = validatingFrame(run, "blocked");
        QBuffer blockedSource;
        blockedSource.setData("image");
        QVERIFY(blockedSource.open(QIODevice::ReadOnly));
        try {
            blockedStorage.store(
                target,
                session,
                run,
                blockedFrame,
                QStringLiteral("image.CR2"),
                blockedSource);
            QFAIL("Expected a storage error");
        } catch (const ImageStorageError& error) {
            QCOMPARE(error.operation(), ImageStorageOperation::CreateDirectory);
        }
    }
};

QTEST_GUILESS_MAIN(ImageStorageTest)

#include "test_image_storage.moc"
