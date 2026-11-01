#include "presentation/views/capture_configuration_view.h"

#include "presentation/styles/theme.h"

#include <QBoxLayout>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <utility>

namespace presentation {
namespace {

QFrame* makeCard(const QString& objectName, const QString& title, QWidget* parent)
{
    auto* card = theme::makeCard(objectName, parent);
    card->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 10);
    layout->setSpacing(6);
    auto* heading = theme::makeTextLabel(title, objectName + QStringLiteral("Title"), card);
    heading->setObjectName(objectName + QStringLiteral("Title"));
    heading->setStyleSheet(QStringLiteral("font-size:11pt;font-weight:600;"));
    layout->addWidget(heading);
    return card;
}

QVBoxLayout* cardLayout(QFrame* card)
{
    return qobject_cast<QVBoxLayout*>(card->layout());
}

QString frameTypeName(domain::FrameType type)
{
    switch (type) {
    case domain::FrameType::Light:
        return QStringLiteral("Light");
    case domain::FrameType::Dark:
        return QStringLiteral("Dark");
    case domain::FrameType::Bias:
        return QStringLiteral("Bias");
    case domain::FrameType::Flat:
        return QStringLiteral("Flat");
    case domain::FrameType::DarkFlat:
        return QStringLiteral("Dark Flat");
    }
    return {};
}

} // namespace

CaptureConfigurationView::CaptureConfigurationView(QWidget* parent)
    : QScrollArea(parent)
{
    setObjectName(QStringLiteral("captureConfigurationPage"));
    setAttribute(Qt::WA_StyledBackground, true);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet(QStringLiteral(
        "#captureConfigurationPage, QScrollArea#captureConfigurationPage { background:#050d13; border:0; }"));

    QPalette scrollPalette = palette();
    scrollPalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    setPalette(scrollPalette);
    setAutoFillBackground(true);

    QPalette viewportPalette = viewport()->palette();
    viewportPalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    viewport()->setPalette(viewportPalette);
    viewport()->setAutoFillBackground(true);

    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("captureConfigurationContent"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setMinimumWidth(0);
    page->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    page->setAutoFillBackground(true);
    page->setStyleSheet(QStringLiteral(
        "#captureConfigurationContent { background:#050d13; }"
        "QLabel { color:#e5edf3; }"
        "QFrame#cameraConnectionCard, QFrame#frameTypeCard, QFrame#captureModeCard, "
        "QFrame#cameraSettingsCard, QFrame#sessionStatisticsCard, QFrame#liveViewCard, "
        "QFrame#lastCaptureCard { background:#0b1115; border:1px solid #293239; border-radius:8px; }"
        "QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox { background:#11171c; color:#e5edf3; "
        "border:1px solid #303940; border-radius:5px; padding:5px 7px; }"
        "QComboBox QAbstractItemView { background:#11171c; color:#e5edf3; "
        "selection-background-color:#0c2928; selection-color:#49e3b6; border:0; outline:0; }"
        "QPushButton#frameTypeButton { background:#171d22; color:#e5edf3; "
        "border:1px solid #20282e; border-radius:6px; padding:3px 2px; font-size:9pt; }"
        "QPushButton#frameTypeButton:checked { background:#102621; color:#e5fff7; "
        "border:2px solid #00e88d; }"
        "QRadioButton { color:#e5edf3; spacing:6px; font-size:10pt; }"
        "QRadioButton::indicator { width:14px; height:14px; border:2px solid #687786; border-radius:8px; }"
        "QRadioButton::indicator:checked { background:#19e590; border-color:#19e590; }"
        "QPushButton#reconnectCameraButton, QPushButton#captureSecondaryAction { background:#171d22; "
        "color:#e5edf3; border:1px solid #303940; border-radius:6px; padding:6px 10px; }"
        "QPushButton#reconnectCameraButton:hover, QPushButton#captureSecondaryAction:hover { "
        "border-color:#49e3b6; color:#49e3b6; }"
        "QPushButton#capturePrimaryAction { background:#087943; color:white; border:1px solid #04e68b; "
        "border-radius:7px; padding:9px 14px; font-size:11pt; font-weight:700; }"
        "QPushButton#capturePrimaryAction:hover { background:#0b8d51; }"
        "QPushButton#capturePrimaryAction:disabled { background:#25312d; color:#85948e; border-color:#384b43; }"
        "QProgressBar { background:#141a1e; color:#e5edf3; border:0; border-radius:7px; "
        "min-height:10px; text-align:center; }"
        "QProgressBar::chunk { background:#17df83; border-radius:7px; }"
        "QLabel#liveViewImage, QLabel#capturePreviewImage { background:#05090c; "
        "border:1px solid #252d32; border-radius:6px; }"));

    QPalette pagePalette = page->palette();
    pagePalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    page->setPalette(pagePalette);

    pageLayout_ = new QVBoxLayout(page);
    pageLayout_->setContentsMargins(8, 8, 8, 8);
    pageLayout_->setSpacing(8);

    auto* cameraCard = makeCard(
        QStringLiteral("cameraConnectionCard"),
        QStringLiteral("Câmera"),
        page);
    auto* cameraRow = new QHBoxLayout;
    cameraIndicatorLabel_ = theme::makeTextLabel(
        QStringLiteral("●"),
        QStringLiteral("cameraConnectionIndicator"),
        cameraCard);
    cameraIndicatorLabel_->setStyleSheet(QStringLiteral("color:#13e56e;font-size:16pt;"));
    cameraStatusLabel_ = theme::makeTextLabel(
        QStringLiteral("Câmera simulada conectada"),
        QStringLiteral("cameraConnectionStatus"),
        cameraCard);
    cameraStatusLabel_->setStyleSheet(QStringLiteral("color:#13e56e;font-size:11pt;font-weight:700;"));
    cameraDetailsLabel_ = theme::makeTextLabel(
        QStringLiteral("SimulatedCamera · ambiente de desenvolvimento"),
        QStringLiteral("cameraConnectionDetails"),
        cameraCard);
    cameraDetailsLabel_->setStyleSheet(QStringLiteral("color:#b4bec5;"));
    auto* cameraText = new QVBoxLayout;
    cameraText->setSpacing(2);
    cameraText->addWidget(cameraStatusLabel_);
    cameraText->addWidget(cameraDetailsLabel_);
    cameraRow->addWidget(cameraIndicatorLabel_);
    cameraRow->addLayout(cameraText, 1);
    reconnectCameraButton_ = new QPushButton(QStringLiteral("↻  Reconectar"), cameraCard);
    reconnectCameraButton_->setObjectName(QStringLiteral("reconnectCameraButton"));
    connect(reconnectCameraButton_, &QPushButton::clicked,
            this, &CaptureConfigurationView::reconnectCameraRequested);
    cameraRow->addWidget(reconnectCameraButton_);
    cardLayout(cameraCard)->addLayout(cameraRow);
    pageLayout_->addWidget(cameraCard);

    cameraInput_ = new QComboBox(page);
    cameraInput_->setObjectName(QStringLiteral("captureCameraInput"));
    cameraInput_->addItem(QStringLiteral("Simulada"), QStringLiteral("simulated"));
    cameraInput_->addItem(QStringLiteral("Câmera física (indisponível)"), QStringLiteral("physical"));
    if (auto* model = qobject_cast<QStandardItemModel*>(cameraInput_->model())) {
        if (QStandardItem* physicalItem = model->item(1)) {
            physicalItem->setFlags(physicalItem->flags() & ~Qt::ItemIsEnabled);
        }
    }
    cameraInput_->hide();

    contentColumnsLayout_ = new QHBoxLayout;
    contentColumnsLayout_->setSpacing(8);
    auto* leftColumn = new QWidget(page);
    auto* leftLayout = new QVBoxLayout(leftColumn);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(7);
    auto* rightColumn = new QWidget(page);
    auto* rightLayout = new QVBoxLayout(rightColumn);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(7);
    contentColumnsLayout_->addWidget(leftColumn, 5);
    contentColumnsLayout_->addWidget(rightColumn, 7);
    pageLayout_->addLayout(contentColumnsLayout_, 1);

    auto* frameCard = makeCard(
        QStringLiteral("frameTypeCard"),
        QStringLiteral("▣  Tipo de Quadro"),
        leftColumn);
    frameTypeInput_ = new QComboBox(frameCard);
    frameTypeInput_->setObjectName(QStringLiteral("frameTypeInput"));
    const std::array<std::pair<domain::FrameType, QString>, 5> frameTypes{{
        {domain::FrameType::Light, QStringLiteral("✦\nLight")},
        {domain::FrameType::Dark, QStringLiteral("☾\nDark")},
        {domain::FrameType::Bias, QStringLiteral("▬\nBias")},
        {domain::FrameType::Flat, QStringLiteral("▦\nFlat")},
        {domain::FrameType::DarkFlat, QStringLiteral("◉\nDark Flat")}
    }};
    frameTypeButtons_ = new QButtonGroup(frameCard);
    frameTypeButtons_->setExclusive(true);
    auto* frameGrid = new QGridLayout;
    frameGrid->setObjectName(QStringLiteral("frameTypeGrid"));
    frameGrid->setContentsMargins(0, 0, 0, 0);
    frameGrid->setSpacing(6);
    for (std::size_t index = 0; index < frameTypes.size(); ++index) {
        const auto [type, text] = frameTypes[index];
        frameTypeInput_->addItem(frameTypeName(type), static_cast<int>(type));
        auto* button = new QPushButton(text, frameCard);
        button->setObjectName(QStringLiteral("frameTypeButton"));
        button->setCheckable(true);
        button->setMinimumHeight(54);
        button->setAccessibleName(frameTypeName(type));
        frameTypeButtons_->addButton(button, static_cast<int>(type));
        frameGrid->addWidget(button, 0, static_cast<int>(index));
        connect(button, &QPushButton::clicked, this, [this, type] {
            const int index = frameTypeInput_->findData(static_cast<int>(type));
            if (index >= 0) {
                frameTypeInput_->setCurrentIndex(index);
            }
        });
    }
    frameTypeInput_->hide();
    connect(frameTypeInput_, &QComboBox::currentIndexChanged, this, [this] {
        const int selectedType = frameTypeInput_->currentData().toInt();
        if (QAbstractButton* button = frameTypeButtons_->button(selectedType)) {
            button->setChecked(true);
        }
    });
    frameTypeButtons_->button(static_cast<int>(domain::FrameType::Light))->setChecked(true);
    cardLayout(frameCard)->addLayout(frameGrid);
    leftLayout->addWidget(frameCard);

    auto* captureCard = makeCard(
        QStringLiteral("captureModeCard"),
        QStringLiteral("▶  Modo de Captura"),
        leftColumn);
    modeInput_ = new QComboBox(captureCard);
    modeInput_->setObjectName(QStringLiteral("captureModeInput"));
    modeInput_->addItem(QStringLiteral("Quantidade definida"), static_cast<int>(domain::CaptureMode::Finite));
    modeInput_->addItem(QStringLiteral("Contínua"), static_cast<int>(domain::CaptureMode::Continuous));
    modeInput_->hide();
    auto* modeRow = new QHBoxLayout;
    finiteModeButton_ = new QRadioButton(QStringLiteral("Quantidade Definida"), captureCard);
    continuousModeButton_ = new QRadioButton(QStringLiteral("Contínua"), captureCard);
    finiteModeButton_->setObjectName(QStringLiteral("finiteCaptureModeButton"));
    continuousModeButton_->setObjectName(QStringLiteral("continuousCaptureModeButton"));
    finiteModeButton_->setChecked(true);
    modeRow->addWidget(finiteModeButton_);
    modeRow->addStretch();
    modeRow->addWidget(continuousModeButton_);
    cardLayout(captureCard)->addLayout(modeRow);

    countInput_ = new QSpinBox(captureCard);
    countInput_->setObjectName(QStringLiteral("frameCountInput"));
    countInput_->setRange(1, 10000);
    countInput_->setValue(1);
    auto* countRow = new QFormLayout;
    countRow->addRow(QStringLiteral("Quantidade:"), countInput_);
    cardLayout(captureCard)->addLayout(countRow);
    connect(modeInput_, &QComboBox::currentIndexChanged, this, [this] {
        const auto mode = static_cast<domain::CaptureMode>(modeInput_->currentData().toInt());
        finiteModeButton_->setChecked(mode == domain::CaptureMode::Finite);
        continuousModeButton_->setChecked(mode == domain::CaptureMode::Continuous);
        countInput_->setEnabled(mode == domain::CaptureMode::Finite);
    });
    connect(finiteModeButton_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            modeInput_->setCurrentIndex(modeInput_->findData(
                static_cast<int>(domain::CaptureMode::Finite)));
        }
    });
    connect(continuousModeButton_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            modeInput_->setCurrentIndex(modeInput_->findData(
                static_cast<int>(domain::CaptureMode::Continuous)));
        }
    });

    leftLayout->addWidget(captureCard);

    auto* settingsCard = makeCard(
        QStringLiteral("cameraSettingsCard"),
        QStringLiteral("⚙  Configurações da Câmera"),
        leftColumn);
    auto* settingsGrid = new QGridLayout;
    settingsGrid->setContentsMargins(0, 0, 0, 0);
    settingsGrid->setHorizontalSpacing(8);
    settingsGrid->setVerticalSpacing(4);
    const auto addSettingLabel = [settingsCard, settingsGrid](const QString& text,
                                                              const QString& name,
                                                              int column) {
        auto* label = theme::makeTextLabel(text, name, settingsCard);
        settingsGrid->addWidget(label, 0, column);
    };
    addSettingLabel(QStringLiteral("ISO"), QStringLiteral("isoSettingLabel"), 0);
    addSettingLabel(QStringLiteral("Exposição"), QStringLiteral("exposureSettingLabel"), 1);
    addSettingLabel(QStringLiteral("Abertura (f/)"), QStringLiteral("apertureSettingLabel"), 2);

    exposureInput_ = new QDoubleSpinBox(settingsCard);
    exposureInput_->setObjectName(QStringLiteral("exposureInput"));
    exposureInput_->setDecimals(3);
    exposureInput_->setSingleStep(0.1);
    exposureInput_->setSuffix(QStringLiteral(" s"));
    exposureInput_->setFixedHeight(28);
    isoInput_ = new QComboBox(settingsCard);
    isoInput_->setObjectName(QStringLiteral("isoInput"));
    isoInput_->setFixedHeight(28);
    apertureInput_ = new QComboBox(settingsCard);
    apertureInput_->setObjectName(QStringLiteral("apertureInput"));
    apertureInput_->addItem(QStringLiteral("Indisponível"));
    apertureInput_->setFixedHeight(28);
    apertureInput_->setEnabled(false);
    apertureInput_->setToolTip(QStringLiteral(
        "O adaptador atual da câmera não informa nem controla a abertura."));
    settingsGrid->addWidget(isoInput_, 1, 0);
    settingsGrid->addWidget(exposureInput_, 1, 1);
    settingsGrid->addWidget(apertureInput_, 1, 2);
    for (int column = 0; column < 3; ++column) {
        settingsGrid->setColumnStretch(column, 1);
    }
    cardLayout(settingsCard)->addLayout(settingsGrid);
    leftLayout->addWidget(settingsCard);

    sessionStatisticsCard_ = makeCard(
        QStringLiteral("sessionStatisticsCard"),
        QStringLiteral("▥  Informações da Sessão"),
        leftColumn);
    captureStatisticsLabel_ = theme::makeTextLabel(
        QStringLiteral("As estatísticas aparecerão após carregar os dados da sessão."),
        QStringLiteral("captureStatistics"),
        sessionStatisticsCard_);
    captureStatisticsLabel_->setWordWrap(true);
    captureStatisticsLabel_->setMinimumHeight(0);
    cardLayout(sessionStatisticsCard_)->addWidget(captureStatisticsLabel_);
    leftLayout->addWidget(sessionStatisticsCard_);

    captureFeedbackCard_ = theme::makeCard(QStringLiteral("captureFeedbackCard"), leftColumn);
    captureFeedbackCard_->setAttribute(Qt::WA_StyledBackground, true);
    captureFeedbackCard_->setStyleSheet(QStringLiteral(
        "QFrame#captureFeedbackCard { background:#0b1115;border:1px solid #293239;border-radius:8px; }"));
    auto* feedbackLayout = new QVBoxLayout(captureFeedbackCard_);
    feedbackLayout->setContentsMargins(8, 6, 8, 6);
    feedbackLayout->setSpacing(4);
    errorLabel_ = theme::makeTextLabel({}, QStringLiteral("captureValidationError"), captureFeedbackCard_);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(QStringLiteral("color:#ff8e8e;"));
    captureStatusLabel_ = theme::makeTextLabel(
        {},
        QStringLiteral("captureConfigurationStatus"),
        captureFeedbackCard_);
    captureStatusLabel_->setWordWrap(true);
    captureStatusLabel_->setStyleSheet(QStringLiteral("color:#49e3b6;"));
    feedbackLayout->addWidget(errorLabel_);
    feedbackLayout->addWidget(captureStatusLabel_);
    captureFeedbackCard_->hide();
    leftLayout->addWidget(captureFeedbackCard_);

    captureStopButton_ = new QPushButton(
        QStringLiteral("■  Parar após o quadro atual"),
        page);
    captureStopButton_->setObjectName(QStringLiteral("stopCaptureButton"));
    captureStopButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background:#351416;color:#ff646b;border:1px solid #a32b31;"
        "border-radius:6px;padding:6px 10px; }"));
    captureStopButton_->hide();
    captureStopButton_->hide();
    connect(captureStopButton_, &QPushButton::clicked,
            this, &CaptureConfigurationView::stopCaptureRequested);

    auto* liveViewCard = makeCard(
        QStringLiteral("liveViewCard"),
        QStringLiteral("▣  Live View"),
        rightColumn);
    liveViewStatusLabel_ = theme::makeTextLabel(
        QStringLiteral("Live View indisponível nesta câmera."),
        QStringLiteral("liveViewStatus"),
        liveViewCard);
    liveViewStatusLabel_->setStyleSheet(QStringLiteral("color:#9eabb3;"));
    auto* liveViewHeader = new QHBoxLayout;
    liveViewHeader->addWidget(liveViewStatusLabel_, 1);
    liveViewStartButton_ = new QPushButton(QStringLiteral("▶  Ativar"), liveViewCard);
    liveViewStartButton_->setObjectName(QStringLiteral("startLiveViewButton"));
    liveViewStopButton_ = new QPushButton(QStringLiteral("■  Encerrar"), liveViewCard);
    liveViewStopButton_->setObjectName(QStringLiteral("stopLiveViewButton"));
    liveViewStopButton_->setStyleSheet(QStringLiteral(
        "QPushButton { color:#ff646b;background:#291518;border:1px solid #a32b31;"
        "border-radius:6px;padding:7px 10px; }"));
    connect(liveViewStartButton_, &QPushButton::clicked,
            this, &CaptureConfigurationView::startLiveViewRequested);
    connect(liveViewStopButton_, &QPushButton::clicked,
            this, &CaptureConfigurationView::stopLiveViewRequested);
    liveViewHeader->addWidget(liveViewStartButton_);
    liveViewHeader->addWidget(liveViewStopButton_);
    cardLayout(liveViewCard)->addLayout(liveViewHeader);
    liveViewImageLabel_ = new QLabel(liveViewCard);
    liveViewImageLabel_->setObjectName(QStringLiteral("liveViewImage"));
    liveViewImageLabel_->setAlignment(Qt::AlignCenter);
    liveViewImageLabel_->setMinimumHeight(190);
    liveViewImageLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    liveViewImageLabel_->setText(QStringLiteral(
        "◉\n\nVisualização ao vivo não fornecida pelo adaptador da câmera"));
    liveViewImageLabel_->setStyleSheet(QStringLiteral(
        "color:#8e9ba2;font-size:12pt;background:#05090c;border:1px solid #252d32;"
        "border-radius:6px;"));
    cardLayout(liveViewCard)->addWidget(liveViewImageLabel_, 1);
    rightLayout->addWidget(liveViewCard, 3);

    auto* previewCard = makeCard(
        QStringLiteral("lastCaptureCard"),
        QStringLiteral("▧  Último Quadro Capturado"),
        rightColumn);
    auto* previewRow = new QHBoxLayout;
    capturePreviewImageLabel_ = new QLabel(previewCard);
    capturePreviewImageLabel_->setObjectName(QStringLiteral("capturePreviewImage"));
    capturePreviewImageLabel_->setAlignment(Qt::AlignCenter);
    capturePreviewImageLabel_->setMinimumSize(90, 110);
    capturePreviewImageLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    capturePreviewImageLabel_->setText(QStringLiteral("Nenhum quadro capturado"));
    capturePreviewMetadataLabel_ = theme::makeTextLabel(
        QStringLiteral("Tipo: —\nISO: —\nExposição: —\nData/Hora: —"),
        QStringLiteral("capturePreviewMetadata"),
        previewCard);
    capturePreviewMetadataLabel_->setWordWrap(true);
    capturePreviewMetadataLabel_->setMinimumWidth(110);
    capturePreviewMetadataLabel_->setMaximumWidth(170);
    previewRow->addWidget(capturePreviewImageLabel_, 1);
    previewRow->addWidget(capturePreviewMetadataLabel_);
    cardLayout(previewCard)->addLayout(previewRow, 1);
    capturePreviewStatusLabel_ = theme::makeTextLabel(
        QStringLiteral("Nenhuma imagem disponível."),
        QStringLiteral("capturePreviewStatus"),
        previewCard);
    capturePreviewStatusLabel_->setAlignment(Qt::AlignCenter);
    capturePreviewStatusLabel_->setWordWrap(true);
    cardLayout(previewCard)->addWidget(capturePreviewStatusLabel_);
    rightLayout->addWidget(previewCard, 2);

    auto* footer = new QHBoxLayout;
    footer->setSpacing(7);
    captureBackButton_ = new QPushButton(QStringLiteral("⌂  Menu Principal"), page);
    captureBackButton_->setObjectName(QStringLiteral("captureBackButton"));
    captureBackButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background:#171d22;color:#e5edf3;border:1px solid #303940;"
        "border-radius:6px;padding:9px 11px; }"
        "QPushButton:hover { border-color:#49e3b6;color:#49e3b6; }"));
    connect(captureBackButton_, &QPushButton::clicked,
            this, &CaptureConfigurationView::backRequested);
    captureStartButton_ = new QPushButton(QStringLiteral("▶  Iniciar Captura"), page);
    captureStartButton_->setObjectName(QStringLiteral("startCaptureButton"));
    captureStartButton_->setStyleSheet(QStringLiteral(
        "QPushButton#startCaptureButton { background:#087943;color:white;border:1px solid #04e68b;"
        "border-radius:7px;padding:9px 14px;font-size:11pt;font-weight:700; }"
        "QPushButton#startCaptureButton:hover { background:#0b8d51; }"
        "QPushButton#startCaptureButton:disabled { background:#25312d;color:#85948e;border-color:#384b43; }"));
    connect(captureStartButton_, &QPushButton::clicked,
            this, &CaptureConfigurationView::onStartCaptureClicked);
    stackingButton_ = new QPushButton(QStringLiteral("▱  Ir para Empilhamento"), page);
    stackingButton_->setObjectName(QStringLiteral("stackingNavigationButton"));
    stackingButton_->setEnabled(false);
    stackingButton_->setToolTip(QStringLiteral("Empilhamento estará disponível em uma etapa futura."));
    stackingButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background:#171d22;color:#e5edf3;border:1px solid #303940;"
        "border-radius:6px;padding:9px 11px; }"
        "QPushButton:disabled { color:#879097; }"));
    auto* captureActionLayout = new QHBoxLayout;
    captureActionLayout->setSpacing(6);
    captureActionLayout->addWidget(captureStartButton_, 1);
    captureActionLayout->addWidget(captureStopButton_);
    footer->addWidget(captureBackButton_, 1);
    footer->addLayout(captureActionLayout, 2);
    footer->addWidget(stackingButton_, 1);
    pageLayout_->addLayout(footer);

    setWidget(page);
}

void CaptureConfigurationView::resizeEvent(QResizeEvent* event)
{
    QScrollArea::resizeEvent(event);
    const bool compact = viewport()->height() < 760;
    pageLayout_->setContentsMargins(
        compact ? 6 : 8,
        compact ? 6 : 8,
        compact ? 6 : 8,
        compact ? 6 : 8);
    pageLayout_->setSpacing(compact ? 6 : 8);
    if (contentColumnsLayout_) {
        contentColumnsLayout_->setDirection(
            viewport()->width() < 900
                ? QBoxLayout::TopToBottom
                : QBoxLayout::LeftToRight);
        contentColumnsLayout_->setSpacing(compact ? 6 : 8);
    }
    for (QAbstractButton* button : frameTypeButtons_->buttons()) {
        button->setMinimumHeight(compact ? 34 : 42);
    }
    if (liveViewImageLabel_) {
        liveViewImageLabel_->setMinimumHeight(compact ? 130 : 190);
    }
    if (capturePreviewImageLabel_) {
        capturePreviewImageLabel_->setMinimumSize(90, compact ? 80 : 110);
    }
    for (const QString& name : {
             QStringLiteral("cameraConnectionCard"),
             QStringLiteral("frameTypeCard"),
             QStringLiteral("captureModeCard"),
             QStringLiteral("cameraSettingsCard"),
             QStringLiteral("sessionStatisticsCard"),
             QStringLiteral("liveViewCard"),
             QStringLiteral("lastCaptureCard")}) {
        if (auto* card = findChild<QFrame*>(name)) {
            if (auto* cardBoxLayout = qobject_cast<QVBoxLayout*>(card->layout())) {
                const int margin = compact ? 7 : 10;
                cardBoxLayout->setContentsMargins(margin, margin, margin, margin);
                cardBoxLayout->setSpacing(compact ? 4 : 6);
            }
        }
    }
    updatePreviewPixmap();
}

void CaptureConfigurationView::setCapabilities(
    const domain::CameraCapabilities& capabilities,
    const domain::CameraSettings& currentSettings)
{
    exposureAvailable_ = capabilities.exposure.state != domain::CapabilityState::Unavailable;
    exposureInput_->setEnabled(exposureAvailable_);
    exposureInput_->setReadOnly(capabilities.exposure.state == domain::CapabilityState::ReadOnly);
    if (exposureAvailable_) {
        const double minimum = static_cast<double>(
            capabilities.exposure.minimum.value_or(std::chrono::microseconds{1000}).count())
            / 1'000'000.0;
        const double maximum = static_cast<double>(
            capabilities.exposure.maximum.value_or(std::chrono::minutes{30}).count())
            / 1'000'000.0;
        exposureInput_->setRange(minimum, maximum);
        exposureInput_->setValue(currentSettings.exposure
                ? static_cast<double>(currentSettings.exposure->count()) / 1'000'000.0
                : minimum);
    } else {
        exposureInput_->setRange(0.0, 0.0);
    }

    const bool isoAvailable = capabilities.iso.state != domain::CapabilityState::Unavailable;
    isoInput_->clear();
    isoInput_->setEnabled(isoAvailable
        && capabilities.iso.state == domain::CapabilityState::Supported);
    if (isoAvailable) {
        if (capabilities.iso.state == domain::CapabilityState::Supported) {
            isoInput_->addItem(QStringLiteral("Não informar"), QVariant{});
            for (const unsigned int iso : capabilities.iso.values) {
                isoInput_->addItem(QString::number(iso), iso);
            }
        } else if (currentSettings.iso) {
            isoInput_->addItem(QString::number(*currentSettings.iso), *currentSettings.iso);
        } else {
            isoInput_->addItem(QStringLiteral("ISO indisponível"));
            isoInput_->setEnabled(false);
        }
        if (currentSettings.iso) {
            const int currentIndex = isoInput_->findData(*currentSettings.iso);
            if (currentIndex >= 0) {
                isoInput_->setCurrentIndex(currentIndex);
            }
        }
    } else {
        isoInput_->addItem(QStringLiteral("Não suportado"));
    }

    cameraDetailsLabel_->setText(QStringLiteral("SimulatedCamera · ambiente de desenvolvimento"));
    setCameraConnectionStatus(QStringLiteral("Câmera simulada conectada"), true);
    liveViewAvailable_ = capabilities.liveView == domain::CapabilityState::Supported;
    if (!liveViewAvailable_) {
        setLiveViewStatusText(QStringLiteral("Live View indisponível nesta câmera."));
    } else {
        setLiveViewStatusText(QStringLiteral("Live View desligado."));
    }
    updateLiveViewControls(liveViewAvailable_, liveViewActive_, false, false);
}

void CaptureConfigurationView::setCameraConnectionStatus(const QString& text, bool connected)
{
    cameraConnected_ = connected;
    cameraStatusLabel_->setText(text);
    cameraIndicatorLabel_->setStyleSheet(QStringLiteral("color:%1;font-size:16pt;")
        .arg(connected ? QStringLiteral("#13e56e") : QStringLiteral("#ff7777")));
    cameraStatusLabel_->setStyleSheet(QStringLiteral("color:%1;font-size:11pt;font-weight:700;")
        .arg(connected ? QStringLiteral("#13e56e") : QStringLiteral("#ff7777")));
}

void CaptureConfigurationView::setBackButtonText(const QString& text)
{
    if (captureBackButton_) {
        captureBackButton_->setText(text);
    }
}

void CaptureConfigurationView::setCaptureStartButtonText(const QString& text)
{
    if (captureStartButton_) {
        captureStartButton_->setText(text);
    }
}

void CaptureConfigurationView::setLiveViewStatusText(const QString& text)
{
    if (liveViewStatusLabel_) {
        liveViewStatusLabel_->setText(text);
    }
}

void CaptureConfigurationView::updateLiveViewControls(
    bool available,
    bool active,
    bool pending,
    bool captureRunning,
    bool reconnectingCamera)
{
    liveViewAvailable_ = available;
    liveViewActive_ = active;
    const bool cameraBusy = captureRunning || pending || active || reconnectingCamera;
    if (liveViewStartButton_) {
        liveViewStartButton_->setEnabled(available && !cameraBusy);
    }
    if (liveViewStopButton_) {
        liveViewStopButton_->setEnabled(available && active && !pending
            && !captureRunning && !reconnectingCamera);
    }
    if (captureStartButton_) {
        captureStartButton_->setEnabled(cameraConnected_ && exposureAvailable_ && !cameraBusy);
    }
    if (captureBackButton_) {
        captureBackButton_->setEnabled(!cameraBusy);
    }
    if (reconnectCameraButton_) {
        reconnectCameraButton_->setEnabled(!cameraBusy);
    }
    if (liveViewImageLabel_) {
        if (!available) {
            liveViewImageLabel_->setText(QStringLiteral(
                "◉\n\nVisualização ao vivo não fornecida pelo adaptador da câmera"));
        } else if (active) {
            liveViewImageLabel_->setText(QStringLiteral(
                "Live View ativo\n\nO adaptador confirmou a conexão, mas não fornece quadros de vídeo."));
        } else if (reconnectingCamera) {
            liveViewImageLabel_->setText(QStringLiteral("Reconectando câmera…"));
        } else if (pending) {
            liveViewImageLabel_->setText(QStringLiteral("Conectando ao Live View…"));
        } else if (captureRunning) {
            liveViewImageLabel_->setText(QStringLiteral("Live View indisponível durante a captura."));
        } else {
            liveViewImageLabel_->setText(QStringLiteral("Live View desligado."));
        }
    }
}

void CaptureConfigurationView::setValidationError(const QString& error)
{
    if (errorLabel_) {
        errorLabel_->setText(error);
        captureFeedbackCard_->setVisible(!error.isEmpty() || !captureStatusLabel_->text().isEmpty());
    }
}

void CaptureConfigurationView::setCaptureStatus(const QString& status, const QString& color)
{
    if (captureStatusLabel_) {
        captureStatusLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(color));
        captureStatusLabel_->setText(status);
        captureFeedbackCard_->setVisible(!status.isEmpty() || !errorLabel_->text().isEmpty());
    }
}

void CaptureConfigurationView::setStatistics(const QString& statistics)
{
    if (captureStatisticsLabel_) {
        captureStatisticsLabel_->setText(statistics);
    }
}

void CaptureConfigurationView::setPreviewImage(const QImage& image)
{
    previewImage_ = image;
    updatePreviewPixmap();
}

void CaptureConfigurationView::setPreviewMetadata(const QString& metadata)
{
    if (capturePreviewMetadataLabel_) {
        capturePreviewMetadataLabel_->setText(metadata);
    }
}

void CaptureConfigurationView::setPreviewStatus(const QString& status)
{
    if (capturePreviewStatusLabel_) {
        capturePreviewStatusLabel_->setText(status);
    }
}

void CaptureConfigurationView::clearPreview()
{
    previewImage_ = {};
    if (capturePreviewImageLabel_) {
        capturePreviewImageLabel_->setPixmap({});
        capturePreviewImageLabel_->setText(QStringLiteral("Nenhum quadro capturado"));
    }
}

void CaptureConfigurationView::updatePreviewPixmap()
{
    if (!capturePreviewImageLabel_ || previewImage_.isNull()) {
        return;
    }
    const QSize target = capturePreviewImageLabel_->size().expandedTo(QSize(1, 1));
    capturePreviewImageLabel_->setText({});
    capturePreviewImageLabel_->setPixmap(
        QPixmap::fromImage(previewImage_).scaled(
            target,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
}

void CaptureConfigurationView::onStartCaptureClicked()
{
    errorLabel_->clear();
    captureStatusLabel_->clear();
    captureFeedbackCard_->hide();
    try {
        const auto frameType = static_cast<domain::FrameType>(frameTypeInput_->currentData().toInt());
        const auto mode = static_cast<domain::CaptureMode>(modeInput_->currentData().toInt());
        const auto exposure = std::chrono::microseconds{
            static_cast<std::int64_t>(std::llround(exposureInput_->value() * 1'000'000.0))};
        const std::optional<unsigned int> iso = isoInput_->isEnabled()
                && isoInput_->currentData().isValid()
            ? std::optional<unsigned int>{isoInput_->currentData().toUInt()}
            : std::nullopt;
        const std::optional<std::uint32_t> plannedCount =
            mode == domain::CaptureMode::Finite
            ? std::optional<std::uint32_t>{static_cast<std::uint32_t>(countInput_->value())}
            : std::nullopt;
        const domain::CaptureSettings settings{
            frameType,
            exposure,
            iso,
            mode,
            plannedCount};
        emit startCaptureRequested(settings);
    } catch (const std::exception& error) {
        errorLabel_->setText(QString::fromUtf8(error.what()));
    }
}

void CaptureConfigurationView::setCaptureStarted()
{
    captureStatusLabel_->setStyleSheet(QStringLiteral("color:#49e3b6;"));
    captureStatusLabel_->setText(QStringLiteral("Executando sequência…"));
    captureFeedbackCard_->show();
    captureStartButton_->setEnabled(false);
    captureBackButton_->setEnabled(false);
    stackingButton_->setEnabled(false);
    captureStopButton_->setEnabled(true);
    captureStopButton_->show();
}

void CaptureConfigurationView::setCaptureFinished()
{
    captureStartButton_->setEnabled(true);
    captureBackButton_->setEnabled(true);
    stackingButton_->setEnabled(false);
    captureStopButton_->setEnabled(false);
    captureStopButton_->hide();
}

} // namespace presentation
