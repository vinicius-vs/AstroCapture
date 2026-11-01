#include "presentation/dialogs/session_recovery_dialog.h"

#include "presentation/styles/theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace presentation {

SessionRecoveryDialog::SessionRecoveryDialog(
    const application::SessionRecoveryReport& report,
    QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("sessionRecoveryDialog"));
    setWindowTitle(QStringLiteral("Retomar sessão"));
    setModal(true);
    setMinimumWidth(520);
    resize(700, 480);

    auto* layout = new QVBoxLayout(this);
    auto* summary = theme::makeTextLabel(
        QStringLiteral("Esta sessão tem sequências incompletas. Quadros concluídos não serão repetidos; "
                       "quadros interrompidos permanecem inconclusivos."),
        QStringLiteral("recoverySummary"),
        this);
    summary->setWordWrap(true);
    layout->addWidget(summary);

    runSelector_ = new QComboBox(this);
    runSelector_->setObjectName(QStringLiteral("recoveryRunSelector"));
    std::size_t totalPending = 0;
    std::size_t totalInterrupted = 0;
    for (const application::RecoverableCaptureRun& run : report.incompleteRuns) {
        totalPending += run.pendingFrameIds.size();
        totalInterrupted += run.interruptedFrameIds.size();
        if (!run.canContinue) {
            continue;
        }
        runSelector_->addItem(
            QStringLiteral("%1 · %2 pendente(s), %3 inconclusivo(s)")
                .arg(QString::fromStdString(run.id))
                .arg(run.pendingFrameIds.size())
                .arg(run.interruptedFrameIds.size()),
            QString::fromStdString(run.id));
    }

    std::size_t failedFrameCount = 0;
    std::size_t fileIssueCount = 0;
    for (const application::RecoveryFinding& finding : report.findings) {
        if (finding.type == application::RecoveryFindingType::FailedFrame) {
            ++failedFrameCount;
        } else if (finding.type != application::RecoveryFindingType::PendingFrame
                   && finding.type != application::RecoveryFindingType::InterruptedFrame) {
            ++fileIssueCount;
        }
    }
    layout->addWidget(theme::makeTextLabel(
        QStringLiteral(
            "Pendentes: %1 · inconclusivos: %2 · quadros com falha: %3 · inconsistências nos arquivos: %4")
            .arg(totalPending)
            .arg(totalInterrupted)
            .arg(failedFrameCount)
            .arg(fileIssueCount),
        {},
        this));
    layout->addWidget(runSelector_);

    auto* findings = new QListWidget(this);
    findings->setObjectName(QStringLiteral("recoveryFindings"));
    for (const application::RecoveryFinding& finding : report.findings) {
        QString findingText = finding.details;
        if (!finding.filePath.isEmpty()) {
            findingText += QStringLiteral("\n%1").arg(finding.filePath);
        } else if (finding.frameId) {
            findingText += QStringLiteral("\nQuadro %1")
                               .arg(QString::fromStdString(*finding.frameId));
        }
        findings->addItem(findingText);
    }
    if (report.findings.empty()) {
        findings->addItem(QStringLiteral("Nenhuma inconsistência de arquivo encontrada."));
    }
    layout->addWidget(findings, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* continueButton = buttons->addButton(
        QStringLiteral("Continuar pendentes"),
        QDialogButtonBox::AcceptRole);
    continueButton->setObjectName(QStringLiteral("continueRecoveryButton"));
    continueButton->setEnabled(runSelector_->count() > 0);
    auto* newSequenceButton = buttons->addButton(
        QStringLiteral("Criar nova sequência"),
        QDialogButtonBox::DestructiveRole);
    newSequenceButton->setObjectName(QStringLiteral("newRecoverySequenceButton"));
    auto* cancelButton = buttons->addButton(
        QStringLiteral("Cancelar"),
        QDialogButtonBox::RejectRole);
    cancelButton->setObjectName(QStringLiteral("cancelRecoveryButton"));
    layout->addWidget(buttons);

    connect(continueButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(newSequenceButton, &QPushButton::clicked, this, [this] {
        done(NewSequenceCode);
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

std::optional<domain::CaptureRunId> SessionRecoveryDialog::selectedRunId() const
{
    if (!runSelector_ || runSelector_->currentIndex() < 0) {
        return std::nullopt;
    }
    const QString data = runSelector_->currentData().toString();
    if (data.isEmpty()) {
        return std::nullopt;
    }
    return data.toStdString();
}

} // namespace presentation
