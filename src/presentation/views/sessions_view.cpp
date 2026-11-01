#include "presentation/views/sessions_view.h"

#include "presentation/styles/theme.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPalette>
#include <QToolButton>
#include <QVBoxLayout>

namespace presentation {

SessionsView::SessionsView(
    application::SessionService& sessionService,
    QWidget* parent)
    : QWidget(parent)
    , sessionService_(sessionService)
{
    setObjectName(QStringLiteral("sessionsPage"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    setStyleSheet(QStringLiteral("#sessionsPage { background-color:#050d13; }"));

    QPalette pagePalette = palette();
    pagePalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    setPalette(pagePalette);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    layout->addWidget(theme::makeTextLabel(
        QStringLiteral("Sessões de captura"),
        QStringLiteral("sectionTitle"),
        this));

    auto* sessionsDescription = theme::makeTextLabel(
        QStringLiteral("Selecione uma sessão existente para continuar."),
        QStringLiteral("mutedText"),
        this);
    sessionsDescription->setWordWrap(true);
    layout->addWidget(sessionsDescription);

    layout->addWidget(theme::makeTextLabel(
        QStringLiteral("Sessões existentes"),
        QStringLiteral("sectionTitle"),
        this));

    sessionList_ = new QListWidget(this);
    sessionList_->setObjectName(QStringLiteral("sessionList"));
    sessionList_->setSpacing(5);
    sessionList_->setMinimumWidth(0);
    sessionList_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    sessionList_->setAutoFillBackground(true);
    QPalette listPalette = sessionList_->palette();
    listPalette.setColor(QPalette::Base, QColor(QStringLiteral("#071119")));
    listPalette.setColor(QPalette::Window, QColor(QStringLiteral("#071119")));
    listPalette.setColor(QPalette::Text, QColor(QStringLiteral("#e5edf3")));
    sessionList_->setPalette(listPalette);
    sessionList_->viewport()->setPalette(listPalette);
    sessionList_->viewport()->setAutoFillBackground(true);
    layout->addWidget(sessionList_, 1);

    auto* footer = new QHBoxLayout;
    activeSessionLabel_ = new QLabel(QStringLiteral("Nenhuma sessão aberta"), this);
    activeSessionLabel_->setObjectName(QStringLiteral("activeSessionLabel"));
    activeSessionLabel_->setWordWrap(true);
    footer->addWidget(activeSessionLabel_, 1);
    layout->addLayout(footer);

    connect(sessionList_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if (item) {
            emit openSessionRequested(item->data(Qt::UserRole).toString().toStdString());
        }
    });

    refresh();
}

void SessionsView::setActiveSessionText(const QString& text)
{
    if (activeSessionLabel_) {
        activeSessionLabel_->setText(text);
    }
}

QListWidget* SessionsView::sessionList() const noexcept
{
    return sessionList_;
}

void SessionsView::refresh()
{
    sessionList_->clear();
    for (const domain::SessionRecord& record : sessionService_.listSessions()) {
        const QString name = QString::fromStdString(record.session.name());
        const QString sessionId = QString::fromStdString(record.session.id());
        auto* item = new QListWidgetItem(sessionList_);
        item->setSizeHint(QSize(0, 58));
        item->setData(Qt::UserRole, sessionId);

        auto* row = new QWidget(sessionList_);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 5, 5, 5);
        rowLayout->setSpacing(10);

        auto* detailsLayout = new QVBoxLayout;
        detailsLayout->setContentsMargins(0, 0, 0, 0);
        detailsLayout->setSpacing(2);
        detailsLayout->addWidget(theme::makeTextLabel(
            name,
            QStringLiteral("sessionName"),
            row));
        detailsLayout->addWidget(theme::makeTextLabel(
            QStringLiteral("%1 · %2")
                .arg(theme::stageName(record.session.stage()),
                     record.isOpen ? QStringLiteral("Aberta") : QStringLiteral("Fechada")),
            QStringLiteral("mutedText"),
            row));
        rowLayout->addLayout(detailsLayout, 1);

        auto* menuButton = new QToolButton(row);
        menuButton->setObjectName(QStringLiteral("sessionMenuButton"));
        menuButton->setText(QStringLiteral("⋮"));
        menuButton->setToolTip(QStringLiteral("Ações da sessão"));
        menuButton->setPopupMode(QToolButton::InstantPopup);
        menuButton->setStyleSheet(QStringLiteral(
            "QToolButton { color:#e5edf3; background:transparent; border:0; "
            "border-radius:4px; padding:5px 10px; font-size:16pt; }"
            "QToolButton:hover { background:#17303a; color:#49e3b6; }"
            "QToolButton::menu-indicator { image:none; width:0; }"));

        auto* menu = new QMenu(menuButton);
        menu->setStyleSheet(QStringLiteral(
            "QMenu { background:#09151d; color:#e5edf3; border:1px solid #263943; padding:4px; }"
            "QMenu::item { padding:7px 24px; }"
            "QMenu::item:selected { background:#0c2928; color:#49e3b6; }"));
        QAction* openAction = menu->addAction(QStringLiteral("Abrir"));
        QAction* informationAction = menu->addAction(QStringLiteral("Informações"));
        menu->addSeparator();
        QAction* deleteAction = menu->addAction(QStringLiteral("Excluir"));
        menuButton->setMenu(menu);

        connect(openAction, &QAction::triggered, this, [this, sessionId] {
            emit openSessionRequested(sessionId.toStdString());
        });
        connect(informationAction, &QAction::triggered, this, [this, sessionId] {
            emit showInformationRequested(sessionId.toStdString());
        });
        connect(deleteAction, &QAction::triggered, this, [this, sessionId] {
            emit deleteSessionRequested(sessionId.toStdString());
        });

        rowLayout->addWidget(menuButton);
        sessionList_->setItemWidget(item, row);
    }
}

} // namespace presentation
