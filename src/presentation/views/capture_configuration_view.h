#pragma once

#include "domain/camera.h"
#include "domain/capture_entities.h"

#include <QScrollArea>
#include <QString>

#include <chrono>
#include <cstdint>
#include <optional>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;

namespace presentation {

class CaptureConfigurationView final : public QScrollArea
{
    Q_OBJECT

public:
    explicit CaptureConfigurationView(QWidget* parent = nullptr);

    void setCapabilities(
        const domain::CameraCapabilities& capabilities,
        const domain::CameraSettings& currentSettings);

    void setLiveViewStatusText(const QString& text);
    void updateLiveViewControls(
        bool available,
        bool active,
        bool pending,
        bool captureRunning);

    void setValidationError(const QString& error);
    void setCaptureStatus(const QString& status, const QString& color = QStringLiteral("#49e3b6"));
    void setSequenceMetrics(const QString& metrics);
    void setElapsedTime(const QString& elapsed);
    void setStatistics(const QString& statistics);
    void setPreviewImage(const QImage& image);
    void setPreviewStatus(const QString& status);
    void clearPreview();

    void setCaptureStarted(const QString& exposureText);
    void updateProgress(
        quint32 completed,
        quint32 planned,
        quint32 failed,
        const QString& exposureText,
        const QString& details,
        qint64 elapsedMilliseconds);
    void setCaptureFinished();

signals:
    void startCaptureRequested(domain::CaptureSettings settings);
    void stopCaptureRequested();
    void backRequested();
    void startLiveViewRequested();
    void stopLiveViewRequested();

private:
    void onStartCaptureClicked();

    QComboBox* cameraInput_{};
    QLabel* cameraStatusLabel_{};
    QComboBox* frameTypeInput_{};
    QComboBox* modeInput_{};
    QDoubleSpinBox* exposureInput_{};
    QComboBox* isoInput_{};
    QSpinBox* countInput_{};
    QLabel* errorLabel_{};
    QLabel* captureStatusLabel_{};
    QProgressBar* captureProgressBar_{};
    QLabel* captureSequenceMetricsLabel_{};
    QLabel* captureElapsedLabel_{};
    QLabel* captureStatisticsLabel_{};
    QLabel* liveViewStatusLabel_{};
    QPushButton* liveViewStartButton_{};
    QPushButton* liveViewStopButton_{};
    QLabel* capturePreviewImageLabel_{};
    QLabel* capturePreviewStatusLabel_{};
    QPushButton* captureStartButton_{};
    QPushButton* captureStopButton_{};
    QPushButton* captureBackButton_{};
    bool exposureAvailable_{true};
};

} // namespace presentation
