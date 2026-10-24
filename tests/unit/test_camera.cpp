#include "domain/camera.h"

#include <QtTest>

using namespace domain;
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
};

QTEST_APPLESS_MAIN(CameraContractTest)

#include "test_camera.moc"
