#include "presentation/dialogs/session_configuration_dialog.h"

#include "presentation/styles/theme.h"
#include "presentation/widgets/dark_combo_box.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <exception>

namespace presentation {

SessionConfigurationDialog::SessionConfigurationDialog(
    application::SessionService& sessionService,
    QWidget* parent)
    : QDialog(parent)
    , sessionService_(sessionService)
{
    setObjectName(QStringLiteral("sessionConfigurationDialog"));
    setWindowTitle(QStringLiteral("Configurar sessão"));
    setModal(true);
    setMinimumWidth(420);
    setStyleSheet(QStringLiteral(
        "QDialog { background:#09151d; color:#e5edf3; }"
        "QLabel { color:#e5edf3; }"
        "QLineEdit, QComboBox { background:#071119; color:#e5edf3; "
        "border:1px solid #263943; border-radius:5px; padding:7px; }"
        "QComboBox QAbstractItemView { background:#071119; color:#e5edf3; "
        "selection-background-color:#0c2928; selection-color:#49e3b6; "
        "border:0; outline:0; }"
        "QPushButton { background:#0d302d; color:#49e3b6; "
        "border:1px solid #21c99b; border-radius:6px; padding:8px 12px; }"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(theme::makeTextLabel(
        QStringLiteral("Configure os dados da nova sessão antes de iniciar."),
        QStringLiteral("sessionDialogDescription"),
        this));

    auto* targetInput = new QLineEdit(this);
    targetInput->setObjectName(QStringLiteral("sessionDialogTargetInput"));
    targetInput->setPlaceholderText(QStringLiteral("Ex.: M42"));

    auto* sessionInput = new QLineEdit(this);
    sessionInput->setObjectName(QStringLiteral("sessionDialogNameInput"));
    sessionInput->setPlaceholderText(QStringLiteral("Ex.: Observação de outubro"));

    auto* workflowInput = new DarkComboBox(this);
    workflowInput->setObjectName(QStringLiteral("sessionDialogWorkflowInput"));
    workflowInput->addItem(QStringLiteral("Captura"), static_cast<int>(domain::SessionStage::Capture));
    workflowInput->addItem(QStringLiteral("Empilhamento"), static_cast<int>(domain::SessionStage::Stacking));

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Nome da Sessão"), targetInput);
    form->addRow(QStringLiteral("Descrição"), sessionInput);
    form->addRow(QStringLiteral("Fluxo de trabalho"), workflowInput);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(this);
    buttons->setObjectName(QStringLiteral("sessionDialogButtons"));
    auto* startButton = buttons->addButton(
        QStringLiteral("Criar e iniciar sessão"),
        QDialogButtonBox::AcceptRole);
    startButton->setObjectName(QStringLiteral("createAndStartSessionButton"));
    auto* cancelButton = buttons->addButton(QDialogButtonBox::Cancel);
    cancelButton->setText(QStringLiteral("Cancelar"));
    startButton->setEnabled(false);
    layout->addWidget(buttons);

    const auto updateStartButton = [targetInput, sessionInput, startButton] {
        startButton->setEnabled(
            !targetInput->text().trimmed().isEmpty()
            && !sessionInput->text().trimmed().isEmpty());
    };
    connect(targetInput, &QLineEdit::textChanged, this, updateStartButton);
    connect(sessionInput, &QLineEdit::textChanged, this, updateStartButton);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(startButton, &QPushButton::clicked, this, [this, targetInput, sessionInput, workflowInput] {
        try {
            const auto stage = static_cast<domain::SessionStage>(
                workflowInput->currentData().toInt());
            const auto record = sessionService_.createSession(
                targetInput->text(),
                sessionInput->text(),
                stage);
            createdSessionId_ = record.session.id();
            accept();
        } catch (const std::exception& error) {
            QMessageBox::warning(
                this,
                QStringLiteral("Não foi possível iniciar a sessão"),
                QString::fromUtf8(error.what()));
        }
    });
}

const std::string& SessionConfigurationDialog::createdSessionId() const noexcept
{
    return createdSessionId_;
}

} // namespace presentation
