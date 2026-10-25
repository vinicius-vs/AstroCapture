#pragma once

#include "domain/camera.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QString>

#include <atomic>
#include <memory>
#include <string>

class QCloseEvent;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTimer;
class QThread;
class QStackedWidget;
class QWidget;

namespace application {
class CaptureConfigurationService;
class SessionRecoveryService;
class SessionService;
}

namespace domain {
class CaptureRun;
struct SessionRecord;
}

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(
        application::SessionService& sessionService,
        application::CaptureConfigurationService& captureConfigurationService,
        domain::ICamera& camera,
        QString applicationDataDirectory,
        application::SessionRecoveryService* sessionRecoveryService = nullptr,
        QWidget* parent = nullptr);

private:
    void closeEvent(QCloseEvent* event) override;
    QWidget* createHomePage();
    QWidget* createSessionsPage();
    void openSessionConfigurationDialog();
    void showCaptureConfigurationPage(const std::string& sessionId, bool returnHome);
    void startCapture(domain::CaptureRun captureRun);
    void inspectSessionRecovery(const domain::SessionRecord& session);
    void requestCaptureStop();
    void openSession(const std::string& sessionId);
    void showSessionInformation(const std::string& sessionId);
    void deleteSession(const std::string& sessionId);
    void showHomePage();
    void showSessionsPage();
    void setNavigationSelection(bool homeSelected);
    void refreshSessions();

    application::SessionService& sessionService_;
    application::CaptureConfigurationService& captureConfigurationService_;
    domain::ICamera& camera_;
    QString applicationDataDirectory_;
    application::SessionRecoveryService* sessionRecoveryService_{};
    QStackedWidget* pages_{};
    QWidget* homePage_{};
    QWidget* sessionsPage_{};
    QWidget* captureConfigurationPage_{};
    QPushButton* homeNavigationButton_{};
    QPushButton* libraryNavigationButton_{};
    QListWidget* sessionList_{};
    QLabel* activeSessionLabel_{};
    QLabel* captureStatusLabel_{};
    QLabel* captureSequenceMetricsLabel_{};
    QLabel* captureElapsedLabel_{};
    QLabel* captureStatisticsLabel_{};
    QProgressBar* captureProgressBar_{};
    QPushButton* captureStartButton_{};
    QPushButton* captureStopButton_{};
    QPushButton* captureBackButton_{};
    QThread* captureThread_{};
    QTimer* captureUiTimer_{};
    QElapsedTimer captureElapsedTimer_;
    QString captureExposureText_;
    std::shared_ptr<std::atomic_bool> captureStopRequested_;
    bool closeAfterCapture_{};
    std::string activeSessionId_;
};
