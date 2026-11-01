#pragma once

#include "application/session_recovery_service.h"

#include <QDialog>

#include <optional>
#include <string>

class QComboBox;

namespace presentation {

class SessionRecoveryDialog final : public QDialog
{
    Q_OBJECT

public:
    static constexpr int NewSequenceCode = 2;

    explicit SessionRecoveryDialog(
        const application::SessionRecoveryReport& report,
        QWidget* parent = nullptr);

    std::optional<domain::CaptureRunId> selectedRunId() const;

private:
    QComboBox* runSelector_{};
};

} // namespace presentation
