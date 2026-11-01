#pragma once

#include "application/session_service.h"
#include "presentation/widgets/responsive_home_scroll_area.h"

#include <string>

class QBoxLayout;
class QGridLayout;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace presentation {

class HomeView final : public ResponsiveHomeScrollArea
{
    Q_OBJECT

public:
    explicit HomeView(
        application::SessionService& sessionService,
        QWidget* parent = nullptr);

    void refresh();
    void setActiveSessionId(const std::string& id);


signals:
    void startSessionRequested();
    void openLibraryRequested();
    void openCurrentSessionRequested(const std::string& sessionId);

private:
    void rebuildRecentSessions(QWidget* page);
    void rebuildCurrentSessionCard(QWidget* page);
    void startButtonSpinner(QPushButton* button);
    void stopSpinner();

    application::SessionService& sessionService_;
    QWidget* contentWidget_{};
    QBoxLayout* pageLayout_{};
    QVBoxLayout* mainColumn_{};
    QVBoxLayout* rightColumn_{};
    QVBoxLayout* bannerLayout_{};
    QBoxLayout* recentHeader_{};
    QGridLayout* actionLayout_{};
    QGridLayout* recentLayout_{};
    std::vector<QWidget*> actionCards_;
    std::vector<QWidget*> recentCards_;
    QWidget* currentSessionCard_{};
    std::string activeSessionId_;

    QTimer* spinnerTimer_{};
    int spinnerFrame_{};
};

} // namespace presentation
