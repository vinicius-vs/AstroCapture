#pragma once

#include "domain/camera.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QString>

#include <atomic>
#include <memory>
#include <string>

class QCloseEvent;
class QPushButton;
class QStackedWidget;
class QThread;
class QTimer;
class QWidget;

namespace application {
class CaptureConfigurationService;
class CapturePreviewService;
class SessionRecoveryService;
class SessionService;
}

namespace domain {
class CaptureRun;
struct SessionRecord;
}

namespace presentation {
class CaptureConfigurationView;
class HomeView;
class SessionsView;
}

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(
        application::SessionService& sessionService,
        application::CaptureConfigurationService& captureConfigurationService,
        domain::ICamera& camera,
        QString applicationDataDirectory,
        application::SessionRecoveryService* sessionRecoveryService = nullptr,
        application::CapturePreviewService* capturePreviewService = nullptr,
        QWidget* parent = nullptr);

private:
    void closeEvent(QCloseEvent* event) override;
    void openSessionConfigurationDialog();
    void showCaptureConfigurationPage(const std::string& sessionId, bool returnHome);
    void startCapture(domain::CaptureRun captureRun);
    void inspectSessionRecovery(const domain::SessionRecord& session);
    void loadCapturePreview(const std::string& sessionId);
    void decodeCapturePreview(const QString& imagePath);
    void beginLiveViewOperation(bool start);
    void updateLiveViewControls();
    void requestCaptureStop();
    void openSession(const std::string& sessionId);
    void showSessionInformation(const std::string& sessionId);
    void deleteSession(const std::string& sessionId);
    void showHomePage();
    void showSessionsPage();
    void setNavigationSelection(bool homeSelected);

    application::SessionService& sessionService_;
    application::CaptureConfigurationService& captureConfigurationService_;
    domain::ICamera& camera_;
    QString applicationDataDirectory_;
    application::SessionRecoveryService* sessionRecoveryService_{};
    application::CapturePreviewService* capturePreviewService_{};

    QStackedWidget* pages_{};
    presentation::HomeView* homeView_{};
    presentation::SessionsView* sessionsView_{};
    presentation::CaptureConfigurationView* captureConfigurationView_{};

    QPushButton* homeNavigationButton_{};
    QPushButton* libraryNavigationButton_{};

    QThread* captureThread_{};
    QTimer* captureUiTimer_{};
    QElapsedTimer captureElapsedTimer_;
    QString captureExposureText_;
    QString liveViewStatusText_;
    std::shared_ptr<std::atomic_bool> captureStopRequested_;
    std::string activeSessionId_;
    quint64 previewRequestId_{};
    bool liveViewAvailable_{};
    bool liveViewActive_{};
    bool liveViewOperationPending_{};
    bool closeAfterCapture_{};
    bool closeAfterLiveViewStop_{};
};
