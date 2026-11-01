#pragma once

#include "application/session_service.h"

#include <QDialog>
#include <QString>

#include <string>

namespace presentation {

class DarkComboBox;

class SessionConfigurationDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SessionConfigurationDialog(
        application::SessionService& sessionService,
        QWidget* parent = nullptr);

    const std::string& createdSessionId() const noexcept;

private:
    void updateStartButtonState();

    application::SessionService& sessionService_;
    std::string createdSessionId_;
};

} // namespace presentation
