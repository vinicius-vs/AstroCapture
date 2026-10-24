#include "domain/capture_entities.h"
#include "infrastructure/storage/storage_layout.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <chrono>
#include <stdexcept>

using namespace std::chrono_literals;
using namespace domain;
using infrastructure::StorageLayout;

class StorageLayoutTest final : public QObject
{
    Q_OBJECT

private:
    static CaptureSettings settings(FrameType frameType = FrameType::Light)
    {
        return {frameType, 5s, 800, CaptureMode::Finite, 2};
    }

private slots:
    void buildsStableDirectoriesFromIdentifiersAndReadableLabels()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const Target target("../one", "M 42 / Órion");
        const Session session(
            "session one",
            target.id(),
            "Observing",
            SessionStage::Capture,
            Timestamp{1s},
            Timestamp{1s});
        const CaptureRun captureRun(
            "run-1",
            session.id(),
            std::chrono::year{2026} / 10 / 24,
            settings());
        const StorageLayout layout(temporaryDirectory.path());

        QCOMPARE(
            layout.targetDirectory(target),
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("targets/%2E%2E%2Fone_M-42-Órion")));
        QCOMPARE(
            layout.sessionDirectory(target, session),
            QDir(temporaryDirectory.path())
                .filePath(QStringLiteral("targets/%2E%2E%2Fone_M-42-Órion/sessions/session%20one")));
        QCOMPARE(
            layout.frameDirectory(target, session, captureRun),
            QDir(temporaryDirectory.path())
                .filePath(QStringLiteral("targets/%2E%2E%2Fone_M-42-Órion/sessions/session%20one/"
                                         "nights/2026-10-24/Light")));

        const QString candidate = layout.frameFilePath(
            target,
            session,
            captureRun,
            "frame/one",
            QStringLiteral("IMG_0001.CR2"));
        QVERIFY(candidate.endsWith(QStringLiteral("/frame%2Fone.CR2")));
        QVERIFY(!QFileInfo::exists(candidate));
    }

    void preservesExtensionsAndAddsNumberedSuffixOnCollision()
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
        const CaptureRun captureRun(
            "run-1",
            session.id(),
            std::chrono::year{2026} / 10 / 24,
            settings(FrameType::DarkFlat));
        const StorageLayout layout(temporaryDirectory.path());

        const QString first = layout.frameFilePath(
            target,
            session,
            captureRun,
            "frame-1",
            QStringLiteral("camera-image.FITS"));
        QVERIFY(first.endsWith(QStringLiteral("/DarkFlat/frame-1.FITS")));
        QVERIFY(QDir().mkpath(QFileInfo(first).absolutePath()));
        QFile firstFile(first);
        QVERIFY(firstFile.open(QIODevice::WriteOnly));
        firstFile.close();

        const QString second = layout.frameFilePath(
            target,
            session,
            captureRun,
            "frame-1",
            QStringLiteral("camera-image.FITS"));
        QVERIFY(second.endsWith(QStringLiteral("/DarkFlat/frame-1_001.FITS")));
        QFile secondFile(second);
        QVERIFY(secondFile.open(QIODevice::WriteOnly));
        secondFile.close();

        const QString third = layout.frameFilePath(
            target,
            session,
            captureRun,
            "frame-1",
            QStringLiteral("camera-image.FITS"));
        QVERIFY(third.endsWith(QStringLiteral("/DarkFlat/frame-1_002.FITS")));
    }

    void doesNotMixTargetsWithTheSameNameAndSupportsMissingExtensions()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const Target firstTarget("target-1", "M42");
        const Target secondTarget("target-2", "M42");
        const Session firstSession(
            "session-1",
            firstTarget.id(),
            "Night",
            SessionStage::Capture,
            Timestamp{1s},
            Timestamp{1s});
        const Session secondSession(
            "session-2",
            secondTarget.id(),
            "Night",
            SessionStage::Capture,
            Timestamp{1s},
            Timestamp{1s});
        const CaptureRun firstRun(
            "run-1",
            firstSession.id(),
            std::chrono::year{2026} / 10 / 24,
            settings());
        const CaptureRun secondRun(
            "run-2",
            secondSession.id(),
            std::chrono::year{2026} / 10 / 24,
            settings());
        const StorageLayout layout(temporaryDirectory.path());

        QVERIFY(layout.targetDirectory(firstTarget) != layout.targetDirectory(secondTarget));
        const QString firstPath = layout.frameFilePath(
            firstTarget,
            firstSession,
            firstRun,
            "frame-1",
            QStringLiteral("camera-image"));
        QVERIFY(firstPath.endsWith(QStringLiteral("/Light/frame-1")));
    }

    void rejectsMismatchedAssociationsAndInvalidInputs()
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
        const Target otherTarget("target-2", "M42");
        const Session otherSession(
            "session-2",
            otherTarget.id(),
            "Other night",
            SessionStage::Capture,
            Timestamp{1s},
            Timestamp{1s});
        const CaptureRun captureRun(
            "run-1",
            session.id(),
            std::chrono::year{2026} / 10 / 24,
            settings());

        QVERIFY_EXCEPTION_THROWN(StorageLayout(QString{}), std::invalid_argument);
        const StorageLayout layout(temporaryDirectory.path());
        QVERIFY_EXCEPTION_THROWN(layout.sessionDirectory(target, otherSession), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(layout.frameDirectory(target, otherSession, captureRun), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(
            layout.frameFilePath(target, session, captureRun, "", QStringLiteral("image.CR2")),
            std::invalid_argument);
    }
};

QTEST_GUILESS_MAIN(StorageLayoutTest)

#include "test_storage_layout.moc"
