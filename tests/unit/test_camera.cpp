#include "domain/camera.h"
#include "infrastructure/camera/simulated_camera.h"

#include <QtTest>

#include <algorithm>
#include <sstream>
#include <vector>

using namespace domain;
using namespace infrastructure;
using namespace std::chrono_literals;

namespace {

class TestCamera final : public ICamera
{
public:
    CameraOperationResult connect() override
    {
        return std::monostate{};
    }

    CameraOperationResult disconnect() override
    {
        return std::monostate{};
    }

    CameraResult<CameraCapabilities> capabilities() const override
    {
        CameraCapabilities result;
        result.exposure.state = CapabilityState::Supported;
        result.exposure.minimum = 1ms;
        result.exposure.maximum = 30s;
        result.iso.state = CapabilityState::ReadOnly;
        result.liveView = CapabilityState::Unavailable;
        return result;
    }

    CameraResult<CameraSettings> settings() const override
    {
        return CameraSettings{5s, 800};
    }

    CameraOperationResult applySettings(const CameraSettings&) override
    {
        return std::monostate{};
    }

    CameraResult<CameraCapture> capture(const CaptureRequest&) override
    {
        return CameraCapture{"capture-1", ".CR2", std::nullopt};
    }

    CameraOperationResult transfer(
        const CameraCapture&,
        std::ostream&,
        TransferProgressCallback) override
    {
        return std::monostate{};
    }

    CameraOperationResult startLiveView() override
    {
        return CameraError{
            CameraErrorCode::UnsupportedOperation,
            "Live view is unavailable",
            "startLiveView",
            std::nullopt
        };
    }

    CameraOperationResult stopLiveView() override
    {
        return std::monostate{};
    }
};

} // namespace

class CameraContractTest final : public QObject
{
    Q_OBJECT

private slots:
    void capabilitiesDistinguishAccessAndUnavailableValues()
    {
        TestCamera camera;
        const auto result = camera.capabilities();
        QVERIFY(std::holds_alternative<CameraCapabilities>(result));

        const auto& capabilities = std::get<CameraCapabilities>(result);
        QCOMPARE(capabilities.exposure.state, CapabilityState::Supported);
        QCOMPARE(capabilities.exposure.minimum, std::optional(1ms));
        QCOMPARE(capabilities.exposure.maximum, std::optional(30s));
        QCOMPARE(capabilities.iso.state, CapabilityState::ReadOnly);
        QCOMPARE(capabilities.liveView, CapabilityState::Unavailable);

        const SettingCapability<unsigned int> unavailable;
        QCOMPARE(unavailable.state, CapabilityState::Unavailable);
        QVERIFY(!unavailable.minimum.has_value());
        QVERIFY(!unavailable.maximum.has_value());
        QVERIFY(unavailable.values.empty());
    }

    void cameraContractReturnsCaptureMetadataAndStructuredErrors()
    {
        TestCamera camera;

        const auto capture = camera.capture(CaptureRequest{CameraSettings{5s, 800}});
        QVERIFY(std::holds_alternative<CameraCapture>(capture));
        QCOMPARE(std::get<CameraCapture>(capture).fileExtension, std::string(".CR2"));

        const auto liveView = camera.startLiveView();
        QVERIFY(std::holds_alternative<CameraError>(liveView));
        const auto& error = std::get<CameraError>(liveView);
        QCOMPARE(error.code, CameraErrorCode::UnsupportedOperation);
        QCOMPARE(error.operation, std::string("startLiveView"));
        QVERIFY(!error.message.empty());
    }

    void simulatedCameraRequiresConnectionAndReportsCapabilities()
    {
        SimulatedCamera camera;

        const auto settingsBeforeConnect = camera.settings();
        QVERIFY(std::holds_alternative<CameraError>(settingsBeforeConnect));
        QCOMPARE(
            std::get<CameraError>(settingsBeforeConnect).code,
            CameraErrorCode::NotConnected);
        const auto connection = camera.connect();
        QVERIFY(std::holds_alternative<std::monostate>(connection));

        const auto capabilitiesResult = camera.capabilities();
        QVERIFY(std::holds_alternative<CameraCapabilities>(capabilitiesResult));
        const CameraCapabilities& capabilities =
            std::get<CameraCapabilities>(capabilitiesResult);
        QCOMPARE(capabilities.exposure.state, CapabilityState::Supported);
        QCOMPARE(capabilities.exposure.minimum, std::optional(1ms));
        QCOMPARE(capabilities.exposure.maximum, std::optional(30min));
        QCOMPARE(capabilities.iso.state, CapabilityState::Supported);
        QCOMPARE(
            capabilities.iso.values,
            (std::vector<unsigned int>{100, 200, 400, 800, 1600, 3200, 6400}));
        QCOMPARE(capabilities.liveView, CapabilityState::Unavailable);

        const auto initialSettings = camera.settings();
        QVERIFY(std::holds_alternative<CameraSettings>(initialSettings));
        QCOMPARE(std::get<CameraSettings>(initialSettings).exposure, std::optional(5s));
        QCOMPARE(std::get<CameraSettings>(initialSettings).iso, std::optional(800U));
    }

    void simulatedCameraAppliesOnlySupportedSettings()
    {
        SimulatedCamera camera;
        camera.connect();

        QVERIFY(std::holds_alternative<std::monostate>(camera.applySettings({2s, 1600})));
        const auto updatedSettings = camera.settings();
        QVERIFY(std::holds_alternative<CameraSettings>(updatedSettings));
        QCOMPARE(std::get<CameraSettings>(updatedSettings).exposure, std::optional(2s));
        QCOMPARE(std::get<CameraSettings>(updatedSettings).iso, std::optional(1600U));

        const auto badExposure = camera.applySettings({31min, std::nullopt});
        QVERIFY(std::holds_alternative<CameraError>(badExposure));
        QCOMPARE(std::get<CameraError>(badExposure).code, CameraErrorCode::InvalidSettings);
        const auto badIso = camera.applySettings({std::nullopt, 125});
        QVERIFY(std::holds_alternative<CameraError>(badIso));
        QCOMPARE(std::get<CameraError>(badIso).code, CameraErrorCode::InvalidSettings);

        const auto unchangedSettings = camera.settings();
        QCOMPARE(std::get<CameraSettings>(unchangedSettings).exposure, std::optional(2s));
        QCOMPARE(std::get<CameraSettings>(unchangedSettings).iso, std::optional(1600U));
    }

    void simulatedCameraCapturesAndTransfersDeterministicTestImage()
    {
        SimulatedCamera camera;
        camera.connect();

        const auto captureResult = camera.capture(CaptureRequest{CameraSettings{3s, 400}});
        QVERIFY(std::holds_alternative<CameraCapture>(captureResult));
        const CameraCapture capture = std::get<CameraCapture>(captureResult);
        QVERIFY(capture.id.starts_with("simulated-capture-"));
        QCOMPARE(capture.fileExtension, std::string(".pgm"));
        QVERIFY(capture.expectedSize.has_value());

        std::ostringstream destination(std::ios::binary);
        std::optional<TransferProgress> reportedProgress;
        const auto transferResult = camera.transfer(
            capture,
            destination,
            [&reportedProgress](const TransferProgress& progress) {
                reportedProgress = progress;
            });
        QVERIFY(std::holds_alternative<std::monostate>(transferResult));
        const std::string image = destination.str();
        QVERIFY(image.starts_with("P5\n8 8\n255\n"));
        QCOMPARE(image.size(), static_cast<std::size_t>(*capture.expectedSize));
        QVERIFY(reportedProgress.has_value());
        QCOMPARE(reportedProgress->bytesTransferred, *capture.expectedSize);
        QCOMPARE(reportedProgress->totalBytes, capture.expectedSize);

        CameraCapture invalidCapture = capture;
        invalidCapture.expectedSize = *capture.expectedSize + 1;
        std::ostringstream invalidDestination(std::ios::binary);
        const auto invalidTransfer = camera.transfer(invalidCapture, invalidDestination);
        QVERIFY(std::holds_alternative<CameraError>(invalidTransfer));
        QCOMPARE(std::get<CameraError>(invalidTransfer).code, CameraErrorCode::TransferFailed);
        QVERIFY(invalidDestination.str().empty());

        const auto unsupportedLiveView = camera.startLiveView();
        QVERIFY(std::holds_alternative<CameraError>(unsupportedLiveView));
        QCOMPARE(
            std::get<CameraError>(unsupportedLiveView).code,
            CameraErrorCode::UnsupportedOperation);

        camera.disconnect();
        const auto disconnectedCapture = camera.capture({});
        QVERIFY(std::holds_alternative<CameraError>(disconnectedCapture));
        QCOMPARE(
            std::get<CameraError>(disconnectedCapture).code,
            CameraErrorCode::NotConnected);
    }

    void configuredExposureFailureIsReproducibleAndOneShot()
    {
        SimulatedCameraOptions options;
        options.failures.failExposureOnCall = 1;
        SimulatedCamera firstCamera(options);
        SimulatedCamera secondCamera(options);
        firstCamera.connect();
        secondCamera.connect();

        const auto firstFailure = firstCamera.capture(CaptureRequest{CameraSettings{1ms, 800}});
        const auto repeatedFailure = secondCamera.capture(
            CaptureRequest{CameraSettings{1ms, 800}});
        QVERIFY(std::holds_alternative<CameraError>(firstFailure));
        QVERIFY(std::holds_alternative<CameraError>(repeatedFailure));
        QCOMPARE(
            std::get<CameraError>(firstFailure).code,
            CameraErrorCode::CaptureFailed);
        QCOMPARE(
            std::get<CameraError>(firstFailure).message,
            std::get<CameraError>(repeatedFailure).message);

        const auto retry = firstCamera.capture(CaptureRequest{CameraSettings{1ms, 800}});
        QVERIFY(std::holds_alternative<CameraCapture>(retry));
    }

    void configuredExposureDelayIsApplied()
    {
        SimulatedCameraOptions options;
        options.exposureDelay = 10ms;
        SimulatedCamera camera(options);
        camera.connect();

        const auto start = std::chrono::steady_clock::now();
        const auto result = camera.capture(CaptureRequest{CameraSettings{1ms, 800}});
        const auto elapsed = std::chrono::steady_clock::now() - start;

        QVERIFY(std::holds_alternative<CameraCapture>(result));
        QVERIFY(elapsed >= 8ms);
    }

    void realisticTransferReportsProgressForEachChunkAndCanFailAfterBytes()
    {
        SimulatedCameraOptions options;
        options.transferChunkSize = 10;
        options.transferChunkDelay = 1ms;
        options.failures.failTransferOnCall = 1;
        options.failures.failTransferAfterBytes = 20;
        SimulatedCamera camera(options);
        camera.connect();
        const auto captureResult = camera.capture(CaptureRequest{CameraSettings{1ms, 800}});
        QVERIFY(std::holds_alternative<CameraCapture>(captureResult));

        std::vector<TransferProgress> updates;
        std::ostringstream destination(std::ios::binary);
        const auto result = camera.transfer(
            std::get<CameraCapture>(captureResult),
            destination,
            [&updates](const TransferProgress& update) {
                updates.push_back(update);
            });

        QVERIFY(std::holds_alternative<CameraError>(result));
        QCOMPARE(std::get<CameraError>(result).code, CameraErrorCode::TransferFailed);
        QCOMPARE(destination.str().size(), std::size_t{20});
        QCOMPARE(updates.size(), std::size_t{2});
        QCOMPARE(updates[0].bytesTransferred, std::uint64_t{10});
        QCOMPARE(updates[1].bytesTransferred, std::uint64_t{20});
        QCOMPARE(updates[1].totalBytes, std::get<CameraCapture>(captureResult).expectedSize);
    }

    void configuredDisconnectionAndInvalidFileAreReported()
    {
        SimulatedCameraOptions disconnectOptions;
        disconnectOptions.failures.disconnectOnTransferCall = 1;
        disconnectOptions.failures.failTransferAfterBytes = 12;
        SimulatedCamera disconnectedCamera(disconnectOptions);
        disconnectedCamera.connect();
        const auto captureResult =
            disconnectedCamera.capture(CaptureRequest{CameraSettings{1ms, 800}});
        QVERIFY(std::holds_alternative<CameraCapture>(captureResult));

        std::ostringstream destination(std::ios::binary);
        const auto transferResult = disconnectedCamera.transfer(
            std::get<CameraCapture>(captureResult),
            destination);
        QVERIFY(std::holds_alternative<CameraError>(transferResult));
        QCOMPARE(
            std::get<CameraError>(transferResult).code,
            CameraErrorCode::DeviceDisconnected);
        QCOMPARE(destination.str().size(), std::size_t{12});
        QVERIFY(std::holds_alternative<CameraError>(disconnectedCamera.settings()));
        QCOMPARE(
            std::get<CameraError>(disconnectedCamera.settings()).code,
            CameraErrorCode::NotConnected);

        SimulatedCameraOptions invalidFileOptions;
        invalidFileOptions.failures.invalidFileOnCaptureCall = 1;
        SimulatedCamera invalidFileCamera(invalidFileOptions);
        invalidFileCamera.connect();
        const auto invalidCapture =
            invalidFileCamera.capture(CaptureRequest{CameraSettings{1ms, 800}});
        QVERIFY(std::holds_alternative<CameraCapture>(invalidCapture));
        QVERIFY(
            std::get<CameraCapture>(invalidCapture).fileExtension != std::string(".pgm"));
    }

    void simulatedCameraRejectsInvalidFailurePlans()
    {
        SimulatedCameraOptions invalidCall;
        invalidCall.failures.failExposureOnCall = 0;
        QVERIFY_EXCEPTION_THROWN(
            static_cast<void>(SimulatedCamera{invalidCall}),
            std::invalid_argument);

        SimulatedCameraOptions conflictingCalls;
        conflictingCalls.failures.failTransferOnCall = 1;
        conflictingCalls.failures.disconnectOnTransferCall = 1;
        QVERIFY_EXCEPTION_THROWN(
            static_cast<void>(SimulatedCamera{conflictingCalls}),
            std::invalid_argument);
    }
};

QTEST_APPLESS_MAIN(CameraContractTest)

#include "test_camera.moc"
