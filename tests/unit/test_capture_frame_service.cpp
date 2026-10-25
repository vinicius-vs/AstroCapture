#include "application/capture_frame_service.h"
#include "application/capture_sequence_service.h"
#include "domain/repositories/repository_errors.h"
#include "infrastructure/camera/simulated_camera.h"
#include "infrastructure/persistence/sqlite_database.h"
#include "infrastructure/persistence/sqlite_repositories.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <chrono>
#include <algorithm>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

using namespace std::chrono_literals;
using namespace application;
using namespace domain;
using namespace infrastructure;

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

class TransferFailingCamera final : public ICamera
{
public:
    explicit TransferFailingCamera(std::size_t failOnTransfer = 1)
        : failOnTransfer_(failOnTransfer)
    {
    }

    CameraOperationResult connect() override
    {
        return camera_.connect();
    }

    CameraOperationResult disconnect() override
    {
        return camera_.disconnect();
    }

    CameraResult<CameraCapabilities> capabilities() const override
    {
        return camera_.capabilities();
    }

    CameraResult<CameraSettings> settings() const override
    {
        return camera_.settings();
    }

    CameraOperationResult applySettings(const CameraSettings& settings) override
    {
        return camera_.applySettings(settings);
    }

    CameraResult<CameraCapture> capture(const CaptureRequest& request) override
    {
        return camera_.capture(request);
    }

    CameraOperationResult transfer(
        const CameraCapture& capture,
        std::ostream& destination,
        TransferProgressCallback progress) override
    {
        ++transferCount_;
        if (transferCount_ == failOnTransfer_) {
            return CameraError{
                CameraErrorCode::TransferFailed,
                "Injected transfer failure",
                "transfer",
                "TransferFailingCamera"
            };
        }
        return camera_.transfer(capture, destination, std::move(progress));
    }

    CameraOperationResult startLiveView() override
    {
        return camera_.startLiveView();
    }

    CameraOperationResult stopLiveView() override
    {
        return camera_.stopLiveView();
    }

private:
    SimulatedCamera camera_;
    std::size_t failOnTransfer_;
    std::size_t transferCount_{0};
};

class ApplyFailOnceCamera final : public ICamera
{
public:
    CameraOperationResult connect() override
    {
        return camera_.connect();
    }

    CameraOperationResult disconnect() override
    {
        return camera_.disconnect();
    }

    CameraResult<CameraCapabilities> capabilities() const override
    {
        return camera_.capabilities();
    }

    CameraResult<CameraSettings> settings() const override
    {
        return camera_.settings();
    }

    CameraOperationResult applySettings(const CameraSettings& settings) override
    {
        if (failNextApply_) {
            failNextApply_ = false;
            return CameraError{
                CameraErrorCode::InvalidSettings,
                "Injected settings failure",
                "applySettings",
                "ApplyFailOnceCamera"
            };
        }
        return camera_.applySettings(settings);
    }

    CameraResult<CameraCapture> capture(const CaptureRequest& request) override
    {
        return camera_.capture(request);
    }

    CameraOperationResult transfer(
        const CameraCapture& capture,
        std::ostream& destination,
        TransferProgressCallback progress) override
    {
        return camera_.transfer(capture, destination, std::move(progress));
    }

    CameraOperationResult startLiveView() override
    {
        return camera_.startLiveView();
    }

    CameraOperationResult stopLiveView() override
    {
        return camera_.stopLiveView();
    }

private:
    SimulatedCamera camera_;
    bool failNextApply_{true};
};

class ThrowingCaptureCamera final : public ICamera
{
public:
    CameraOperationResult connect() override { return camera_.connect(); }
    CameraOperationResult disconnect() override { return camera_.disconnect(); }
    CameraResult<CameraCapabilities> capabilities() const override { return camera_.capabilities(); }
    CameraResult<CameraSettings> settings() const override { return camera_.settings(); }
    CameraOperationResult applySettings(const CameraSettings& settings) override
    {
        return camera_.applySettings(settings);
    }
    CameraResult<CameraCapture> capture(const CaptureRequest&) override
    {
        throw std::runtime_error("Exposure outcome is unknown");
    }
    CameraOperationResult transfer(
        const CameraCapture& capture,
        std::ostream& destination,
        TransferProgressCallback progress) override
    {
        return camera_.transfer(capture, destination, std::move(progress));
    }
    CameraOperationResult startLiveView() override { return camera_.startLiveView(); }
    CameraOperationResult stopLiveView() override { return camera_.stopLiveView(); }

private:
    SimulatedCamera camera_;
};

struct CaptureFixture
{
    explicit CaptureFixture(
        const QString& rootPath,
        CaptureSettings settings = {
            FrameType::Light,
            5s,
            800,
            CaptureMode::Finite,
            1
        },
        bool startRun = true)
        : database(QDir(rootPath).filePath(QStringLiteral("capture.sqlite3")))
        , session("session-1", target.id(), "Observation", SessionStage::Capture, Timestamp{1s}, Timestamp{1s})
        , captureRun("run-1", session.id(), std::chrono::year{2026} / 10 / 25, settings)
        , sessions(database)
        , runs(database)
        , frames(database)
        , imageStorage(StorageLayout(QDir(rootPath).filePath(QStringLiteral("images"))))
    {
        database.open();
        sessions.create(target, session);
        if (startRun) {
            captureRun.start();
        }
        runs.create(captureRun);
    }

    Target target{"target-1", "M42"};
    SqliteDatabase database;
    Session session;
    CaptureRun captureRun;
    SqliteSessionRepository sessions;
    SqliteCaptureRunRepository runs;
    SqliteFrameRepository frames;
    ImageStorage imageStorage;
};

} // namespace

class CaptureFrameServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void simulatedCaptureStoresImageAndCompletedMetadata()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(directory.path());
        SimulatedCamera camera;
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService service(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);

        std::vector<CaptureFrameProgress> progress;
        const Frame frame = service.execute(
            "frame-1",
            [&progress](const CaptureFrameProgress& update) {
                progress.push_back(update);
            });

        QCOMPARE(frame.state(), FrameState::Completed);
        QCOMPARE(frame.type(), FrameType::Light);
        QVERIFY(frame.filePath().has_value());
        QVERIFY(frame.filePath()->is_absolute());
        QVERIFY(frame.fileSize().has_value());
        const auto persistedFrame = fixture.frames.findById(frame.id());
        QVERIFY(persistedFrame.has_value());
        QCOMPARE(persistedFrame->state(), FrameState::Completed);
        QCOMPARE(persistedFrame->filePath(), frame.filePath());
        QCOMPARE(persistedFrame->fileSize(), frame.fileSize());

        const QString filePath = QString::fromLocal8Bit(frame.filePath()->c_str());
        QFile image(filePath);
        QVERIFY(image.open(QIODevice::ReadOnly));
        const QByteArray bytes = image.readAll();
        QVERIFY(bytes.startsWith("P5\n8 8\n255\n"));
        QCOMPARE(static_cast<std::uint64_t>(bytes.size()), *frame.fileSize());

        QVERIFY(!progress.empty());
        QCOMPARE(progress.front().state, FrameState::Pending);
        QCOMPARE(progress.back().state, FrameState::Completed);
        bool reportedTransferProgress = false;
        std::uint64_t lastTransferredBytes = 0;
        for (const CaptureFrameProgress& update : progress) {
            if (update.transfer) {
                reportedTransferProgress = true;
                QCOMPARE(update.state, FrameState::Transferring);
                QVERIFY(update.transfer->bytesTransferred > lastTransferredBytes);
                lastTransferredBytes = update.transfer->bytesTransferred;
            }
        }
        QVERIFY(reportedTransferProgress);
        QCOMPARE(lastTransferredBytes, *frame.fileSize());
        QCOMPARE(logging.events.size(), std::size_t{6});
        QCOMPARE(logging.events.back().state, std::optional<QString>{QStringLiteral("Completed")});
    }

    void transferFailurePersistsFailedFrameAndNeverReportsSuccess()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(directory.path());
        TransferFailingCamera camera;
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService service(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);

        bool caughtFailure = false;
        try {
            service.execute("frame-failed");
        } catch (const CaptureFrameError& error) {
            caughtFailure = true;
            QCOMPARE(error.stage(), CaptureFrameStage::Transfer);
            QCOMPARE(error.frameId(), FrameId("frame-failed"));
            QVERIFY(error.cameraError().has_value());
            QCOMPARE(error.cameraError()->code, CameraErrorCode::TransferFailed);
            QVERIFY(!error.recoveryOptions().empty());
            QVERIFY(std::find(
                        error.recoveryOptions().begin(),
                        error.recoveryOptions().end(),
                        CaptureRecoveryDecision::Retry)
                    == error.recoveryOptions().end());
        }
        QVERIFY(caughtFailure);

        const auto failedFrame = fixture.frames.findById("frame-failed");
        QVERIFY(failedFrame.has_value());
        QCOMPARE(failedFrame->state(), FrameState::Failed);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Paused);
        QVERIFY_EXCEPTION_THROWN(
            service.applyRecoveryDecision("frame-failed", CaptureRecoveryDecision::Retry),
            std::logic_error);
        QVERIFY(failedFrame->errorMessage().has_value());
        QVERIFY(!failedFrame->filePath().has_value());
        QCOMPARE(logging.events.back().severity, LogSeverity::Error);
        QCOMPARE(logging.events.back().state, std::optional<QString>{QStringLiteral("Failed")});
    }

    void unsupportedSettingsAreRejectedBeforeCreatingAFrame()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(
            directory.path(),
            CaptureSettings{FrameType::Dark, 31min, 800, CaptureMode::Finite, 1});
        SimulatedCamera camera;
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService service(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);

        QVERIFY_EXCEPTION_THROWN(
            service.execute("invalid-frame"),
            CaptureFrameError);
        QVERIFY(!fixture.frames.findById("invalid-frame").has_value());
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Paused);
    }

    void safePreExposureFailureRequiresExplicitRetryAndRecordsAttempts()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(directory.path());
        ApplyFailOnceCamera camera;
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService service(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);

        bool caughtFailure = false;
        try {
            service.execute("frame-retry");
        } catch (const CaptureFrameError& error) {
            caughtFailure = true;
            QCOMPARE(error.stage(), CaptureFrameStage::ApplySettings);
            QVERIFY(std::find(
                        error.recoveryOptions().begin(),
                        error.recoveryOptions().end(),
                        CaptureRecoveryDecision::Retry)
                    != error.recoveryOptions().end());
        }
        QVERIFY(caughtFailure);
        QCOMPARE(fixture.frames.findById("frame-retry")->state(), FrameState::Pending);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Paused);

        service.applyRecoveryDecision("frame-retry", CaptureRecoveryDecision::Retry);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Running);
        const Frame completedFrame = service.execute("frame-retry");
        QCOMPARE(completedFrame.state(), FrameState::Completed);
        QCOMPARE(
            std::count_if(
                logging.events.begin(),
                logging.events.end(),
                [](const LogEvent& event) {
                    return event.event == QStringLiteral("capture.frame.attempt_started");
                }),
            2);
        QCOMPARE(
            std::count_if(
                logging.events.begin(),
                logging.events.end(),
                [](const LogEvent& event) {
                    return event.event == QStringLiteral("capture.recovery.decision");
                }),
            1);
    }

    void recoveryCanPauseOrAbortWithoutRetryingUncertainFrame()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(directory.path());
        TransferFailingCamera camera;
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService service(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);

        QVERIFY_EXCEPTION_THROWN(service.execute("frame-abort"), CaptureFrameError);
        service.applyRecoveryDecision("frame-abort", CaptureRecoveryDecision::Pause);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Paused);
        service.applyRecoveryDecision("frame-abort", CaptureRecoveryDecision::Abort);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Failed);
        QCOMPARE(fixture.captureRun.state(), CaptureRunState::Failed);
    }

    void thrownExposureFailureIsRecordedAsInconclusiveAndCannotBeRetried()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(directory.path());
        ThrowingCaptureCamera camera;
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService service(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);

        bool caughtFailure = false;
        try {
            service.execute("frame-unknown");
        } catch (const CaptureFrameError& error) {
            caughtFailure = true;
            QCOMPARE(error.stage(), CaptureFrameStage::Exposure);
            QVERIFY(error.cameraError().has_value());
            QCOMPARE(error.cameraError()->code, CameraErrorCode::Unknown);
            QVERIFY(std::find(
                        error.recoveryOptions().begin(),
                        error.recoveryOptions().end(),
                        CaptureRecoveryDecision::Retry)
                    == error.recoveryOptions().end());
        }
        QVERIFY(caughtFailure);
        QCOMPARE(fixture.frames.findById("frame-unknown")->state(), FrameState::Failed);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Paused);
        QVERIFY_EXCEPTION_THROWN(
            service.applyRecoveryDecision("frame-unknown", CaptureRecoveryDecision::Retry),
            std::logic_error);
    }

    void finiteSequenceCompletesPlannedFramesAndReportsConfirmedProgress()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(
            directory.path(),
            CaptureSettings{FrameType::Light, 5s, 800, CaptureMode::Finite, 3},
            false);
        SimulatedCamera camera;
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService frameService(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);
        CaptureSequenceService sequenceService(
            frameService,
            fixture.runs,
            fixture.frames,
            fixture.captureRun);

        std::vector<CaptureSequenceProgress> progress;
        const std::vector<Frame> frames = sequenceService.execute(
            [&progress](const CaptureSequenceProgress& update) {
                progress.push_back(update);
            });

        QCOMPARE(frames.size(), std::size_t{3});
        QCOMPARE(fixture.captureRun.state(), CaptureRunState::Completed);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Completed);
        QCOMPARE(fixture.frames.findByCaptureRunId("run-1").size(), std::size_t{3});
        for (const Frame& frame : frames) {
            QCOMPARE(frame.state(), FrameState::Completed);
            QVERIFY(frame.filePath().has_value());
            QVERIFY(frame.fileSize().has_value());
        }

        QVERIFY(!progress.empty());
        QCOMPARE(progress.front().completedFrames, std::uint32_t{0});
        QCOMPARE(progress.back().completedFrames, std::uint32_t{3});
        for (std::size_t index = 1; index < progress.size(); ++index) {
            QVERIFY(progress[index].completedFrames >= progress[index - 1].completedFrames);
        }
        QCOMPARE(
            std::count_if(
                progress.begin(),
                progress.end(),
                [](const CaptureSequenceProgress& update) {
                    return update.currentFrame
                        && update.currentFrame->state == FrameState::Completed;
                }),
            3);
    }

    void sequenceFailurePreservesCompletedFramesAndConfirmedProgress()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(
            directory.path(),
            CaptureSettings{FrameType::Light, 5s, 800, CaptureMode::Finite, 3});
        TransferFailingCamera camera(2);
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService frameService(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);
        CaptureSequenceService sequenceService(
            frameService,
            fixture.runs,
            fixture.frames,
            fixture.captureRun);

        std::vector<CaptureSequenceProgress> progress;
        QVERIFY_EXCEPTION_THROWN(
            sequenceService.execute([&progress](const CaptureSequenceProgress& update) {
                progress.push_back(update);
            }),
            CaptureFrameError);

        const std::vector<Frame> persistedFrames =
            fixture.frames.findByCaptureRunId("run-1");
        QCOMPARE(persistedFrames.size(), std::size_t{2});
        QCOMPARE(
            std::count_if(
                persistedFrames.begin(),
                persistedFrames.end(),
                [](const Frame& frame) {
                    return frame.state() == FrameState::Completed;
                }),
            1);
        QCOMPARE(
            std::count_if(
                persistedFrames.begin(),
                persistedFrames.end(),
                [](const Frame& frame) {
                    return frame.state() == FrameState::Failed;
                }),
            1);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Paused);
        QCOMPARE(fixture.captureRun.state(), CaptureRunState::Paused);
        QVERIFY(!progress.empty());
        QCOMPARE(progress.back().completedFrames, std::uint32_t{1});
        for (const CaptureSequenceProgress& update : progress) {
            QVERIFY(update.completedFrames <= 1);
        }
    }

    void sequenceRetryUsesSameFrameAfterSafePreExposureFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(
            directory.path(),
            CaptureSettings{FrameType::Light, 5s, 800, CaptureMode::Finite, 2});
        ApplyFailOnceCamera camera;
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService frameService(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);
        CaptureSequenceService sequenceService(
            frameService,
            fixture.runs,
            fixture.frames,
            fixture.captureRun);

        std::optional<FrameId> failedFrameId;
        try {
            sequenceService.execute();
        } catch (const CaptureFrameError& error) {
            failedFrameId = error.frameId();
            QVERIFY(std::find(
                        error.recoveryOptions().begin(),
                        error.recoveryOptions().end(),
                        CaptureRecoveryDecision::Retry)
                    != error.recoveryOptions().end());
        }
        QVERIFY(failedFrameId.has_value());
        QCOMPARE(
            fixture.frames.findById(*failedFrameId)->state(),
            FrameState::Pending);

        frameService.applyRecoveryDecision(
            *failedFrameId,
            CaptureRecoveryDecision::Retry);
        const std::vector<Frame> completedFrames = sequenceService.execute();

        QCOMPARE(completedFrames.size(), std::size_t{2});
        QCOMPARE(
            fixture.frames.findById(*failedFrameId)->state(),
            FrameState::Completed);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Completed);
    }

    void simulatedSafeFailureCanPauseAndResumeSequence()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(
            directory.path(),
            CaptureSettings{FrameType::Light, 5s, 800, CaptureMode::Finite, 2});
        SimulatedCameraOptions cameraOptions;
        cameraOptions.failures.failApplySettingsOnCall = 1;
        SimulatedCamera camera(cameraOptions);
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService frameService(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);
        CaptureSequenceService sequenceService(
            frameService,
            fixture.runs,
            fixture.frames,
            fixture.captureRun);

        std::optional<FrameId> failedFrameId;
        try {
            sequenceService.execute();
        } catch (const CaptureFrameError& error) {
            failedFrameId = error.frameId();
            QCOMPARE(error.stage(), CaptureFrameStage::ApplySettings);
            QVERIFY(std::find(
                        error.recoveryOptions().begin(),
                        error.recoveryOptions().end(),
                        CaptureRecoveryDecision::Retry)
                    != error.recoveryOptions().end());
        }
        QVERIFY(failedFrameId.has_value());
        QCOMPARE(fixture.captureRun.state(), CaptureRunState::Paused);
        QCOMPARE(fixture.frames.findById(*failedFrameId)->state(), FrameState::Pending);

        frameService.applyRecoveryDecision(
            *failedFrameId,
            CaptureRecoveryDecision::Retry);
        const std::vector<Frame> completedFrames = sequenceService.execute();

        QCOMPARE(completedFrames.size(), std::size_t{2});
        QCOMPARE(fixture.captureRun.state(), CaptureRunState::Completed);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Completed);
    }

    void simulatedInvalidFileIsRejectedWithoutPublishingAnImage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(directory.path());
        SimulatedCameraOptions cameraOptions;
        cameraOptions.failures.invalidFileOnCaptureCall = 1;
        SimulatedCamera camera(cameraOptions);
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService frameService(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);

        bool rejectedInvalidFile = false;
        try {
            frameService.execute("frame-invalid-file");
        } catch (const CaptureFrameError& error) {
            rejectedInvalidFile = true;
            QCOMPARE(error.stage(), CaptureFrameStage::Exposure);
        }
        QVERIFY(rejectedInvalidFile);

        const auto frame = fixture.frames.findById("frame-invalid-file");
        QVERIFY(frame.has_value());
        QCOMPARE(frame->state(), FrameState::Failed);
        QVERIFY(!frame->filePath().has_value());
        QVERIFY(fixture.runs.findById("run-1")->state() == CaptureRunState::Paused);
    }

    void continuousStopFinishesCurrentFrameAndDoesNotScheduleAnother()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(
            directory.path(),
            CaptureSettings{FrameType::Light, 5s, 800, CaptureMode::Continuous, std::nullopt});
        SimulatedCamera camera;
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService frameService(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);
        CaptureSequenceService sequenceService(
            frameService,
            fixture.runs,
            fixture.frames,
            fixture.captureRun);

        std::vector<CaptureSequenceProgress> progress;
        const std::vector<Frame> completedFrames = sequenceService.execute(
            [&sequenceService, &progress](const CaptureSequenceProgress& update) {
                progress.push_back(update);
                if (update.currentFrame
                    && update.currentFrame->state == FrameState::Exposing) {
                    sequenceService.requestStop();
                }
            });

        QCOMPARE(completedFrames.size(), std::size_t{1});
        QCOMPARE(completedFrames.front().state(), FrameState::Completed);
        QCOMPARE(fixture.frames.findByCaptureRunId("run-1").size(), std::size_t{1});
        QCOMPARE(fixture.captureRun.state(), CaptureRunState::Paused);
        QCOMPARE(fixture.runs.findById("run-1")->state(), CaptureRunState::Paused);
        QVERIFY(!progress.empty());
        QVERIFY(!progress.back().plannedFrameCount.has_value());
        QVERIFY(std::any_of(
            progress.begin(),
            progress.end(),
            [](const CaptureSequenceProgress& update) {
                return update.currentFrame
                    && update.currentFrame->state == FrameState::Completed
                    && update.completedFrames == 1;
            }));
    }

    void stopBeforeExposureLeavesFramePendingAndDoesNotCallCamera()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CaptureFixture fixture(
            directory.path(),
            CaptureSettings{FrameType::Light, 5s, 800, CaptureMode::Continuous, std::nullopt});
        SimulatedCamera camera;
        QVERIFY(std::holds_alternative<std::monostate>(camera.connect()));
        RecordingLoggingService logging;
        CaptureFrameService frameService(
            camera,
            fixture.runs,
            fixture.frames,
            fixture.imageStorage,
            logging,
            fixture.target,
            fixture.session,
            fixture.captureRun);
        CaptureSequenceService sequenceService(
            frameService,
            fixture.runs,
            fixture.frames,
            fixture.captureRun);

        const std::vector<Frame> completedFrames = sequenceService.execute(
            [&sequenceService](const CaptureSequenceProgress& update) {
                if (update.currentFrame
                    && update.currentFrame->state == FrameState::Pending) {
                    sequenceService.requestStop();
                }
            });

        QVERIFY(completedFrames.empty());
        const std::vector<Frame> frames = fixture.frames.findByCaptureRunId("run-1");
        QCOMPARE(frames.size(), std::size_t{1});
        QCOMPARE(frames.front().state(), FrameState::Pending);
        QCOMPARE(fixture.captureRun.state(), CaptureRunState::Paused);
        QVERIFY(std::none_of(
            logging.events.begin(),
            logging.events.end(),
            [](const LogEvent& event) {
                return event.event == QStringLiteral("capture.frame.attempt_started");
            }));
    }
};

QTEST_GUILESS_MAIN(CaptureFrameServiceTest)

#include "test_capture_frame_service.moc"
