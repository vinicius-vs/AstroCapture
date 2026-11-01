#include "presentation/styles/theme.h"

namespace presentation::theme {

const char* const DashboardStyle = R"(
    QMainWindow {
        background: #050d13;
    }
    #applicationShell, QWidget#applicationShell {
        background: #050d13;
        color: #e5edf3;
        font-family: "Noto Sans";
        font-size: 10pt;
    }
    #mainPages, QStackedWidget#mainPages {
        background: #050d13;
    }
    #homePage, QScrollArea#homePage {
        background-color: #050d13;
        border: 0;
    }
    #homeContent {
        background-color: #050d13;
    }
    #sessionsPage {
        background-color: #050d13;
    }
    #captureConfigurationPage, QScrollArea#captureConfigurationPage {
        background: #050d13;
        border: 0;
    }
    #captureConfigurationContent {
        background: #050d13;
    }
    QLabel {
        color: #e5edf3;
    }
    QFrame#sidebar, QFrame#statusCard, QFrame#actionCard, QFrame#recentCard {
        background: #09151d;
        border: 1px solid #192a34;
        border-radius: 10px;
    }
    QLabel#brandMark {
        color: #40e0b1;
        font-size: 18pt;
        font-weight: 700;
    }
    QLabel#brandTagline, QLabel#mutedText, QLabel#statusDetail {
        color: #91a6b4;
    }
    QPushButton#navigationButton {
        background: transparent;
        color: #b5c5d0;
        text-align: left;
        border: 0;
        border-radius: 7px;
        padding: 11px 14px;
    }
    QPushButton#navigationButton:hover, QPushButton#navigationButton[active="true"] {
        background: #0c2928;
        color: #42e4b3;
    }
    QPushButton#navigationButton:disabled {
        color: #53636d;
    }
    QPushButton#primaryAction {
        background: #0d302d;
        color: #49e3b6;
        border: 1px solid #21c99b;
        border-radius: 7px;
        padding: 10px 14px;
        font-weight: 600;
    }
    QPushButton#primaryAction:hover {
        background: #114039;
    }
    QPushButton#secondaryAction {
        background: #0a1720;
        color: #e5edf3;
        border: 1px solid #1b303c;
        border-radius: 7px;
        padding: 10px 14px;
    }
    QPushButton#secondaryAction:hover {
        border-color: #36c99f;
        color: #49e3b6;
    }
    QLabel#sectionTitle {
        font-size: 14pt;
        font-weight: 600;
    }
    QLabel#cardTitle {
        font-size: 11pt;
        font-weight: 600;
    }
    QLineEdit, QComboBox, QListWidget {
        background: #071119;
        color: #e5edf3;
        border: 1px solid #263943;
        border-radius: 5px;
        padding: 7px;
    }
    QListWidget::item {
        padding: 8px;
    }
    QListWidget::item:selected {
        background: #0c2928;
        color: #49e3b6;
    }
)";

QString stageName(domain::SessionStage stage)
{
    switch (stage) {
    case domain::SessionStage::Capture:
        return QStringLiteral("Captura");
    case domain::SessionStage::Stacking:
        return QStringLiteral("Empilhamento");
    case domain::SessionStage::Editing:
        return QStringLiteral("Edição");
    case domain::SessionStage::Finished:
        return QStringLiteral("Finalizada");
    }
    return QStringLiteral("Desconhecida");
}

QFrame* makeCard(const QString& objectName, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(objectName);
    return card;
}

QLabel* makeTextLabel(
    const QString& text,
    const QString& objectName,
    QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    return label;
}

} // namespace presentation::theme
