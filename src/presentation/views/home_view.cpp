#include "presentation/views/home_view.h"

#include "presentation/styles/theme.h"
#include "presentation/widgets/starfield_banner.h"

#include <QBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <tuple>

namespace presentation {

HomeView::HomeView(application::SessionService& sessionService, QWidget* parent)
    : ResponsiveHomeScrollArea(parent)
    , sessionService_(sessionService)
{
    // Basic widget setup
    setObjectName(QStringLiteral("homePage"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet(QStringLiteral("#homePage { background-color:#050d13; border:0; }"));

    QPalette scrollPalette = palette();
    scrollPalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    setPalette(scrollPalette);
    setAutoFillBackground(true);

    QPalette viewportPalette = viewport()->palette();
    viewportPalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    viewport()->setPalette(viewportPalette);
    viewport()->setAutoFillBackground(true);

    // Content widget
    contentWidget_ = new QWidget(this);
    contentWidget_->setObjectName(QStringLiteral("homeContent"));
    contentWidget_->setAttribute(Qt::WA_StyledBackground, true);
    contentWidget_->setMinimumWidth(0);
    contentWidget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    contentWidget_->setAutoFillBackground(true);
    contentWidget_->setStyleSheet(QStringLiteral("#homeContent { background-color:#050d13; }"));

    QPalette pagePalette = contentWidget_->palette();
    pagePalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    contentWidget_->setPalette(pagePalette);

    pageLayout_ = new QBoxLayout(QBoxLayout::LeftToRight, contentWidget_);
    pageLayout_->setObjectName(QStringLiteral("homeContentLayout"));
    pageLayout_->setContentsMargins(8, 8, 8, 8);
    pageLayout_->setSpacing(16);

    mainColumn_ = new QVBoxLayout;
    mainColumn_->setSpacing(16);

    // Banner
    auto* banner = new StarfieldBanner(contentWidget_);
    bannerLayout_ = new QVBoxLayout(banner);
    bannerLayout_->setContentsMargins(34, 26, 34, 26);
    bannerLayout_->addStretch();

    auto* welcomeTitle = theme::makeTextLabel(
        QStringLiteral("Bem-vindo ao\nAstroCapture"),
        QStringLiteral("welcomeTitle"),
        banner);
    welcomeTitle->setWordWrap(true);
    welcomeTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    welcomeTitle->setStyleSheet(QStringLiteral("font-size:25pt; font-weight:700; color:#f1f6fa;"));
    bannerLayout_->addWidget(welcomeTitle);

    auto* welcomeSubtitle = theme::makeTextLabel(
        QStringLiteral("Registre o céu, explore o universo."),
        QStringLiteral("welcomeSubtitle"),
        banner);
    welcomeSubtitle->setWordWrap(true);
    welcomeSubtitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    welcomeSubtitle->setStyleSheet(QStringLiteral("font-size:12pt; font-weight:600; color:#e6edf3;"));
    bannerLayout_->addWidget(welcomeSubtitle);

    auto* welcomeDetail = theme::makeTextLabel(
        QStringLiteral("Organize suas sessões astronômicas e acompanhe seu fluxo de captura."),
        QStringLiteral("welcomeDetail"),
        banner);
    welcomeDetail->setWordWrap(true);
    welcomeDetail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    welcomeDetail->setMaximumWidth(440);
    welcomeDetail->setStyleSheet(QStringLiteral("color:#a9bdca;"));
    bannerLayout_->addWidget(welcomeDetail);
    bannerLayout_->addStretch();
    mainColumn_->addWidget(banner);

    // Action cards (start, library, sequences, settings)
    actionLayout_ = new QGridLayout;
    actionLayout_->setObjectName(QStringLiteral("homeActionGrid"));
    actionLayout_->setHorizontalSpacing(12);
    actionLayout_->setVerticalSpacing(12);

    const std::vector<std::tuple<QString, QString, QString, QString>> actions = {
        {QStringLiteral("Iniciar sessão"), QStringLiteral("◎"),
         QStringLiteral("Crie ou abra uma sessão para preparar sua captura."),
         QStringLiteral("startCaptureAction")},
        {QStringLiteral("Abrir biblioteca"), QStringLiteral("▱"),
         QStringLiteral("Consulte as sessões e observações salvas."),
         QStringLiteral("openLibraryAction")},
        {QStringLiteral("Sequências"), QStringLiteral("▤"),
         QStringLiteral("Controles de sequência estarão disponíveis nesta etapa."),
         QStringLiteral("sequencesAction")},
        {QStringLiteral("Configurações"), QStringLiteral("⚙"),
         QStringLiteral("Preferências e dispositivos serão configurados aqui."),
         QStringLiteral("settingsAction")}
    };

    for (std::size_t index = 0; index < actions.size(); ++index) {
        const auto& [title, icon, detail, objectName] = actions[index];
        auto* card = theme::makeCard(QStringLiteral("actionCard"), contentWidget_);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        cardLayout->setSpacing(9);

        auto* actionIcon = theme::makeTextLabel(icon, QStringLiteral("cardIcon"), card);
        actionIcon->setStyleSheet(QStringLiteral("font-size:22pt; color:#40e0b1;"));
        auto* actionTitle = theme::makeTextLabel(title, QStringLiteral("cardTitle"), card);
        auto* actionDetail = theme::makeTextLabel(detail, QStringLiteral("mutedText"), card);
        actionDetail->setWordWrap(true);
        actionDetail->setMinimumHeight(54);
        auto* actionButton = new QPushButton(index == 0 ? QStringLiteral("Iniciar  →") : QStringLiteral("Abrir  →"), card);
        actionButton->setObjectName(objectName);
        if (index < 2) {
            actionButton->setProperty("primary", index == 0);
            actionButton->setStyleSheet(index == 0
                ? QStringLiteral("border:0; background:transparent; color:#46dfb2; text-align:left;")
                : QStringLiteral("border:0; background:transparent; color:#c1d1db; text-align:left;"));
            if (index == 0) {
                connect(actionButton, &QPushButton::clicked, this, [this] { emit startSessionRequested(); });
            } else {
                connect(actionButton, &QPushButton::clicked, this, [this] { emit openLibraryRequested(); });
            }
        } else {
            actionButton->setEnabled(false);
            actionButton->setText(QStringLiteral("Em breve"));
            actionButton->setStyleSheet(QStringLiteral("border:0; background:transparent; color:#657681; text-align:left;"));
        }
        cardLayout->addWidget(actionIcon);
        cardLayout->addWidget(actionTitle);
        cardLayout->addWidget(actionDetail);
        cardLayout->addStretch();
        cardLayout->addWidget(actionButton);
        actionCards_.push_back(card);
        actionLayout_->addWidget(card, 0, static_cast<int>(index));
    }
    mainColumn_->addLayout(actionLayout_);

    // Recent sessions header
    recentHeader_ = new QBoxLayout(QBoxLayout::LeftToRight);
    recentHeader_->addWidget(theme::makeTextLabel(QStringLiteral("Sessões recentes"), QStringLiteral("sectionTitle"), contentWidget_));
    recentHeader_->addStretch();
    auto* viewSessionsButton = new QPushButton(QStringLiteral("Ver sessões  →"), contentWidget_);
    viewSessionsButton->setObjectName(QStringLiteral("viewSessionsAction"));
    viewSessionsButton->setStyleSheet(QStringLiteral("border:0; background:transparent; color:#46dfb2;"));
    connect(viewSessionsButton, &QPushButton::clicked, this, [this] { emit openLibraryRequested(); });
    recentHeader_->addWidget(viewSessionsButton);
    mainColumn_->addLayout(recentHeader_);

    // Recent sessions grid
    recentLayout_ = new QGridLayout;
    recentLayout_->setObjectName(QStringLiteral("recentSessionsGrid"));
    recentLayout_->setHorizontalSpacing(10);
    recentLayout_->setVerticalSpacing(10);
    rebuildRecentSessions(contentWidget_);
    mainColumn_->addLayout(recentLayout_);

    // Right column – status cards
    rightColumn_ = new QVBoxLayout;
    rightColumn_->setSpacing(12);

    // Camera status card
    auto* cameraCard = theme::makeCard(QStringLiteral("statusCard"), contentWidget_);
    auto* cameraLayout = new QVBoxLayout(cameraCard);
    cameraLayout->setContentsMargins(16, 14, 16, 14);
    cameraLayout->addWidget(theme::makeTextLabel(QStringLiteral("◎   Status da câmera"), QStringLiteral("cardTitle"), cameraCard));
    cameraLayout->addWidget(theme::makeTextLabel(QStringLiteral("●  Não conectada"), QStringLiteral("cameraConnectionStatus"), cameraCard));
    cameraLayout->addWidget(theme::makeTextLabel(QStringLiteral("Conecte uma câmera na tela de captura."), QStringLiteral("statusDetail"), cameraCard));
    rightColumn_->addWidget(cameraCard);

    // Current session card (built later)
    rebuildCurrentSessionCard(contentWidget_);

    // Data storage card
    auto* dataCard = theme::makeCard(QStringLiteral("statusCard"), contentWidget_);
    auto* dataLayout = new QVBoxLayout(dataCard);
    dataLayout->setContentsMargins(16, 14, 16, 14);
    dataLayout->addWidget(theme::makeTextLabel(QStringLiteral("▤   Dados e armazenamento"), QStringLiteral("cardTitle"), dataCard));
    dataLayout->addWidget(theme::makeTextLabel(QStringLiteral("Banco local"), QStringLiteral("statusDetail"), dataCard));
    dataLayout->addWidget(theme::makeTextLabel(QStringLiteral("SQLite · astrocapture.sqlite3"), QStringLiteral("databaseStatus"), dataCard));
    dataLayout->addWidget(theme::makeTextLabel(QStringLiteral("Os originais são preservados na pasta de dados do aplicativo."), QStringLiteral("statusDetail"), dataCard));
    rightColumn_->addWidget(dataCard);

    // Tip card
    auto* tipCard = theme::makeCard(QStringLiteral("statusCard"), contentWidget_);
    auto* tipLayout = new QVBoxLayout(tipCard);
    tipLayout->setContentsMargins(16, 14, 16, 14);
    tipLayout->addWidget(theme::makeTextLabel(QStringLiteral("✧   Dica"), QStringLiteral("cardTitle"), tipCard));
    auto* tip = theme::makeTextLabel(QStringLiteral("Você pode preparar sessões antes de conectar uma câmera. O simulador ajuda a testar o fluxo."), QStringLiteral("statusDetail"), tipCard);
    tip->setWordWrap(true);
    tipLayout->addWidget(tip);
    rightColumn_->addWidget(tipCard);
    rightColumn_->addStretch();

    // Layout adjustments for right column widgets' labels
    for (int i = 0; i < rightColumn_->count(); ++i) {
        QWidget* w = rightColumn_->itemAt(i)->widget();
        if (w) {
            for (QLabel* label : w->findChildren<QLabel*>()) {
                label->setWordWrap(true);
            }
        }
    }

    // Assemble page layout
    pageLayout_->addLayout(mainColumn_, 1);
    pageLayout_->addLayout(rightColumn_);
    rightColumn_->setStretch(0, 0);
    rightColumn_->setStretch(1, 0);
    rightColumn_->setStretch(2, 0);
    rightColumn_->setStretch(3, 0);
    pageLayout_->setStretch(0, 1);
    pageLayout_->setStretch(1, 0);

    // Responsive layout handler
    const auto updateLayout = [this](int width) {
        const int cardColumns = width >= 1050 ? 4 : width >= 520 ? 2 : 1;
        const bool stackedColumns = width < 950;
        pageLayout_->setDirection(stackedColumns ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        const int margin = width < 400 ? 2 : width < 520 ? 4 : 8;
        pageLayout_->setContentsMargins(margin, margin, margin, margin);
        pageLayout_->setSpacing(width < 520 ? 8 : 16);
        recentHeader_->setDirection(width < 400 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        const int bannerHorizontalMargin = width < 520 ? 12 : 34;
        bannerLayout_->setContentsMargins(bannerHorizontalMargin,
                                          width < 520 ? 18 : 26,
                                          bannerHorizontalMargin,
                                          width < 520 ? 18 : 26);
        // arrange cards
        auto arrange = [cardColumns](QGridLayout* layout, const std::vector<QWidget*>& cards) {
            for (QWidget* c : cards) layout->removeWidget(c);
            for (int col = 0; col < 4; ++col) layout->setColumnStretch(col, col < cardColumns ? 1 : 0);
            for (size_t i = 0; i < cards.size(); ++i) {
                layout->addWidget(cards[i], static_cast<int>(i) / cardColumns, static_cast<int>(i) % cardColumns);
            }
        };

        arrange(actionLayout_, actionCards_);
        arrange(recentLayout_, recentCards_);
    };
    setResizeHandler(updateLayout);
    updateLayout(viewport()->width());

    setWidget(contentWidget_);
}

void HomeView::setActiveSessionId(const std::string& id) {
    activeSessionId_ = id;
    // Optionally refresh UI if needed
}

void HomeView::rebuildRecentSessions(QWidget* page) {
    for (QWidget* card : recentCards_) {
        recentLayout_->removeWidget(card);
        delete card;
    }
    recentCards_.clear();

    const auto sessions = sessionService_.listSessions();
    if (sessions.empty()) {
        auto* emptyCard = theme::makeCard(QStringLiteral("recentCard"), page);
        auto* emptyLayout = new QVBoxLayout(emptyCard);
        emptyLayout->setContentsMargins(16, 13, 16, 13);
        emptyLayout->addWidget(theme::makeTextLabel(QStringLiteral("Nenhuma sessão criada"), QStringLiteral("cardTitle"), emptyCard));
        emptyLayout->addWidget(theme::makeTextLabel(QStringLiteral("Inicie uma captura para criar sua primeira sessão."), QStringLiteral("mutedText"), emptyCard));
        recentCards_.push_back(emptyCard);
        recentLayout_->addWidget(emptyCard, 0, 0);
    } else {
        const auto visibleCount = qMin<std::size_t>(sessions.size(), 4);
        for (std::size_t index = 0; index < visibleCount; ++index) {
            const auto& record = sessions[index];
            auto* card = theme::makeCard(QStringLiteral("recentCard"), page);
            auto* recentCardLayout = new QVBoxLayout(card);
            recentCardLayout->setContentsMargins(13, 12, 13, 12);
            recentCardLayout->addWidget(theme::makeTextLabel(QString::fromStdString(record.session.name()), QStringLiteral("cardTitle"), card));
            recentCardLayout->addWidget(theme::makeTextLabel(QStringLiteral("%1 · %2")
                .arg(theme::stageName(record.session.stage()),
                     record.isOpen ? QStringLiteral("Aberta") : QStringLiteral("Fechada")),
                QStringLiteral("mutedText"), card));
            recentCards_.push_back(card);
            recentLayout_->addWidget(card, 0, static_cast<int>(index));
        }
    }
    for (QWidget* card : recentCards_) {
        for (QLabel* label : card->findChildren<QLabel*>()) {
            label->setWordWrap(true);
        }
    }
}

void HomeView::rebuildCurrentSessionCard(QWidget* page) {
    stopSpinner();
    if (currentSessionCard_) {
        rightColumn_->removeWidget(currentSessionCard_);
        delete currentSessionCard_;
        currentSessionCard_ = nullptr;
    }
    const auto sessions = sessionService_.listSessions();
    currentSessionCard_ = theme::makeCard(QStringLiteral("statusCard"), page);
    auto* currentSessionLayout = new QVBoxLayout(currentSessionCard_);
    currentSessionLayout->setContentsMargins(16, 14, 16, 14);
    currentSessionLayout->addWidget(theme::makeTextLabel(QStringLiteral("◇   Sessão atual"), QStringLiteral("cardTitle"), currentSessionCard_));

    const auto currentSessionIt = std::find_if(sessions.begin(), sessions.end(), [](const domain::SessionRecord& r) { return r.isOpen; });
    currentSessionLayout->addWidget(theme::makeTextLabel(
        currentSessionIt == sessions.end() ? QStringLiteral("Nenhuma sessão aberta")
                                          : QString::fromStdString(currentSessionIt->session.name()),
        QStringLiteral("currentSessionName"), currentSessionCard_));

    if (currentSessionIt != sessions.end()) {
        currentSessionLayout->addWidget(theme::makeTextLabel(
            QStringLiteral("%1 · %2")
                .arg(theme::stageName(currentSessionIt->session.stage()))
                .arg(QString::fromStdString(currentSessionIt->session.targetId())),
            QStringLiteral("statusDetail"), currentSessionCard_));
    } else {
        currentSessionLayout->addWidget(theme::makeTextLabel(QStringLiteral("Crie ou abra uma sessão para começar."), QStringLiteral("statusDetail"), currentSessionCard_));
    }

    const bool hasOpenSession = currentSessionIt != sessions.end();
    auto* currentSessionButton = new QPushButton(
        hasOpenSession ? QStringLiteral("Iniciar captura  →") : QStringLiteral("Nova sessão  →"),
        currentSessionCard_);
    currentSessionButton->setObjectName(QStringLiteral("currentSessionAction"));
    currentSessionButton->setStyleSheet(QStringLiteral("border:1px solid #25c99b; border-radius:6px; padding:8px; color:#46dfb2;"));
    if (hasOpenSession) {
        const std::string sessionId = currentSessionIt->session.id();
        connect(currentSessionButton, &QPushButton::clicked, this,
                [this, sessionId, currentSessionButton] {
                    startButtonSpinner(currentSessionButton);
                    QTimer::singleShot(0, this, [this, sessionId] {
                        emit openCurrentSessionRequested(sessionId);
                    });
                });
    } else {
        connect(currentSessionButton, &QPushButton::clicked, this,
                [this, currentSessionButton] {
                    startButtonSpinner(currentSessionButton);
                    QTimer::singleShot(0, this, [this] {
                        emit startSessionRequested();
                    });
                });
    }
    currentSessionLayout->addWidget(currentSessionButton);

    for (QLabel* label : currentSessionCard_->findChildren<QLabel*>()) {
        label->setWordWrap(true);
    }
    rightColumn_->insertWidget(1, currentSessionCard_);
}

void HomeView::refresh() {
    rebuildRecentSessions(contentWidget_);
    rebuildCurrentSessionCard(contentWidget_);
    // Resize handler will adjust layout; invoke directly
    const int w = viewport()->width();
    const auto handler = [this](int width) {
        const int cardColumns = width >= 1050 ? 4 : width >= 520 ? 2 : 1;
        // rearrange action and recent cards
        auto arrange = [cardColumns](QGridLayout* layout, const std::vector<QWidget*>& cards) {
            for (QWidget* c : cards) layout->removeWidget(c);
            for (int col = 0; col < 4; ++col) layout->setColumnStretch(col, col < cardColumns ? 1 : 0);
            for (size_t i = 0; i < cards.size(); ++i) {
                layout->addWidget(cards[i], static_cast<int>(i) / cardColumns, static_cast<int>(i) % cardColumns);
            }
        };
        arrange(actionLayout_, actionCards_);
        arrange(recentLayout_, recentCards_);
    };
    handler(w);
}

void HomeView::startButtonSpinner(QPushButton* button)
{
    // Spinner frames using Unicode braille/quarter-circle characters
    static const std::array<const char*, 4> kFrames{"◐", "◓", "◑", "◒"};

    spinnerFrame_ = 0;
    button->setEnabled(false);
    button->setText(QString::fromUtf8(kFrames[0]) + QStringLiteral("  Carregando…"));
    button->setStyleSheet(QStringLiteral(
        "border:1px solid #1a7a62; border-radius:6px; padding:8px; color:#2bb58a;"));

    if (!spinnerTimer_) {
        spinnerTimer_ = new QTimer(this);
        spinnerTimer_->setInterval(140);
    }

    // Disconnect any previous connection before reconnecting
    spinnerTimer_->disconnect();
    connect(spinnerTimer_, &QTimer::timeout, this, [this, button] {
        spinnerFrame_ = (spinnerFrame_ + 1) % 4;
        static const std::array<const char*, 4> frames{"◐", "◓", "◑", "◒"};
        button->setText(QString::fromUtf8(frames[spinnerFrame_]) + QStringLiteral("  Carregando…"));
    });
    spinnerTimer_->start();
}

void HomeView::stopSpinner()
{
    if (spinnerTimer_ && spinnerTimer_->isActive()) {
        spinnerTimer_->stop();
        spinnerTimer_->disconnect();
    }
}

} // namespace presentation
