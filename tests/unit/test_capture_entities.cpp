#include "domain/capture_entities.h"
#include "domain/camera.h"

#include <QtTest>

using namespace std::chrono_literals;
using namespace domain;

class CaptureEntitiesTest final : public QObject
{
    Q_OBJECT

private:
    static CaptureSettings finiteSettings()
    {
        return {
            FrameType::Light,
            5s,
            800,
            CaptureMode::Finite,
            3
        };
    }

private slots:
    void targetRequiresIdentifierAndName()
    {
        const Target target("target-1", "M42");
        QCOMPARE(target.id(), std::string("target-1"));
        QCOMPARE(target.name(), std::string("M42"));

        QVERIFY_EXCEPTION_THROWN(Target("", "M42"), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(Target("target-1", ""), std::invalid_argument);
    }

    void sessionTracksWorkflowStageAndRejectsInvalidDates()
    {
        const Timestamp createdAt{1s};
        Session session("session-1", "target-1", "Orion", SessionStage::Capture, createdAt, createdAt);

        session.setStage(SessionStage::Stacking, createdAt + 1s);
        QCOMPARE(session.stage(), SessionStage::Stacking);
        QCOMPARE(session.updatedAt(), createdAt + 1s);
        QCOMPARE(session.targetId(), std::string("target-1"));

        QVERIFY_EXCEPTION_THROWN(
            session.setStage(SessionStage::Editing, createdAt),
            std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(
            Session("session-2", "target-1", "Orion", SessionStage::Capture, createdAt + 1s, createdAt),
            std::invalid_argument);
    }

    void captureSettingsEnforceFiniteAndContinuousCounts()
    {
        finiteSettings().validate();

        CaptureSettings continuous{
            FrameType::Dark,
            1s,
            std::nullopt,
            CaptureMode::Continuous,
            std::nullopt
        };
        continuous.validate();

        CaptureSettings finiteWithoutCount{
            FrameType::Light,
            1s,
            std::nullopt,
            CaptureMode::Finite,
            std::nullopt
        };
        QVERIFY_EXCEPTION_THROWN(finiteWithoutCount.validate(), std::invalid_argument);

        CaptureSettings continuousWithCount{
            FrameType::Light,
            1s,
            std::nullopt,
            CaptureMode::Continuous,
            2
        };
        QVERIFY_EXCEPTION_THROWN(continuousWithCount.validate(), std::invalid_argument);
    }

    void captureSettingsValidateAgainstCameraCapabilities()
    {
        const CaptureSettings settings = finiteSettings();
        CameraCapabilities capabilities;
        capabilities.exposure.state = CapabilityState::Supported;
        capabilities.exposure.minimum = 1s;
        capabilities.exposure.maximum = 30s;
        capabilities.iso.state = CapabilityState::Supported;
        capabilities.iso.values = {100, 400, 800};

        settings.validate(capabilities, CameraSettings{5s, 800});

        CameraCapabilities unsupportedExposure = capabilities;
        unsupportedExposure.exposure.state = CapabilityState::Unavailable;
        QVERIFY_EXCEPTION_THROWN(
            settings.validate(unsupportedExposure, CameraSettings{5s, 800}),
            std::invalid_argument);

        CameraCapabilities readOnlyExposure = capabilities;
        readOnlyExposure.exposure.state = CapabilityState::ReadOnly;
        settings.validate(readOnlyExposure, CameraSettings{5s, 800});
        QVERIFY_EXCEPTION_THROWN(
            settings.validate(readOnlyExposure, CameraSettings{10s, 800}),
            std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(
            settings.validate(readOnlyExposure, CameraSettings{std::nullopt, 800}),
            std::invalid_argument);

        const CaptureSettings unsupportedExposureValue{
            FrameType::Light,
            31s,
            800,
            CaptureMode::Finite,
            3
        };
        QVERIFY_EXCEPTION_THROWN(
            unsupportedExposureValue.validate(capabilities, CameraSettings{5s, 800}),
            std::invalid_argument);
        const CaptureSettings unsupportedIsoValue{
            FrameType::Light,
            5s,
            200,
            CaptureMode::Finite,
            3
        };
        QVERIFY_EXCEPTION_THROWN(
            unsupportedIsoValue.validate(capabilities, CameraSettings{5s, 800}),
            std::invalid_argument);

        CameraCapabilities unsupportedIso = capabilities;
        unsupportedIso.iso.state = CapabilityState::Unavailable;
        const CaptureSettings withoutIso{
            FrameType::Dark,
            5s,
            std::nullopt,
            CaptureMode::Finite,
            1
        };
        withoutIso.validate(unsupportedIso, CameraSettings{5s, std::nullopt});
        QVERIFY_EXCEPTION_THROWN(
            settings.validate(unsupportedIso, CameraSettings{5s, 800}),
            std::invalid_argument);
    }

    void captureRunOnlyAllowsDocumentedTransitions()
    {
        CaptureRun run(
            "run-1",
            "session-1",
            std::chrono::year{2026} / 10 / 18,
            finiteSettings());

        QCOMPARE(run.state(), CaptureRunState::Ready);
        QVERIFY_EXCEPTION_THROWN(run.requestStop(), std::logic_error);
        QVERIFY_EXCEPTION_THROWN(run.pause(), std::logic_error);
        QVERIFY_EXCEPTION_THROWN(run.resume(), std::logic_error);
        QVERIFY_EXCEPTION_THROWN(run.complete(), std::logic_error);
        QVERIFY_EXCEPTION_THROWN(run.fail(), std::logic_error);

        run.start();
        QVERIFY_EXCEPTION_THROWN(run.start(), std::logic_error);
        run.requestStop();
        QVERIFY_EXCEPTION_THROWN(run.requestStop(), std::logic_error);
        run.pause();
        QCOMPARE(run.state(), CaptureRunState::Paused);
        QVERIFY_EXCEPTION_THROWN(run.pause(), std::logic_error);
        run.resume();
        QVERIFY_EXCEPTION_THROWN(run.resume(), std::logic_error);
        run.complete();
        QCOMPARE(run.state(), CaptureRunState::Completed);
        QVERIFY_EXCEPTION_THROWN(run.start(), std::logic_error);
    }

    void captureRunRejectsInvalidDateAndConfiguration()
    {
        QVERIFY_EXCEPTION_THROWN(
            CaptureRun("run-1", "session-1", std::chrono::year{2026} / 2 / 30, finiteSettings()),
            std::invalid_argument);

        CaptureSettings invalidSettings{
            FrameType::Light,
            0us,
            std::nullopt,
            CaptureMode::Finite,
            1
        };
        QVERIFY_EXCEPTION_THROWN(
            CaptureRun("run-2", "session-1", std::chrono::year{2026} / 10 / 18, invalidSettings),
            std::invalid_argument);
    }

    void captureRunCanRestorePersistedState()
    {
        const auto run = CaptureRun::restore(
            "run-1",
            "session-1",
            std::chrono::year{2026} / 10 / 18,
            finiteSettings(),
            CaptureRunState::Paused);
        QCOMPARE(run.state(), CaptureRunState::Paused);

        QVERIFY_EXCEPTION_THROWN(
            CaptureRun::restore(
                "run-2",
                "session-1",
                std::chrono::year{2026} / 10 / 18,
                finiteSettings(),
                static_cast<CaptureRunState>(99)),
            std::invalid_argument);

        const auto completedContinuousRun = CaptureRun::restore(
                "run-3",
                "session-1",
                std::chrono::year{2026} / 10 / 18,
                CaptureSettings{
                    FrameType::Light,
                    1s,
                    std::nullopt,
                    CaptureMode::Continuous,
                    std::nullopt
                },
                CaptureRunState::Completed);
        QCOMPARE(completedContinuousRun.state(), CaptureRunState::Completed);
    }

    void continuousCaptureRunCannotBeCompletedWithoutACompletionCondition()
    {
        CaptureRun run(
            "run-1",
            "session-1",
            std::chrono::year{2026} / 10 / 18,
            CaptureSettings{
                FrameType::Light,
                1s,
                std::nullopt,
                CaptureMode::Continuous,
                std::nullopt
            });
        run.start();

        QVERIFY_EXCEPTION_THROWN(run.complete(), std::logic_error);
        QCOMPARE(run.state(), CaptureRunState::Running);
    }

    void captureRunMayFinishAfterSafeStopRequest()
    {
        CaptureRun run(
            "run-2",
            "session-1",
            std::chrono::year{2026} / 10 / 18,
            CaptureSettings{
                FrameType::Light,
                1s,
                std::nullopt,
                CaptureMode::Continuous,
                std::nullopt
            });
        run.start();
        run.requestStop();
        run.complete();

        QCOMPARE(run.state(), CaptureRunState::Completed);

        CaptureRun finishedRun(
            "run-3",
            "session-1",
            std::chrono::year{2026} / 10 / 18,
            CaptureSettings{
                FrameType::Light,
                1s,
                std::nullopt,
                CaptureMode::Finite,
                1
            });
        finishedRun.start();
        finishedRun.pause();
        finishedRun.complete();
        QCOMPARE(finishedRun.state(), CaptureRunState::Completed);
    }

    void frameMustFinishTransferAndValidationBeforeCompletion()
    {
        Frame frame(
            "frame-1",
            "run-1",
            FrameType::Light,
            5s,
            800);
        const Timestamp capturedAt{2s};

        QVERIFY_EXCEPTION_THROWN(frame.startValidation(), std::logic_error);
        frame.startExposure(capturedAt);
        frame.startTransfer();
        frame.startValidation();
        QVERIFY_EXCEPTION_THROWN(
            frame.completeValidation("images/frame.cr2", 0),
            std::invalid_argument);
        frame.completeValidation("images/frame.cr2", 1024);

        QCOMPARE(frame.state(), FrameState::Completed);
        QCOMPARE(frame.capturedAt(), std::optional<Timestamp>(capturedAt));
        QCOMPARE(frame.filePath(), std::optional<std::filesystem::path>("images/frame.cr2"));
        QCOMPARE(frame.fileSize(), std::optional<std::uint64_t>(1024));
    }

    void frameCanRestorePersistedStateAndRejectInconsistentData()
    {
        const Timestamp capturedAt{2s};
        auto frame = Frame::restore(
            "frame-1",
            "run-1",
            FrameType::Light,
            FrameState::Completed,
            5s,
            800,
            capturedAt,
            std::filesystem::path("images/frame.cr2"),
            1024,
            std::nullopt);

        QCOMPARE(frame.state(), FrameState::Completed);
        QVERIFY_EXCEPTION_THROWN(
            Frame::restore(
                "frame-2",
                "run-1",
                FrameType::Light,
                FrameState::Completed,
                5s,
                800,
                capturedAt,
                std::nullopt,
                std::nullopt,
                std::nullopt),
            std::invalid_argument);
    }

    void failedFrameCanBeRetriedWithoutStaleCaptureData()
    {
        Frame frame("frame-1", "run-1", FrameType::Dark);
        frame.startExposure(Timestamp{3s});
        frame.fail("camera transfer failed");

        QCOMPARE(frame.state(), FrameState::Failed);
        QVERIFY(frame.errorMessage().has_value());
        QVERIFY_EXCEPTION_THROWN(frame.fail("again"), std::logic_error);

        frame.retry();
        QCOMPARE(frame.state(), FrameState::Pending);
        QVERIFY(!frame.capturedAt().has_value());
        QVERIFY(!frame.errorMessage().has_value());
    }

    void frameRejectsInvalidTransitionsAndOnlyRetriesFailures()
    {
        Frame frame("frame-1", "run-1", FrameType::Light);

        QVERIFY_EXCEPTION_THROWN(frame.startTransfer(), std::logic_error);
        QVERIFY_EXCEPTION_THROWN(frame.startValidation(), std::logic_error);
        QVERIFY_EXCEPTION_THROWN(frame.fail("failure"), std::logic_error);
        QVERIFY_EXCEPTION_THROWN(frame.retry(), std::logic_error);

        frame.startExposure(Timestamp{1s});
        frame.startTransfer();
        frame.startValidation();
        frame.completeValidation("images/frame.cr2", 1024);

        QVERIFY_EXCEPTION_THROWN(frame.fail("failure"), std::logic_error);
        QVERIFY_EXCEPTION_THROWN(frame.retry(), std::logic_error);
        QCOMPARE(frame.state(), FrameState::Completed);
    }
};

QTEST_MAIN(CaptureEntitiesTest)

#include "test_capture_entities.moc"
