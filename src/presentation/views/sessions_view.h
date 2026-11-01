#pragma once

#include "application/session_service.h"

#include <QString>
#include <QWidget>

#include <string>

class QLabel;
class QListWidget;

namespace presentation {

class SessionsView final : public QWidget
{
    Q_OBJECT

public:
    explicit SessionsView(
        application::SessionService& sessionService,
        QWidget* parent = nullptr);

    void refresh();
    void setActiveSessionText(const QString& text);
    QListWidget* sessionList() const noexcept;

signals:
    void openSessionRequested(const std::string& sessionId);
    void showInformationRequested(const std::string& sessionId);
    void deleteSessionRequested(const std::string& sessionId);

private:
    application::SessionService& sessionService_;
    QListWidget* sessionList_{};
    QLabel* activeSessionLabel_{};
};

} // namespace presentation
