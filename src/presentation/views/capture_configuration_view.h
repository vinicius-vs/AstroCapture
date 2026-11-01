#pragma once

#include "domain/camera.h"
#include "domain/capture_entities.h"

#include <QImage>
#include <QScrollArea>
#include <QString>

#include <chrono>
#include <cstdint>
#include <optional>

class QBoxLayout;
class QButtonGroup;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QResizeEvent;
class QVBoxLayout;

namespace presentation {

class CaptureConfigurationView final : public QScrollArea
{
    Q_OBJECT

public:
    explicit CaptureConfigurationView(QWidget* parent = nullptr);

    void setCapabilities(
        const domain::CameraCapabilities& capabilities,
        const domain::CameraSettings& currentSettings);
    void setCameraConnectionStatus(const QString& text, bool connected);
    void setBackButtonText(const QString& text);
    void setCaptureStartButtonText(const QString& text);
    void setLiveViewStatusText(const QString& text);
    void updateLiveViewControls(
        bool available,
        bool active,
        bool pending,
        bool captureRunning,
        bool reconnectingCamera = false);

    void setValidationError(const QString& error);
    void setCaptureStatus(const QString& status, const QString& color = QStringLiteral("#49e3b6"));
    void setStatistics(const QString& statistics);
    void setPreviewImage(const QImage& image);
    void setPreviewMetadata(const QString& metadata);
    void setPreviewStatus(const QString& status);
    void clearPreview();

    void setCaptureStarted();
    void setCaptureFinished();

signals:
    void startCaptureRequested(domain::CaptureSettings settings);
    void stopCaptureRequested();
    void backRequested();
    void reconnectCameraRequested();
    void startLiveViewRequested();
    void stopLiveViewRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void onStartCaptureClicked();
    void updatePreviewPixmap();

    QVBoxLayout* pageLayout_{};
    QBoxLayout* contentColumnsLayout_{};
    QComboBox* cameraInput_{};
    QLabel* cameraIndicatorLabel_{};
    QLabel* cameraStatusLabel_{};
    QLabel* cameraDetailsLabel_{};
    QPushButton* reconnectCameraButton_{};
    QComboBox* frameTypeInput_{};
    QButtonGroup* frameTypeButtons_{};
    QComboBox* modeInput_{};
    QRadioButton* finiteModeButton_{};
    QRadioButton* continuousModeButton_{};
    QDoubleSpinBox* exposureInput_{};
    QComboBox* isoInput_{};
    QComboBox* apertureInput_{};
    QSpinBox* countInput_{};
    QFrame* captureFeedbackCard_{};
    QFrame* sessionStatisticsCard_{};
    QLabel* errorLabel_{};
    QLabel* captureStatusLabel_{};
    QLabel* captureStatisticsLabel_{};
    QLabel* liveViewStatusLabel_{};
    QLabel* liveViewImageLabel_{};
    QPushButton* liveViewStartButton_{};
    QPushButton* liveViewStopButton_{};
    QLabel* capturePreviewImageLabel_{};
    QLabel* capturePreviewMetadataLabel_{};
    QLabel* capturePreviewStatusLabel_{};
    QPushButton* captureStartButton_{};
    QPushButton* captureStopButton_{};
    QPushButton* captureBackButton_{};
    QPushButton* stackingButton_{};
    QImage previewImage_;
    bool exposureAvailable_{true};
    bool cameraConnected_{};
    bool liveViewAvailable_{};
    bool liveViewActive_{};
};

} // namespace presentation
