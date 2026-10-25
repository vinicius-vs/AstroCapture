#pragma once

#include <QMainWindow>

#include <string>

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QWidget;

namespace application {
class CaptureConfigurationService;
class SessionService;
}

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(
        application::SessionService& sessionService,
        application::CaptureConfigurationService& captureConfigurationService,
        QWidget* parent = nullptr);

private:
    QWidget* createHomePage();
    QWidget* createSessionsPage();
    void openSessionConfigurationDialog();
    void showCaptureConfigurationPage(const std::string& sessionId, bool returnHome);
    void openSession(const std::string& sessionId);
    void showSessionInformation(const std::string& sessionId);
    void deleteSession(const std::string& sessionId);
    void showHomePage();
    void showSessionsPage();
    void setNavigationSelection(bool homeSelected);
    void refreshSessions();

    application::SessionService& sessionService_;
    application::CaptureConfigurationService& captureConfigurationService_;
    QStackedWidget* pages_{};
    QWidget* homePage_{};
    QWidget* sessionsPage_{};
    QWidget* captureConfigurationPage_{};
    QPushButton* homeNavigationButton_{};
    QPushButton* libraryNavigationButton_{};
    QListWidget* sessionList_{};
    QLabel* activeSessionLabel_{};
    std::string activeSessionId_;
};
