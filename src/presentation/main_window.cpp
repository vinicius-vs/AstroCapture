#include "main_window.h"

#include "application/capture_configuration_service.h"
#include "application/capture_preview_service.h"
#include "application/session_recovery_service.h"
#include "application/session_service.h"
#include "app/application_metadata.h"
#include "presentation/capture_worker.h"

#include <QComboBox>
#include <QColor>
#include <QAbstractItemView>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QGridLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFutureWatcher>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QImage>
#include <QImageReader>
#include <QListView>
#include <QListWidget>
#include <QLinearGradient>
#include <QDateTime>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QProgressBar>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <optional>
#include <tuple>
#include <vector>

namespace {

constexpr auto DashboardStyle = R"(
    QWidget#applicationShell {
        background: #050d13;
        color: #e5edf3;
        font-family: "Noto Sans";
        font-size: 10pt;
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

class StarfieldBanner final : public QFrame
{
public:
    explicit StarfieldBanner(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("welcomeBanner"));
        setMinimumHeight(245);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QFrame::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setClipRect(rect());

        QLinearGradient background(rect().topLeft(), rect().bottomRight());
        background.setColorAt(0.0, QColor(QStringLiteral("#081722")));
        background.setColorAt(0.55, QColor(QStringLiteral("#07101a")));
        background.setColorAt(1.0, QColor(QStringLiteral("#10232c")));
        painter.fillRect(rect(), background);

        const QPointF galaxyCenter(width() * 0.77, height() * 0.55);
        QRadialGradient galaxy(galaxyCenter, width() * 0.29);
        galaxy.setColorAt(0.0, QColor(242, 178, 143, 135));
        galaxy.setColorAt(0.09, QColor(225, 127, 157, 110));
        galaxy.setColorAt(0.22, QColor(81, 147, 206, 75));
        galaxy.setColorAt(0.55, QColor(38, 94, 153, 28));
        galaxy.setColorAt(1.0, QColor(20, 54, 91, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(galaxy);
        painter.drawEllipse(
            galaxyCenter,
            width() * 0.29,
            height() * 0.73);

        for (int index = 0; index < 100; ++index) {
            const int x = (index * 167 + 29) % qMax(1, width());
            const int y = (index * 97 + 17) % qMax(1, height());
            const int diameter = index % 13 == 0 ? 3 : 1;
            const int alpha = 65 + (index * 31) % 150;
            painter.setBrush(QColor(221, 237, 255, alpha));
            painter.drawEllipse(QPoint(x, y), diameter, diameter);
        }

        painter.setPen(QPen(QColor(QStringLiteral("#1b3440")), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 10, 10);
    }
};

class ResizeAwareWidget final : public QWidget
{
public:
    using ResizeHandler = std::function<void(int)>;

    explicit ResizeAwareWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
    }

    void setResizeHandler(ResizeHandler handler)
    {
        resizeHandler_ = std::move(handler);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        if (resizeHandler_) {
            resizeHandler_(event->size().width());
        }
    }

private:
    ResizeHandler resizeHandler_;
};

class ResponsiveHomeScrollArea final : public QScrollArea
{
public:
    using ResizeHandler = std::function<void(int)>;

    explicit ResponsiveHomeScrollArea(QWidget* parent = nullptr)
        : QScrollArea(parent)
    {
    }

    void setResizeHandler(ResizeHandler handler)
    {
        resizeHandler_ = std::move(handler);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QScrollArea::resizeEvent(event);
        if (resizeHandler_) {
            resizeHandler_(viewport()->width());
        }
    }

private:
    ResizeHandler resizeHandler_;
};

class DarkComboBox final : public QComboBox
{
public:
    explicit DarkComboBox(QWidget* parent = nullptr)
        : QComboBox(parent)
    {
        auto* listView = new QListView(this);
        listView->setFrameShape(QFrame::NoFrame);
        listView->setLineWidth(0);
        listView->setMidLineWidth(0);
        listView->setSpacing(0);
        listView->setStyleSheet(QStringLiteral(
            "QListView { background:#071119; color:#e5edf3; border:0; outline:0; }"
            "QListView::item { background:#071119; padding:5px 8px; }"
            "QListView::item:selected { background:#0c2928; color:#49e3b6; }"));
        QPalette palette = listView->palette();
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#071119")));
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#071119")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#e5edf3")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#0c2928")));
        palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#49e3b6")));
        listView->setPalette(palette);
        listView->viewport()->setPalette(palette);
        listView->viewport()->setAutoFillBackground(true);
        setView(listView);
    }

    void showPopup() override
    {
        QComboBox::showPopup();
        QWidget* popup = view()->parentWidget();
        if (auto* popupFrame = qobject_cast<QFrame*>(popup)) {
            popupFrame->setFrameShape(QFrame::NoFrame);
            popupFrame->setLineWidth(0);
            popupFrame->setMidLineWidth(0);
            popupFrame->setAutoFillBackground(true);
            QPalette palette = popupFrame->palette();
            palette.setColor(QPalette::Window, QColor(QStringLiteral("#071119")));
            popupFrame->setPalette(palette);
            popupFrame->setStyleSheet(QStringLiteral(
                "QFrame { background:#071119; border:0; padding:0; }"));
        }
    }
};

} // namespace

MainWindow::MainWindow(
    application::SessionService& sessionService,
    application::CaptureConfigurationService& captureConfigurationService,
    domain::ICamera& camera,
    QString applicationDataDirectory,
    application::SessionRecoveryService* sessionRecoveryService,
    application::CapturePreviewService* capturePreviewService,
    QWidget* parent)
    : QMainWindow(parent)
    , sessionService_(sessionService)
    , captureConfigurationService_(captureConfigurationService)
    , camera_(camera)
    , applicationDataDirectory_(std::move(applicationDataDirectory))
    , sessionRecoveryService_(sessionRecoveryService)
    , capturePreviewService_(capturePreviewService)
{
    setWindowTitle(applicationTitle());
    resize(1440, 900);

    auto* shell = new ResizeAwareWidget(this);
    shell->setObjectName(QStringLiteral("applicationShell"));
    shell->setMinimumWidth(0);
    shell->setStyleSheet(QString::fromLatin1(DashboardStyle));

    auto* shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(10, 10, 10, 10);
    shellLayout->setSpacing(12);

    auto* sidebar = new QFrame(shell);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(225);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(15, 22, 15, 14);
    sidebarLayout->setSpacing(8);

    auto* brandMark = makeTextLabel(
        QStringLiteral("◉ AstroCapture"),
        QStringLiteral("brandMark"),
        sidebar);
    auto* brandTagline = makeTextLabel(
        QStringLiteral("Capture o universo"),
        QStringLiteral("brandTagline"),
        sidebar);
    sidebarLayout->addWidget(brandMark);
    sidebarLayout->addWidget(brandTagline);
    sidebarLayout->addSpacing(20);

    homeNavigationButton_ = new QPushButton(QStringLiteral("⌂   Início"), sidebar);
    homeNavigationButton_->setObjectName(QStringLiteral("homeNavigationButton"));
    homeNavigationButton_->setProperty("active", true);
    homeNavigationButton_->setProperty("navigation", true);
    homeNavigationButton_->setStyleSheet(QStringLiteral(
        "QPushButton { text-align:left; border:0; border-radius:7px; padding:11px 14px; "
        "background:transparent; color:#b5c5d0; }"
        "QPushButton[active=\"true\"] { background:#0c2928; color:#42e4b3; }"
        "QPushButton:hover { background:#0c2928; color:#42e4b3; }"));
    libraryNavigationButton_ = new QPushButton(QStringLiteral("▧   Biblioteca"), sidebar);
    libraryNavigationButton_->setObjectName(QStringLiteral("libraryNavigationButton"));
    libraryNavigationButton_->setProperty("navigation", true);
    libraryNavigationButton_->setStyleSheet(homeNavigationButton_->styleSheet());
    auto* sequencesButton = new QPushButton(QStringLiteral("▱   Sequências"), sidebar);
    sequencesButton->setObjectName(QStringLiteral("sequencesNavigationButton"));
    sequencesButton->setProperty("navigation", true);
    sequencesButton->setStyleSheet(libraryNavigationButton_->styleSheet());
    auto* settingsButton = new QPushButton(QStringLiteral("⚙   Configurações"), sidebar);
    settingsButton->setObjectName(QStringLiteral("settingsNavigationButton"));
    settingsButton->setProperty("navigation", true);
    settingsButton->setStyleSheet(libraryNavigationButton_->styleSheet());
    const std::vector<std::pair<QPushButton*, QString>> navigationButtons{
        {homeNavigationButton_, QStringLiteral("⌂")},
        {libraryNavigationButton_, QStringLiteral("▧")},
        {sequencesButton, QStringLiteral("▱")},
        {settingsButton, QStringLiteral("⚙")}};
    for (const auto& [button, icon] : navigationButtons) {
        button->setProperty("fullText", button->text());
        button->setProperty("compactText", icon);
        button->setToolTip(button->text());
        button->setAccessibleName(button->text());
    }
    sidebarLayout->addWidget(homeNavigationButton_);
    sidebarLayout->addWidget(libraryNavigationButton_);
    sidebarLayout->addWidget(sequencesButton);
    sidebarLayout->addWidget(settingsButton);
    sidebarLayout->addStretch();

    auto* readyLayout = new QHBoxLayout;
    auto* readyIndicator = makeTextLabel(QStringLiteral("●"), {}, sidebar);
    readyIndicator->setStyleSheet(QStringLiteral("color:#40e0b1;"));
    readyLayout->addWidget(readyIndicator);
    auto* readyStatusText = makeTextLabel(QStringLiteral("Aplicativo pronto"), {}, sidebar);
    readyLayout->addWidget(readyStatusText);
    readyLayout->addStretch();
    sidebarLayout->addLayout(readyLayout);
    auto* sidebarStatusDetail = makeTextLabel(
        QStringLiteral("A câmera será conectada no fluxo de captura."),
        QStringLiteral("sidebarStatusDetail"),
        sidebar);
    sidebarLayout->addWidget(sidebarStatusDetail);

    pages_ = new QStackedWidget(shell);
    pages_->setObjectName(QStringLiteral("mainPages"));
    pages_->setMinimumWidth(0);
    homePage_ = createHomePage();
    sessionsPage_ = createSessionsPage();
    pages_->addWidget(homePage_);
    pages_->addWidget(sessionsPage_);
    pages_->setCurrentWidget(homePage_);

    shellLayout->addWidget(sidebar);
    shellLayout->addWidget(pages_, 1);
    setCentralWidget(shell);

    connect(homeNavigationButton_, &QPushButton::clicked, this, [this] { showHomePage(); });
    connect(libraryNavigationButton_, &QPushButton::clicked, this, [this] { showSessionsPage(); });
    captureUiTimer_ = new QTimer(this);
    captureUiTimer_->setInterval(1000);
    connect(captureUiTimer_, &QTimer::timeout, this, [this] {
        if (captureElapsedLabel_ && captureElapsedTimer_.isValid()) {
            const qint64 elapsedSeconds = captureElapsedTimer_.elapsed() / 1000;
            captureElapsedLabel_->setText(
                QStringLiteral("Tempo decorrido: %1:%2")
                    .arg(elapsedSeconds / 60, 2, 10, QLatin1Char('0'))
                    .arg(elapsedSeconds % 60, 2, 10, QLatin1Char('0')));
        }
    });
    sequencesButton->setEnabled(false);
    settingsButton->setEnabled(false);

    shell->setResizeHandler([sidebar, sidebarLayout, brandMark, brandTagline,
                             readyStatusText, sidebarStatusDetail, navigationButtons](int width) {
        const bool compact = width < 1000;
        const bool narrow = width < 520;
        sidebar->setFixedWidth(narrow ? 56 : compact ? 72 : 225);
        sidebarLayout->setContentsMargins(
            narrow ? 5 : compact ? 8 : 15,
            compact ? 12 : 22,
            narrow ? 5 : compact ? 8 : 15,
            14);
        brandMark->setText(compact ? QStringLiteral("◉") : QStringLiteral("◉ AstroCapture"));
        brandTagline->setVisible(!compact);
        readyStatusText->setVisible(!compact);
        sidebarStatusDetail->setVisible(!compact);
        for (const auto& [button, icon] : navigationButtons) {
            button->setText(compact ? icon : button->property("fullText").toString());
            button->setStyleSheet(compact
                    ? QStringLiteral("QPushButton { text-align:center; border:0; border-radius:7px; "
                                     "padding:11px 2px; background:transparent; color:#b5c5d0; }"
                                     "QPushButton[active=\"true\"] { background:#0c2928; color:#42e4b3; }"
                                     "QPushButton:hover { background:#0c2928; color:#42e4b3; }")
                    : QStringLiteral("QPushButton { text-align:left; border:0; border-radius:7px; "
                                     "padding:11px 14px; background:transparent; color:#b5c5d0; }"
                                     "QPushButton[active=\"true\"] { background:#0c2928; color:#42e4b3; }"
                                     "QPushButton:hover { background:#0c2928; color:#42e4b3; }"));
        }

    });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (captureThread_ && captureThread_->isRunning()) {
        closeAfterCapture_ = true;
        requestCaptureStop();
        if (captureStatusLabel_) {
            captureStatusLabel_->setText(
                QStringLiteral("Aguardando o quadro atual terminar antes de fechar."));
        }
        event->ignore();
        return;
    }
    event->accept();
}

QWidget* MainWindow::createHomePage()
{
    auto* scroll = new ResponsiveHomeScrollArea(this);
    scroll->setObjectName(QStringLiteral("homePage"));
    scroll->setMinimumWidth(0);
    scroll->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea#homePage { background-color:#050d13; border:0; }"));
    QPalette viewportPalette = scroll->viewport()->palette();
    viewportPalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    scroll->viewport()->setPalette(viewportPalette);
    scroll->viewport()->setAutoFillBackground(true);

    auto* page = new QWidget(scroll);
    page->setObjectName(QStringLiteral("homeContent"));
    page->setMinimumWidth(0);
    page->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    page->setAutoFillBackground(true);
    QPalette pagePalette = page->palette();
    pagePalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    page->setPalette(pagePalette);
    auto* pageLayout = new QBoxLayout(QBoxLayout::LeftToRight, page);
    pageLayout->setObjectName(QStringLiteral("homeContentLayout"));
    pageLayout->setContentsMargins(8, 8, 8, 8);
    pageLayout->setSpacing(16);

    auto* mainColumn = new QVBoxLayout;
    mainColumn->setSpacing(16);

    auto* banner = new StarfieldBanner(page);
    auto* bannerLayout = new QVBoxLayout(banner);
    bannerLayout->setContentsMargins(34, 26, 34, 26);
    bannerLayout->addStretch();
    auto* welcomeTitle = makeTextLabel(
        QStringLiteral("Bem-vindo ao\nAstroCapture"),
        QStringLiteral("welcomeTitle"),
        banner);
    welcomeTitle->setWordWrap(true);
    welcomeTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    welcomeTitle->setStyleSheet(
        QStringLiteral("font-size:25pt; font-weight:700; color:#f1f6fa;"));
    bannerLayout->addWidget(welcomeTitle);
    auto* welcomeSubtitle = makeTextLabel(
        QStringLiteral("Registre o céu, explore o universo."),
        QStringLiteral("welcomeSubtitle"),
        banner);
    welcomeSubtitle->setWordWrap(true);
    welcomeSubtitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    welcomeSubtitle->setStyleSheet(
        QStringLiteral("font-size:12pt; font-weight:600; color:#e6edf3;"));
    bannerLayout->addWidget(welcomeSubtitle);
    auto* welcomeDetail = makeTextLabel(
        QStringLiteral("Organize suas sessões astronômicas e acompanhe seu fluxo de captura."),
        QStringLiteral("welcomeDetail"),
        banner);
    welcomeDetail->setWordWrap(true);
    welcomeDetail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    welcomeDetail->setMaximumWidth(440);
    welcomeDetail->setStyleSheet(QStringLiteral("color:#a9bdca;"));
    bannerLayout->addWidget(welcomeDetail);
    bannerLayout->addStretch();
    mainColumn->addWidget(banner);

    auto* actionLayout = new QGridLayout;
    actionLayout->setObjectName(QStringLiteral("homeActionGrid"));
    actionLayout->setHorizontalSpacing(12);
    actionLayout->setVerticalSpacing(12);
    std::vector<QWidget*> actionCards;
    const std::vector<std::tuple<QString, QString, QString, QString>> actions{
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
         QStringLiteral("settingsAction")}};

    for (std::size_t index = 0; index < actions.size(); ++index) {
        const auto& [title, icon, detail, objectName] = actions[index];
        auto* card = makeCard(QStringLiteral("actionCard"), page);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        cardLayout->setSpacing(9);
        auto* actionIcon = makeTextLabel(icon, {}, card);
        actionIcon->setStyleSheet(QStringLiteral("font-size:22pt; color:#40e0b1;"));
        auto* actionTitle = makeTextLabel(title, QStringLiteral("cardTitle"), card);
        auto* actionDetail = makeTextLabel(detail, QStringLiteral("mutedText"), card);
        actionDetail->setWordWrap(true);
        actionDetail->setMinimumHeight(54);
        auto* actionButton = new QPushButton(
            index == 0 ? QStringLiteral("Iniciar  →") : QStringLiteral("Abrir  →"),
            card);
        actionButton->setObjectName(objectName);
        if (index < 2) {
            actionButton->setProperty("primary", index == 0);
            actionButton->setStyleSheet(index == 0
                    ? QStringLiteral("border:0; background:transparent; color:#46dfb2; text-align:left;")
                    : QStringLiteral("border:0; background:transparent; color:#c1d1db; text-align:left;"));
            if (index == 0) {
                connect(actionButton, &QPushButton::clicked,
                        this, [this] { openSessionConfigurationDialog(); });
            } else {
                connect(actionButton, &QPushButton::clicked,
                        this, [this] { showSessionsPage(); });
            }
        } else {
            actionButton->setEnabled(false);
            actionButton->setText(QStringLiteral("Em breve"));
            actionButton->setStyleSheet(
                QStringLiteral("border:0; background:transparent; color:#657681; text-align:left;"));
        }
        cardLayout->addWidget(actionIcon);
        cardLayout->addWidget(actionTitle);
        cardLayout->addWidget(actionDetail);
        cardLayout->addStretch();
        cardLayout->addWidget(actionButton);
        actionCards.push_back(card);
        actionLayout->addWidget(card, 0, static_cast<int>(index));
    }
    mainColumn->addLayout(actionLayout);

    auto* recentHeader = new QBoxLayout(QBoxLayout::LeftToRight);
    recentHeader->addWidget(makeTextLabel(
        QStringLiteral("Sessões recentes"),
        QStringLiteral("sectionTitle"),
        page));
    recentHeader->addStretch();
    auto* viewSessionsButton = new QPushButton(QStringLiteral("Ver sessões  →"), page);
    viewSessionsButton->setObjectName(QStringLiteral("viewSessionsAction"));
    viewSessionsButton->setStyleSheet(
        QStringLiteral("border:0; background:transparent; color:#46dfb2;"));
    connect(viewSessionsButton, &QPushButton::clicked, this, [this] { showSessionsPage(); });
    recentHeader->addWidget(viewSessionsButton);
    mainColumn->addLayout(recentHeader);

    auto* recentLayout = new QGridLayout;
    recentLayout->setObjectName(QStringLiteral("recentSessionsGrid"));
    recentLayout->setHorizontalSpacing(10);
    recentLayout->setVerticalSpacing(10);
    std::vector<QWidget*> recentCards;
    const auto sessions = sessionService_.listSessions();
    if (sessions.empty()) {
        auto* emptyCard = makeCard(QStringLiteral("recentCard"), page);
        auto* emptyLayout = new QVBoxLayout(emptyCard);
        emptyLayout->setContentsMargins(16, 13, 16, 13);
        emptyLayout->addWidget(makeTextLabel(
            QStringLiteral("Nenhuma sessão criada"),
            QStringLiteral("cardTitle"),
            emptyCard));
        emptyLayout->addWidget(makeTextLabel(
            QStringLiteral("Inicie uma captura para criar sua primeira sessão."),
            QStringLiteral("mutedText"),
            emptyCard));
        recentCards.push_back(emptyCard);
        recentLayout->addWidget(emptyCard, 0, 0);
    } else {
        const auto visibleCount = qMin<std::size_t>(sessions.size(), 4);
        for (std::size_t index = 0; index < visibleCount; ++index) {
            const auto& record = sessions[index];
            auto* card = makeCard(QStringLiteral("recentCard"), page);
            auto* recentCardLayout = new QVBoxLayout(card);
            recentCardLayout->setContentsMargins(13, 12, 13, 12);
            recentCardLayout->addWidget(makeTextLabel(
                QString::fromStdString(record.session.name()),
                QStringLiteral("cardTitle"),
                card));
            recentCardLayout->addWidget(makeTextLabel(
                QStringLiteral("%1 · %2")
                    .arg(stageName(record.session.stage()),
                         record.isOpen ? QStringLiteral("Aberta") : QStringLiteral("Fechada")),
                QStringLiteral("mutedText"),
                card));
            recentCards.push_back(card);
            recentLayout->addWidget(card, 0, static_cast<int>(index));
        }
    }
    mainColumn->addLayout(recentLayout);

    auto* rightColumn = new QVBoxLayout;
    rightColumn->setSpacing(12);
    auto* cameraCard = makeCard(QStringLiteral("statusCard"), page);
    auto* cameraLayout = new QVBoxLayout(cameraCard);
    cameraLayout->setContentsMargins(16, 14, 16, 14);
    cameraLayout->addWidget(makeTextLabel(
        QStringLiteral("◎   Status da câmera"),
        QStringLiteral("cardTitle"),
        cameraCard));
    cameraLayout->addWidget(makeTextLabel(
        QStringLiteral("●  Não conectada"),
        QStringLiteral("cameraConnectionStatus"),
        cameraCard));
    cameraLayout->addWidget(makeTextLabel(
        QStringLiteral("Conecte uma câmera na tela de captura."),
        QStringLiteral("statusDetail"),
        cameraCard));
    rightColumn->addWidget(cameraCard);

    auto* currentSessionCard = makeCard(QStringLiteral("statusCard"), page);
    auto* currentSessionLayout = new QVBoxLayout(currentSessionCard);
    currentSessionLayout->setContentsMargins(16, 14, 16, 14);
    currentSessionLayout->addWidget(makeTextLabel(
        QStringLiteral("◇   Sessão atual"),
        QStringLiteral("cardTitle"),
        currentSessionCard));
    const auto currentSession = std::find_if(
        sessions.begin(),
        sessions.end(),
        [](const domain::SessionRecord& record) { return record.isOpen; });
    currentSessionLayout->addWidget(makeTextLabel(
        currentSession == sessions.end()
            ? QStringLiteral("Nenhuma sessão aberta")
            : QString::fromStdString(currentSession->session.name()),
        QStringLiteral("currentSessionName"),
        currentSessionCard));
    if (currentSession != sessions.end()) {
        currentSessionLayout->addWidget(makeTextLabel(
            QStringLiteral("%1 · %2")
                .arg(stageName(currentSession->session.stage()),
                     QString::fromStdString(currentSession->session.targetId())),
            QStringLiteral("statusDetail"),
            currentSessionCard));
    } else {
        currentSessionLayout->addWidget(makeTextLabel(
            QStringLiteral("Crie ou abra uma sessão para começar."),
            QStringLiteral("statusDetail"),
            currentSessionCard));
    }
    auto* currentSessionButton = new QPushButton(QStringLiteral("Abrir sessões"), currentSessionCard);
    currentSessionButton->setObjectName(QStringLiteral("currentSessionAction"));
    currentSessionButton->setStyleSheet(
        QStringLiteral("border:1px solid #25c99b; border-radius:6px; padding:8px; color:#46dfb2;"));
    connect(currentSessionButton, &QPushButton::clicked, this, [this] { showSessionsPage(); });
    currentSessionLayout->addWidget(currentSessionButton);
    rightColumn->addWidget(currentSessionCard);

    auto* dataCard = makeCard(QStringLiteral("statusCard"), page);
    auto* dataLayout = new QVBoxLayout(dataCard);
    dataLayout->setContentsMargins(16, 14, 16, 14);
    dataLayout->addWidget(makeTextLabel(
        QStringLiteral("▤   Dados e armazenamento"),
        QStringLiteral("cardTitle"),
        dataCard));
    dataLayout->addWidget(makeTextLabel(
        QStringLiteral("Banco local"),
        QStringLiteral("statusDetail"),
        dataCard));
    dataLayout->addWidget(makeTextLabel(
        QStringLiteral("SQLite · astrocapture.sqlite3"),
        QStringLiteral("databaseStatus"),
        dataCard));
    dataLayout->addWidget(makeTextLabel(
        QStringLiteral("Os originais são preservados na pasta de dados do aplicativo."),
        QStringLiteral("statusDetail"),
        dataCard));
    rightColumn->addWidget(dataCard);

    auto* tipCard = makeCard(QStringLiteral("statusCard"), page);
    auto* tipLayout = new QVBoxLayout(tipCard);
    tipLayout->setContentsMargins(16, 14, 16, 14);
    tipLayout->addWidget(makeTextLabel(
        QStringLiteral("✧   Dica"),
        QStringLiteral("cardTitle"),
        tipCard));
    auto* tip = makeTextLabel(
        QStringLiteral("Você pode preparar sessões antes de conectar uma câmera. O simulador ajuda a testar o fluxo."),
        QStringLiteral("statusDetail"),
        tipCard);
    tip->setWordWrap(true);
    tipLayout->addWidget(tip);
    rightColumn->addWidget(tipCard);
    rightColumn->addStretch();
    for (int index = 0; index < rightColumn->count(); ++index) {
        QWidget* card = rightColumn->itemAt(index)->widget();
        if (card) {
            for (QLabel* label : card->findChildren<QLabel*>()) {
                label->setWordWrap(true);
            }
        }
    }
    for (QWidget* card : recentCards) {
        for (QLabel* label : card->findChildren<QLabel*>()) {
            label->setWordWrap(true);
        }
    }

    pageLayout->addLayout(mainColumn, 1);
    pageLayout->addLayout(rightColumn);
    rightColumn->setStretch(0, 0);
    rightColumn->setStretch(1, 0);
    rightColumn->setStretch(2, 0);
    rightColumn->setStretch(3, 0);
    pageLayout->setStretch(0, 1);
    pageLayout->setStretch(1, 0);

    const auto updateLayout = [pageLayout, bannerLayout, recentHeader, actionLayout,
                               actionCards, recentLayout, recentCards](int width) {
        const int cardColumns = width >= 1050 ? 4 : width >= 520 ? 2 : 1;
        const auto arrangeCards = [cardColumns](QGridLayout* layout, const std::vector<QWidget*>& cards) {
            for (QWidget* card : cards) {
                layout->removeWidget(card);
            }
            for (int column = 0; column < 4; ++column) {
                layout->setColumnStretch(column, column < cardColumns ? 1 : 0);
            }
            for (std::size_t index = 0; index < cards.size(); ++index) {
                layout->addWidget(
                    cards[index],
                    static_cast<int>(index) / cardColumns,
                    static_cast<int>(index) % cardColumns);
            }
        };

        const bool stackedColumns = width < 950;
        pageLayout->setDirection(stackedColumns
                ? QBoxLayout::TopToBottom
                : QBoxLayout::LeftToRight);
        const int margin = width < 400 ? 2 : width < 520 ? 4 : 8;
        pageLayout->setContentsMargins(margin, margin, margin, margin);
        pageLayout->setSpacing(width < 520 ? 8 : 16);
        recentHeader->setDirection(width < 400
                ? QBoxLayout::TopToBottom
                : QBoxLayout::LeftToRight);
        const int bannerHorizontalMargin = width < 520 ? 12 : 34;
        bannerLayout->setContentsMargins(
            bannerHorizontalMargin,
            width < 520 ? 18 : 26,
            bannerHorizontalMargin,
            width < 520 ? 18 : 26);
        arrangeCards(actionLayout, actionCards);
        arrangeCards(recentLayout, recentCards);
    };
    scroll->setResizeHandler(updateLayout);
    updateLayout(scroll->viewport()->width());

    scroll->setWidget(page);
    return scroll;
}

QWidget* MainWindow::createSessionsPage()
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("sessionsPage"));
    page->setAutoFillBackground(true);
    QPalette pagePalette = page->palette();
    pagePalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    page->setPalette(pagePalette);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    layout->addWidget(makeTextLabel(
        QStringLiteral("Sessões de captura"),
        QStringLiteral("sectionTitle"),
        page));
    auto* sessionsDescription = makeTextLabel(
        QStringLiteral("Selecione uma sessão existente para continuar."),
        QStringLiteral("mutedText"),
        page);
    sessionsDescription->setWordWrap(true);
    layout->addWidget(sessionsDescription);

    layout->addWidget(makeTextLabel(
        QStringLiteral("Sessões existentes"),
        QStringLiteral("sectionTitle"),
        page));
    sessionList_ = new QListWidget(page);
    sessionList_->setObjectName(QStringLiteral("sessionList"));
    sessionList_->setSpacing(5);
    sessionList_->setMinimumWidth(0);
    sessionList_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    layout->addWidget(sessionList_, 1);
    auto* footer = new QHBoxLayout;
    activeSessionLabel_ = new QLabel(QStringLiteral("Nenhuma sessão aberta"), page);
    activeSessionLabel_->setObjectName(QStringLiteral("activeSessionLabel"));
    activeSessionLabel_->setWordWrap(true);
    footer->addWidget(activeSessionLabel_, 1);
    layout->addLayout(footer);

    connect(sessionList_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        openSession(item->data(Qt::UserRole).toString().toStdString());
    });

    try {
        refreshSessions();
    } catch (const std::exception& error) {
        QMessageBox::critical(
            this,
            QStringLiteral("Não foi possível carregar as sessões"),
            QString::fromUtf8(error.what()));
    }
    return page;
}

void MainWindow::openSessionConfigurationDialog()
{
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("sessionConfigurationDialog"));
    dialog.setWindowTitle(QStringLiteral("Configurar sessão"));
    dialog.setModal(true);
    dialog.setMinimumWidth(420);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background:#09151d; color:#e5edf3; }"
        "QLabel { color:#e5edf3; }"
        "QLineEdit, QComboBox { background:#071119; color:#e5edf3; "
        "border:1px solid #263943; border-radius:5px; padding:7px; }"
        "QComboBox QAbstractItemView { background:#071119; color:#e5edf3; "
        "selection-background-color:#0c2928; selection-color:#49e3b6; "
        "border:0; outline:0; }"
        "QPushButton { background:#0d302d; color:#49e3b6; "
        "border:1px solid #21c99b; border-radius:6px; padding:8px 12px; }"));

    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(makeTextLabel(
        QStringLiteral("Configure os dados da nova sessão antes de iniciar."),
        QStringLiteral("sessionDialogDescription"),
        &dialog));

    auto* targetInput = new QLineEdit(&dialog);
    targetInput->setObjectName(QStringLiteral("sessionDialogTargetInput"));
    targetInput->setPlaceholderText(QStringLiteral("Ex.: M42"));
    auto* sessionInput = new QLineEdit(&dialog);
    sessionInput->setObjectName(QStringLiteral("sessionDialogNameInput"));
    sessionInput->setPlaceholderText(QStringLiteral("Ex.: Observação de outubro"));
    auto* workflowInput = new DarkComboBox(&dialog);
    workflowInput->setObjectName(QStringLiteral("sessionDialogWorkflowInput"));
    workflowInput->addItem(QStringLiteral("Captura"), static_cast<int>(domain::SessionStage::Capture));
    workflowInput->addItem(QStringLiteral("Empilhamento"), static_cast<int>(domain::SessionStage::Stacking));

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Nome da Sessão"), targetInput);
    form->addRow(QStringLiteral("Descrição"), sessionInput);
    form->addRow(QStringLiteral("Fluxo de trabalho"), workflowInput);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(&dialog);
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
    connect(targetInput, &QLineEdit::textChanged, &dialog, updateStartButton);
    connect(sessionInput, &QLineEdit::textChanged, &dialog, updateStartButton);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(startButton, &QPushButton::clicked, &dialog, [this, &dialog, targetInput, sessionInput, workflowInput] {
        try {
            const auto stage = static_cast<domain::SessionStage>(
                workflowInput->currentData().toInt());
            const auto record = sessionService_.createSession(
                targetInput->text(),
                sessionInput->text(),
                stage);
            activeSessionId_ = record.session.id();
            dialog.accept();
        } catch (const std::exception& error) {
            QMessageBox::warning(
                &dialog,
                QStringLiteral("Não foi possível iniciar a sessão"),
                QString::fromUtf8(error.what()));
        }
    });

    if (dialog.exec() == QDialog::Accepted) {
        showCaptureConfigurationPage(activeSessionId_, true);
    }
}

void MainWindow::showCaptureConfigurationPage(const std::string& sessionId, bool returnHome)
{
    domain::CameraCapabilities capabilities;
    domain::CameraSettings currentSettings;
    try {
        capabilities = captureConfigurationService_.cameraCapabilities();
        currentSettings = captureConfigurationService_.currentCameraSettings();
    } catch (const std::exception& error) {
        QMessageBox::warning(
            this,
            QStringLiteral("Câmera indisponível"),
            QString::fromUtf8(error.what()));
        return;
    }

    ++previewRequestId_;
    if (captureConfigurationPage_) {
        pages_->removeWidget(captureConfigurationPage_);
        delete captureConfigurationPage_;
        captureConfigurationPage_ = nullptr;
    }

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("captureConfigurationPage"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea#captureConfigurationPage { background:#050d13; border:0; }"));
    auto* page = new QWidget(scroll);
    page->setObjectName(QStringLiteral("captureConfigurationContent"));
    page->setMinimumWidth(0);
    page->setAutoFillBackground(true);
    page->setStyleSheet(QStringLiteral(
        "QLabel { color:#e5edf3; }"
        "QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox { background:#071119; color:#e5edf3; "
        "border:1px solid #263943; border-radius:5px; padding:7px; }"
        "QComboBox QAbstractItemView { background:#071119; color:#e5edf3; "
        "selection-background-color:#0c2928; selection-color:#49e3b6; border:0; outline:0; }"
        "QPushButton { background:#0d302d; color:#49e3b6; "
        "border:1px solid #21c99b; border-radius:6px; padding:8px 12px; }"));
    QPalette pagePalette = page->palette();
    pagePalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    page->setPalette(pagePalette);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);
    layout->addWidget(makeTextLabel(
        QStringLiteral("Defina a câmera e os parâmetros da sequência."),
        QStringLiteral("captureConfigurationDescription"),
        page));

    auto* cameraInput = new QComboBox(page);
    cameraInput->setObjectName(QStringLiteral("captureCameraInput"));
    cameraInput->addItem(QStringLiteral("Simulada"), QStringLiteral("simulated"));
    cameraInput->addItem(QStringLiteral("Câmera física (indisponível)"), QStringLiteral("physical"));
    if (auto* model = qobject_cast<QStandardItemModel*>(cameraInput->model())) {
        if (QStandardItem* physicalItem = model->item(1)) {
            physicalItem->setFlags(physicalItem->flags() & ~Qt::ItemIsEnabled);
        }
    }

    auto* cameraStatus = makeTextLabel(
        QStringLiteral("Simulador conectado · Exposição: %1–%2 s · ISO: %3")
            .arg(
                QString::number(
                    static_cast<double>(capabilities.exposure.minimum.value_or(
                        std::chrono::microseconds::zero()).count()) / 1'000'000.0,
                    'f',
                    3),
                QString::number(
                    static_cast<double>(capabilities.exposure.maximum.value_or(
                        std::chrono::microseconds::zero()).count()) / 1'000'000.0,
                    'f',
                    0),
                capabilities.iso.state == domain::CapabilityState::Unavailable
                    ? QStringLiteral("indisponível")
                    : QStringLiteral("suportado")),
        QStringLiteral("captureCameraCapabilities"),
                page);
    cameraStatus->setWordWrap(true);

    auto* frameTypeInput = new QComboBox(page);
    frameTypeInput->setObjectName(QStringLiteral("frameTypeInput"));
    frameTypeInput->addItem(QStringLiteral("Light"), static_cast<int>(domain::FrameType::Light));
    frameTypeInput->addItem(QStringLiteral("Dark"), static_cast<int>(domain::FrameType::Dark));
    frameTypeInput->addItem(QStringLiteral("Flat"), static_cast<int>(domain::FrameType::Flat));
    frameTypeInput->addItem(QStringLiteral("Bias"), static_cast<int>(domain::FrameType::Bias));
    frameTypeInput->addItem(QStringLiteral("Dark Flat"), static_cast<int>(domain::FrameType::DarkFlat));

    auto* modeInput = new QComboBox(page);
    modeInput->setObjectName(QStringLiteral("captureModeInput"));
    modeInput->addItem(QStringLiteral("Quantidade definida"), static_cast<int>(domain::CaptureMode::Finite));
    modeInput->addItem(QStringLiteral("Contínua"), static_cast<int>(domain::CaptureMode::Continuous));

    auto* exposureInput = new QDoubleSpinBox(page);
    exposureInput->setObjectName(QStringLiteral("exposureInput"));
    exposureInput->setDecimals(3);
    exposureInput->setSingleStep(0.1);
    exposureInput->setSuffix(QStringLiteral(" s"));
    const bool exposureAvailable =
        capabilities.exposure.state != domain::CapabilityState::Unavailable;
    exposureInput->setEnabled(exposureAvailable);
    if (exposureAvailable) {
        const double minimum = static_cast<double>(
            capabilities.exposure.minimum.value_or(std::chrono::microseconds{1000}).count())
            / 1'000'000.0;
        const double maximum = static_cast<double>(
            capabilities.exposure.maximum.value_or(std::chrono::minutes{30}).count())
            / 1'000'000.0;
        exposureInput->setRange(minimum, maximum);
        exposureInput->setValue(currentSettings.exposure
                ? static_cast<double>(currentSettings.exposure->count()) / 1'000'000.0
                : minimum);
    } else {
        exposureInput->setRange(0.0, 0.0);
    }
    if (capabilities.exposure.state == domain::CapabilityState::ReadOnly) {
        exposureInput->setReadOnly(true);
    }

    auto* isoInput = new QComboBox(page);
    isoInput->setObjectName(QStringLiteral("isoInput"));
    const bool isoAvailable = capabilities.iso.state != domain::CapabilityState::Unavailable;
    if (isoAvailable) {
        if (capabilities.iso.state == domain::CapabilityState::Supported) {
            isoInput->addItem(QStringLiteral("Não informar"), QVariant{});
            for (const unsigned int iso : capabilities.iso.values) {
                isoInput->addItem(QString::number(iso), iso);
            }
        } else if (currentSettings.iso) {
            isoInput->addItem(QString::number(*currentSettings.iso), *currentSettings.iso);
            isoInput->setEnabled(false);
        } else {
            isoInput->addItem(QStringLiteral("ISO indisponível"));
            isoInput->setEnabled(false);
        }

        if (currentSettings.iso) {
            const int currentIndex = isoInput->findData(*currentSettings.iso);
            if (currentIndex >= 0) {
                isoInput->setCurrentIndex(currentIndex);
            }
        }
    } else {
        isoInput->addItem(QStringLiteral("Não suportado"));
        isoInput->setEnabled(false);
    }

    auto* countInput = new QSpinBox(page);
    countInput->setObjectName(QStringLiteral("frameCountInput"));
    countInput->setRange(1, 10000);
    countInput->setValue(1);
    connect(modeInput, &QComboBox::currentIndexChanged, page, [modeInput, countInput] {
        const auto mode = static_cast<domain::CaptureMode>(modeInput->currentData().toInt());
        countInput->setEnabled(mode == domain::CaptureMode::Finite);
    });

    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    form->addRow(QStringLiteral("Câmera"), cameraInput);
    form->addRow(QString{}, cameraStatus);
    form->addRow(QStringLiteral("Tipo de quadro"), frameTypeInput);
    form->addRow(QStringLiteral("Modo"), modeInput);
    form->addRow(QStringLiteral("Exposição"), exposureInput);
    form->addRow(QStringLiteral("ISO"), isoInput);
    form->addRow(QStringLiteral("Quantidade"), countInput);
    layout->addLayout(form);

    auto* errorLabel = makeTextLabel({}, QStringLiteral("captureValidationError"), page);
    errorLabel->setWordWrap(true);
    errorLabel->setStyleSheet(QStringLiteral("color:#ff8e8e;"));
    layout->addWidget(errorLabel);

    captureStatusLabel_ = makeTextLabel(
        {},
        QStringLiteral("captureConfigurationStatus"),
        page);
    captureStatusLabel_->setWordWrap(true);
    captureStatusLabel_->setStyleSheet(QStringLiteral("color:#49e3b6;"));
    layout->addWidget(captureStatusLabel_);

    captureProgressBar_ = new QProgressBar(page);
    captureProgressBar_->setObjectName(QStringLiteral("captureProgressBar"));
    captureProgressBar_->setRange(0, 1);
    captureProgressBar_->setValue(0);
    captureProgressBar_->setTextVisible(true);
    captureProgressBar_->setFormat(QStringLiteral("%v / %m quadros"));
    captureProgressBar_->hide();
    layout->addWidget(captureProgressBar_);

    captureSequenceMetricsLabel_ = makeTextLabel(
        QStringLiteral("Aguardando início da sequência."),
        QStringLiteral("captureSequenceMetrics"),
        page);
    captureSequenceMetricsLabel_->setWordWrap(true);
    captureElapsedLabel_ = makeTextLabel(
        QStringLiteral("Tempo decorrido: 00:00"),
        QStringLiteral("captureElapsedTime"),
        page);
    layout->addWidget(captureSequenceMetricsLabel_);
    layout->addWidget(captureElapsedLabel_);

    captureStatisticsLabel_ = makeTextLabel(
        QStringLiteral("As estatísticas aparecerão após carregar os dados da sessão."),
        QStringLiteral("captureStatistics"),
        page);
    captureStatisticsLabel_->setWordWrap(true);
    layout->addWidget(captureStatisticsLabel_);

    layout->addWidget(makeTextLabel(
        QStringLiteral("Último quadro concluído"),
        QStringLiteral("sectionTitle"),
        page));
    capturePreviewImageLabel_ = new QLabel(page);
    capturePreviewImageLabel_->setObjectName(QStringLiteral("capturePreviewImage"));
    capturePreviewImageLabel_->setAlignment(Qt::AlignCenter);
    capturePreviewImageLabel_->setMinimumHeight(220);
    capturePreviewImageLabel_->setMaximumHeight(400);
    capturePreviewImageLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    capturePreviewImageLabel_->setStyleSheet(
        QStringLiteral("QLabel { background:#071119; border:1px solid #263943; }"));
    layout->addWidget(capturePreviewImageLabel_);
    capturePreviewStatusLabel_ = makeTextLabel(
        QStringLiteral("Nenhuma imagem disponível."),
        QStringLiteral("capturePreviewStatus"),
        page);
    capturePreviewStatusLabel_->setAlignment(Qt::AlignCenter);
    capturePreviewStatusLabel_->setWordWrap(true);
    layout->addWidget(capturePreviewStatusLabel_);

    auto* buttons = new QDialogButtonBox(page);
    auto* startButton = buttons->addButton(
        QStringLiteral("Iniciar captura"),
        QDialogButtonBox::AcceptRole);
    startButton->setObjectName(QStringLiteral("startCaptureButton"));
    captureStartButton_ = startButton;
    startButton->setEnabled(exposureAvailable);
    captureBackButton_ = buttons->addButton(QDialogButtonBox::Cancel);
    captureBackButton_->setText(QStringLiteral("Voltar"));
    captureBackButton_->setObjectName(QStringLiteral("captureBackButton"));
    captureStopButton_ = buttons->addButton(
        QStringLiteral("Parar após o quadro atual"),
        QDialogButtonBox::ActionRole);
    captureStopButton_->setObjectName(QStringLiteral("stopCaptureButton"));
    captureStopButton_->hide();
    connect(captureStopButton_, &QPushButton::clicked, this, [this] {
        requestCaptureStop();
    });
    connect(captureBackButton_, &QPushButton::clicked, this, [this, returnHome] {
        if (returnHome) {
            showHomePage();
        } else {
            showSessionsPage();
        }
    });
    connect(startButton, &QPushButton::clicked, page, [this, sessionId,
                       frameTypeInput, modeInput, exposureInput, isoInput, countInput, errorLabel] {
        errorLabel->clear();
        captureStatusLabel_->clear();
        try {
            const auto frameType = static_cast<domain::FrameType>(frameTypeInput->currentData().toInt());
            const auto mode = static_cast<domain::CaptureMode>(modeInput->currentData().toInt());
            const auto exposure = std::chrono::microseconds{
                static_cast<std::int64_t>(std::llround(exposureInput->value() * 1'000'000.0))};
            const std::optional<unsigned int> iso = isoInput->isEnabled()
                    && isoInput->currentData().isValid()
                ? std::optional<unsigned int>{isoInput->currentData().toUInt()}
                : std::nullopt;
            const std::optional<std::uint32_t> plannedCount =
                mode == domain::CaptureMode::Finite
                ? std::optional<std::uint32_t>{static_cast<std::uint32_t>(countInput->value())}
                : std::nullopt;
            const domain::CaptureSettings settings{
                frameType,
                exposure,
                iso,
                mode,
                plannedCount};
            const auto captureRun = captureConfigurationService_.configureSequence(sessionId, settings);
            errorLabel->clear();
            startCapture(captureRun);
        } catch (const std::exception& error) {
            errorLabel->setText(QString::fromUtf8(error.what()));
        }
    });
    layout->addWidget(buttons);
    scroll->setWidget(page);
    captureConfigurationPage_ = scroll;
    setNavigationSelection(returnHome);
    pages_->addWidget(scroll);
    pages_->setCurrentWidget(scroll);
    QTimer::singleShot(0, this, [this, sessionId] {
        loadCapturePreview(sessionId);
    });
}

void MainWindow::loadCapturePreview(const std::string& sessionId)
{
    if (!capturePreviewService_) {
        capturePreviewImageLabel_->clear();
        capturePreviewStatusLabel_->setText(QStringLiteral("Nenhuma imagem disponível."));
        return;
    }

    try {
        const auto imagePath = capturePreviewService_->latestCompletedImagePath(sessionId);
        if (!imagePath) {
            ++previewRequestId_;
            capturePreviewImageLabel_->clear();
            capturePreviewStatusLabel_->setText(QStringLiteral("Nenhuma imagem disponível."));
            return;
        }
        decodeCapturePreview(QString::fromStdString(imagePath->string()));
    } catch (const std::exception& error) {
        ++previewRequestId_;
        capturePreviewImageLabel_->clear();
        capturePreviewStatusLabel_->setText(
            QStringLiteral("Não foi possível localizar a pré-visualização: %1")
                .arg(QString::fromUtf8(error.what())));
    }
}

void MainWindow::decodeCapturePreview(const QString& imagePath)
{
    const quint64 requestId = ++previewRequestId_;
    capturePreviewImageLabel_->clear();
    capturePreviewStatusLabel_->setText(QStringLiteral("Carregando pré-visualização…"));

    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this,
            [this, watcher, requestId] {
        const QImage image = watcher->result();
        watcher->deleteLater();
        if (requestId != previewRequestId_) {
            return;
        }
        if (image.isNull()) {
            capturePreviewImageLabel_->clear();
            capturePreviewStatusLabel_->setText(
                QStringLiteral("Pré-visualização indisponível: arquivo inválido ou formato não suportado."));
            return;
        }
        capturePreviewImageLabel_->setPixmap(QPixmap::fromImage(image).scaled(
            QSize(960, 640),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
        capturePreviewStatusLabel_->setText(QStringLiteral("Último quadro concluído."));
    });
    watcher->setFuture(QtConcurrent::run([imagePath] {
        QImageReader reader(imagePath);
        reader.setAutoTransform(true);
        const QSize sourceSize = reader.size();
        if (sourceSize.isValid()) {
            reader.setScaledSize(sourceSize.scaled(QSize(1280, 960), Qt::KeepAspectRatio));
        }
        QImage image = reader.read();
        if (image.isNull()) {
            return QImage{};
        }
        if (image.width() > 1280 || image.height() > 960) {
            image = image.scaled(
                QSize(1280, 960),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
        }
        return image;
    }));
}

void MainWindow::startCapture(domain::CaptureRun captureRun)
{
    if (captureThread_ && captureThread_->isRunning()) {
        throw std::logic_error("A capture sequence is already running");
    }

    captureExposureText_ = QStringLiteral("%1 s").arg(
        static_cast<double>(captureRun.settings().exposure.count()) / 1'000'000.0,
        0,
        'f',
        3);
    captureStopRequested_ = std::make_shared<std::atomic_bool>(false);
    auto* worker = new presentation::CaptureWorker(
        applicationDataDirectory_,
        camera_,
        std::move(captureRun),
        captureStopRequested_);
    auto* thread = new QThread(this);
    thread->setObjectName(QStringLiteral("captureExecutionThread"));
    captureThread_ = thread;

    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &presentation::CaptureWorker::execute);
    connect(
        worker,
        &presentation::CaptureWorker::progressChanged,
        this,
        [this](quint32 completed,
               quint32 planned,
               quint32 failed,
               const QString& details,
               qint64 elapsedMilliseconds) {
            if (planned == 0) {
                captureProgressBar_->setRange(0, 0);
                captureProgressBar_->setFormat(QStringLiteral("Captura contínua"));
                captureSequenceMetricsLabel_->setText(
                    QStringLiteral("Concluídos: %1 · Falhas: %2 · Exposição: %3%4")
                        .arg(completed)
                        .arg(failed)
                        .arg(captureExposureText_)
                        .arg(details.isEmpty()
                                 ? QString{}
                                 : QStringLiteral("\n%1").arg(details)));
            } else {
                captureProgressBar_->setRange(0, static_cast<int>(planned));
                captureProgressBar_->setValue(static_cast<int>(completed));
                captureProgressBar_->setFormat(
                    QStringLiteral("%1 / %2 quadros")
                        .arg(completed)
                        .arg(planned));
                const quint32 pending = planned > completed + failed
                    ? planned - completed - failed
                    : 0;
                captureSequenceMetricsLabel_->setText(
                    QStringLiteral(
                        "Planejados: %1 · Concluídos: %2 · Pendentes: %3 · Falhas: %4%5")
                        .arg(planned)
                        .arg(completed)
                        .arg(pending)
                        .arg(failed)
                        .arg(details.isEmpty()
                                 ? QStringLiteral(" · Exposição: %1").arg(captureExposureText_)
                                 : QStringLiteral(" · Exposição: %1\n%2")
                                       .arg(captureExposureText_, details)));
            }
            const qint64 elapsedSeconds = elapsedMilliseconds / 1000;
            captureElapsedLabel_->setText(
                QStringLiteral("Tempo decorrido: %1:%2")
                    .arg(elapsedSeconds / 60, 2, 10, QLatin1Char('0'))
                    .arg(elapsedSeconds % 60, 2, 10, QLatin1Char('0')));
        },
        Qt::QueuedConnection);
    connect(
        worker,
        &presentation::CaptureWorker::statisticsChanged,
        this,
        [this](const QString& details) {
            captureStatisticsLabel_->setText(details);
        },
        Qt::QueuedConnection);
    connect(
        worker,
        &presentation::CaptureWorker::previewAvailable,
        this,
        [this](const QString& imagePath) {
            decodeCapturePreview(imagePath);
        },
        Qt::QueuedConnection);
    connect(
        worker,
        &presentation::CaptureWorker::captureCompleted,
        this,
        [this](quint32 completed, bool paused) {
            captureStatusLabel_->setText(
                paused
                    ? QStringLiteral("Captura pausada · %1 quadros concluídos.").arg(completed)
                    : QStringLiteral("Captura concluída · %1 quadros salvos.").arg(completed));
        },
        Qt::QueuedConnection);
    connect(
        worker,
        &presentation::CaptureWorker::captureFailed,
        this,
        [this](const QString& message) {
            captureStatusLabel_->setStyleSheet(QStringLiteral("color:#ff8e8e;"));
            captureStatusLabel_->setText(
                QStringLiteral("A captura foi interrompida: %1").arg(message));
        },
        Qt::QueuedConnection);
    connect(
        worker,
        &presentation::CaptureWorker::finished,
        thread,
        &QThread::quit,
        Qt::DirectConnection);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread] {
        if (captureThread_ == thread) {
            captureThread_ = nullptr;
        }
        thread->deleteLater();
        captureStartButton_->setEnabled(true);
        captureBackButton_->setEnabled(true);
        captureStopButton_->setEnabled(false);
        captureStopButton_->hide();
        captureProgressBar_->show();
        homeNavigationButton_->setEnabled(true);
        libraryNavigationButton_->setEnabled(true);
        captureStopRequested_.reset();
        captureUiTimer_->stop();
        if (closeAfterCapture_) {
            QTimer::singleShot(0, this, &QWidget::close);
        }
    });

    captureStatusLabel_->setStyleSheet(QStringLiteral("color:#49e3b6;"));
    captureStatusLabel_->setText(QStringLiteral("Executando sequência…"));
    captureSequenceMetricsLabel_->setText(QStringLiteral("Preparando sequência…"));
    captureElapsedTimer_.start();
    captureElapsedLabel_->setText(QStringLiteral("Tempo decorrido: 00:00"));
    captureUiTimer_->start();
    captureProgressBar_->setRange(0, 0);
    captureProgressBar_->setFormat(QStringLiteral("Preparando sequência…"));
    captureProgressBar_->show();
    captureStartButton_->setEnabled(false);
    captureBackButton_->setEnabled(false);
    captureStopButton_->setEnabled(true);
    captureStopButton_->show();
    homeNavigationButton_->setEnabled(false);
    libraryNavigationButton_->setEnabled(false);
    thread->start();
}

void MainWindow::requestCaptureStop()
{
    if (!captureStopRequested_) {
        return;
    }
    captureStopRequested_->store(true);
    captureStopButton_->setEnabled(false);
    captureStatusLabel_->setText(
        QStringLiteral("Parada solicitada; o quadro em andamento será concluído."));
}

void MainWindow::openSession(const std::string& sessionId)
{
    try {
        const auto record = sessionService_.openSession(sessionId);
        activeSessionId_ = sessionId;
        activeSessionLabel_->setText(
            QStringLiteral("Sessão aberta: %1 (%2)")
                .arg(QString::fromStdString(record.session.name()), stageName(record.session.stage())));
        refreshSessions();
        if (sessionRecoveryService_) {
            inspectSessionRecovery(record);
            return;
        }
        showCaptureConfigurationPage(sessionId, false);
    } catch (const std::exception& error) {
        QMessageBox::warning(
            this,
            QStringLiteral("Não foi possível abrir a sessão"),
            QString::fromUtf8(error.what()));
    }
}

void MainWindow::inspectSessionRecovery(const domain::SessionRecord& session)
{
    const auto report = sessionRecoveryService_->inspect(
        session,
        sessionService_.sessionDirectory(session));
    if (!report.requiresDecision()) {
        showCaptureConfigurationPage(session.session.id(), false);
        return;
    }

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("sessionRecoveryDialog"));
    dialog.setWindowTitle(QStringLiteral("Retomar sessão"));
    dialog.setModal(true);
    dialog.setMinimumWidth(520);
    dialog.resize(700, 480);
    auto* layout = new QVBoxLayout(&dialog);
    auto* summary = makeTextLabel(
        QStringLiteral("Esta sessão tem sequências incompletas. Quadros concluídos não serão repetidos; "
                       "quadros interrompidos permanecem inconclusivos."),
        QStringLiteral("recoverySummary"),
        &dialog);
    summary->setWordWrap(true);
    layout->addWidget(summary);

    auto* runSelector = new QComboBox(&dialog);
    runSelector->setObjectName(QStringLiteral("recoveryRunSelector"));
    std::size_t totalPending = 0;
    std::size_t totalInterrupted = 0;
    for (const application::RecoverableCaptureRun& run : report.incompleteRuns) {
        totalPending += run.pendingFrameIds.size();
        totalInterrupted += run.interruptedFrameIds.size();
        if (!run.canContinue) {
            continue;
        }
        runSelector->addItem(
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
    layout->addWidget(makeTextLabel(
        QStringLiteral(
            "Pendentes: %1 · inconclusivos: %2 · quadros com falha: %3 · inconsistências nos arquivos: %4")
            .arg(totalPending)
            .arg(totalInterrupted)
            .arg(failedFrameCount)
            .arg(fileIssueCount),
        {},
        &dialog));
    layout->addWidget(runSelector);

    auto* findings = new QListWidget(&dialog);
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

    auto* buttons = new QDialogButtonBox(&dialog);
    auto* continueButton = buttons->addButton(
        QStringLiteral("Continuar pendentes"),
        QDialogButtonBox::AcceptRole);
    continueButton->setObjectName(QStringLiteral("continueRecoveryButton"));
    continueButton->setEnabled(runSelector->count() > 0);
    auto* newSequenceButton = buttons->addButton(
        QStringLiteral("Criar nova sequência"),
        QDialogButtonBox::DestructiveRole);
    newSequenceButton->setObjectName(QStringLiteral("newRecoverySequenceButton"));
    auto* cancelButton = buttons->addButton(
        QStringLiteral("Cancelar"),
        QDialogButtonBox::RejectRole);
    cancelButton->setObjectName(QStringLiteral("cancelRecoveryButton"));
    layout->addWidget(buttons);

    connect(continueButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(newSequenceButton, &QPushButton::clicked, &dialog, [&dialog] {
        dialog.done(2);
    });
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    const int choice = dialog.exec();
    if (choice == QDialog::Accepted) {
        const domain::CaptureRunId runId = runSelector->currentData().toString().toStdString();
        const auto captureRun = captureConfigurationService_.findCaptureRun(runId);
        if (!captureRun || !report.canContinue(runId)
            || captureRun->state() == domain::CaptureRunState::Completed
            || captureRun->state() == domain::CaptureRunState::Failed) {
            throw std::runtime_error(
                "A sequência selecionada não pode mais ser retomada; reabra a sessão.");
        }
        sessionRecoveryService_->recordDecision(
            report,
            application::RecoveryDecision::Continue,
            runId);
        showCaptureConfigurationPage(session.session.id(), false);
        startCapture(*captureRun);
        return;
    }
    if (choice == 2) {
        sessionRecoveryService_->recordDecision(
            report,
            application::RecoveryDecision::NewSequence);
        showCaptureConfigurationPage(session.session.id(), false);
        return;
    }

    sessionRecoveryService_->recordDecision(
        report,
        application::RecoveryDecision::Cancel);
    showSessionsPage();
}

void MainWindow::showSessionInformation(const std::string& sessionId)
{
    try {
        const auto sessions = sessionService_.listSessions();
        const auto session = std::find_if(
            sessions.cbegin(),
            sessions.cend(),
            [&sessionId](const domain::SessionRecord& candidate) {
                return candidate.session.id() == sessionId;
            });
        if (session == sessions.cend()) {
            throw std::runtime_error("Session does not exist");
        }
        const auto target = sessionService_.targetForSession(sessionId);
        const auto createdAt = std::chrono::duration_cast<std::chrono::milliseconds>(
            session->session.createdAt().time_since_epoch()).count();
        const auto updatedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
            session->session.updatedAt().time_since_epoch()).count();
        const QString details = QStringLiteral(
            "Nome da sessão: %1\nDescrição: %2\nEtapa: %3\nEstado: %4\nCriada: %5\nAtualizada: %6")
            .arg(
                QString::fromStdString(target.name()),
                QString::fromStdString(session->session.name()),
                stageName(session->session.stage()),
                session->isOpen ? QStringLiteral("Aberta") : QStringLiteral("Fechada"),
                QDateTime::fromMSecsSinceEpoch(createdAt).toUTC().toString(Qt::ISODateWithMs),
                QDateTime::fromMSecsSinceEpoch(updatedAt).toUTC().toString(Qt::ISODateWithMs));
        QMessageBox::information(this, QStringLiteral("Informações da sessão"), details);
    } catch (const std::exception& error) {
        QMessageBox::warning(
            this,
            QStringLiteral("Não foi possível carregar as informações"),
            QString::fromUtf8(error.what()));
    }
}

void MainWindow::deleteSession(const std::string& sessionId)
{
    const auto confirmation = QMessageBox::warning(
        this,
        QStringLiteral("Excluir sessão"),
        QStringLiteral(
            "A sessão e todos os arquivos de imagem associados serão excluídos permanentemente. "
            "Deseja continuar?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (confirmation != QMessageBox::Yes) {
        return;
    }

    try {
        sessionService_.deleteSession(sessionId);
        if (activeSessionId_ == sessionId) {
            activeSessionId_.clear();
            activeSessionLabel_->setText(QStringLiteral("Nenhuma sessão aberta"));
        }
        refreshSessions();
    } catch (const std::exception& error) {
        QMessageBox::critical(
            this,
            QStringLiteral("Não foi possível excluir a sessão"),
            QString::fromUtf8(error.what()));
        try {
            refreshSessions();
        } catch (const std::exception& refreshError) {
            QMessageBox::warning(
                this,
                QStringLiteral("Não foi possível atualizar as sessões"),
                QString::fromUtf8(refreshError.what()));
        }
    }
}

void MainWindow::showHomePage()
{
    setNavigationSelection(true);
    QWidget* oldHomePage = homePage_;
    pages_->removeWidget(oldHomePage);
    homePage_ = createHomePage();
    pages_->insertWidget(0, homePage_);
    oldHomePage->deleteLater();
    pages_->setCurrentWidget(homePage_);
}

void MainWindow::showSessionsPage()
{
    setNavigationSelection(false);
    pages_->setCurrentWidget(sessionsPage_);
    try {
        refreshSessions();
    } catch (const std::exception& error) {
        QMessageBox::critical(
            this,
            QStringLiteral("Não foi possível carregar as sessões"),
            QString::fromUtf8(error.what()));
    }
}

void MainWindow::setNavigationSelection(bool homeSelected)
{
    homeNavigationButton_->setProperty("active", homeSelected);
    libraryNavigationButton_->setProperty("active", !homeSelected);
    for (QPushButton* button : {homeNavigationButton_, libraryNavigationButton_}) {
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

void MainWindow::refreshSessions()
{
    sessionList_->clear();
    for (const domain::SessionRecord& record : sessionService_.listSessions()) {
        const QString name = QString::fromStdString(record.session.name());
        const QString sessionId = QString::fromStdString(record.session.id());
        auto* item = new QListWidgetItem(sessionList_);
        item->setSizeHint(QSize(0, 58));
        item->setData(Qt::UserRole, QString::fromStdString(record.session.id()));

        auto* row = new QWidget(sessionList_);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 5, 5, 5);
        rowLayout->setSpacing(10);
        auto* detailsLayout = new QVBoxLayout;
        detailsLayout->setContentsMargins(0, 0, 0, 0);
        detailsLayout->setSpacing(2);
        detailsLayout->addWidget(makeTextLabel(
            name,
            QStringLiteral("sessionName"),
            row));
        detailsLayout->addWidget(makeTextLabel(
            QStringLiteral("%1 · %2")
                .arg(stageName(record.session.stage()),
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
            openSession(sessionId.toStdString());
        });
        connect(informationAction, &QAction::triggered, this, [this, sessionId] {
            showSessionInformation(sessionId.toStdString());
        });
        connect(deleteAction, &QAction::triggered, this, [this, sessionId] {
            deleteSession(sessionId.toStdString());
        });
        rowLayout->addWidget(menuButton);
        sessionList_->setItemWidget(item, row);
    }
}
