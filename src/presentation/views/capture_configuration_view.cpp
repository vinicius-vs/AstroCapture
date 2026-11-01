#include "presentation/views/capture_configuration_view.h"

#include "presentation/styles/theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <cmath>

namespace presentation {

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
    layout->addWidget(theme::makeTextLabel(
        QStringLiteral("Defina a câmera e os parâmetros da sequência."),
        QStringLiteral("captureConfigurationDescription"),
        page));

    cameraInput_ = new QComboBox(page);
    cameraInput_->setObjectName(QStringLiteral("captureCameraInput"));
    cameraInput_->addItem(QStringLiteral("Simulada"), QStringLiteral("simulated"));
    cameraInput_->addItem(QStringLiteral("Câmera física (indisponível)"), QStringLiteral("physical"));
    if (auto* model = qobject_cast<QStandardItemModel*>(cameraInput_->model())) {
        if (QStandardItem* physicalItem = model->item(1)) {
            physicalItem->setFlags(physicalItem->flags() & ~Qt::ItemIsEnabled);
        }
    }

    cameraStatusLabel_ = theme::makeTextLabel(
        {},
        QStringLiteral("captureCameraCapabilities"),
        page);
    cameraStatusLabel_->setWordWrap(true);

    frameTypeInput_ = new QComboBox(page);
    frameTypeInput_->setObjectName(QStringLiteral("frameTypeInput"));
    frameTypeInput_->addItem(QStringLiteral("Light"), static_cast<int>(domain::FrameType::Light));
    frameTypeInput_->addItem(QStringLiteral("Dark"), static_cast<int>(domain::FrameType::Dark));
    frameTypeInput_->addItem(QStringLiteral("Flat"), static_cast<int>(domain::FrameType::Flat));
    frameTypeInput_->addItem(QStringLiteral("Bias"), static_cast<int>(domain::FrameType::Bias));
    frameTypeInput_->addItem(QStringLiteral("Dark Flat"), static_cast<int>(domain::FrameType::DarkFlat));

    modeInput_ = new QComboBox(page);
    modeInput_->setObjectName(QStringLiteral("captureModeInput"));
    modeInput_->addItem(QStringLiteral("Quantidade definida"), static_cast<int>(domain::CaptureMode::Finite));
    modeInput_->addItem(QStringLiteral("Contínua"), static_cast<int>(domain::CaptureMode::Continuous));

    exposureInput_ = new QDoubleSpinBox(page);
    exposureInput_->setObjectName(QStringLiteral("exposureInput"));
    exposureInput_->setDecimals(3);
    exposureInput_->setSingleStep(0.1);
    exposureInput_->setSuffix(QStringLiteral(" s"));

    isoInput_ = new QComboBox(page);
    isoInput_->setObjectName(QStringLiteral("isoInput"));

    countInput_ = new QSpinBox(page);
    countInput_->setObjectName(QStringLiteral("frameCountInput"));
    countInput_->setRange(1, 10000);
    countInput_->setValue(1);
    connect(modeInput_, &QComboBox::currentIndexChanged, page, [this] {
        const auto mode = static_cast<domain::CaptureMode>(modeInput_->currentData().toInt());
        countInput_->setEnabled(mode == domain::CaptureMode::Finite);
    });

    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    form->addRow(QStringLiteral("Câmera"), cameraInput_);
    form->addRow(QString{}, cameraStatusLabel_);
    form->addRow(QStringLiteral("Tipo de quadro"), frameTypeInput_);
    form->addRow(QStringLiteral("Modo"), modeInput_);
    form->addRow(QStringLiteral("Exposição"), exposureInput_);
    form->addRow(QStringLiteral("ISO"), isoInput_);
    form->addRow(QStringLiteral("Quantidade"), countInput_);
    layout->addLayout(form);

    errorLabel_ = theme::makeTextLabel({}, QStringLiteral("captureValidationError"), page);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(QStringLiteral("color:#ff8e8e;"));
    layout->addWidget(errorLabel_);

    captureStatusLabel_ = theme::makeTextLabel(
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

    captureSequenceMetricsLabel_ = theme::makeTextLabel(
        QStringLiteral("Aguardando início da sequência."),
        QStringLiteral("captureSequenceMetrics"),
        page);
    captureSequenceMetricsLabel_->setWordWrap(true);

    captureElapsedLabel_ = theme::makeTextLabel(
        QStringLiteral("Tempo decorrido: 00:00"),
        QStringLiteral("captureElapsedTime"),
        page);
    layout->addWidget(captureSequenceMetricsLabel_);
    layout->addWidget(captureElapsedLabel_);

    captureStatisticsLabel_ = theme::makeTextLabel(
        QStringLiteral("As estatísticas aparecerão após carregar os dados da sessão."),
        QStringLiteral("captureStatistics"),
        page);
    captureStatisticsLabel_->setWordWrap(true);
    layout->addWidget(captureStatisticsLabel_);

    layout->addWidget(theme::makeTextLabel(
        QStringLiteral("Live View"),
        QStringLiteral("sectionTitle"),
        page));

    liveViewStatusLabel_ = theme::makeTextLabel(
        {},
        QStringLiteral("liveViewStatus"),
        page);
    liveViewStatusLabel_->setWordWrap(true);
    layout->addWidget(liveViewStatusLabel_);

    auto* liveViewButtons = new QHBoxLayout;
    liveViewStartButton_ = new QPushButton(QStringLiteral("Iniciar Live View"), page);
    liveViewStartButton_->setObjectName(QStringLiteral("startLiveViewButton"));
    liveViewStopButton_ = new QPushButton(QStringLiteral("Encerrar Live View"), page);
    liveViewStopButton_->setObjectName(QStringLiteral("stopLiveViewButton"));
    connect(liveViewStartButton_, &QPushButton::clicked, this, &CaptureConfigurationView::startLiveViewRequested);
    connect(liveViewStopButton_, &QPushButton::clicked, this, &CaptureConfigurationView::stopLiveViewRequested);
    liveViewButtons->addWidget(liveViewStartButton_);
    liveViewButtons->addWidget(liveViewStopButton_);
    liveViewButtons->addStretch();
    layout->addLayout(liveViewButtons);

    layout->addWidget(theme::makeTextLabel(
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

    capturePreviewStatusLabel_ = theme::makeTextLabel(
        QStringLiteral("Nenhuma imagem disponível."),
        QStringLiteral("capturePreviewStatus"),
        page);
    capturePreviewStatusLabel_->setAlignment(Qt::AlignCenter);
    capturePreviewStatusLabel_->setWordWrap(true);
    layout->addWidget(capturePreviewStatusLabel_);

    auto* buttons = new QDialogButtonBox(page);
    captureStartButton_ = buttons->addButton(
        QStringLiteral("Iniciar captura"),
        QDialogButtonBox::AcceptRole);
    captureStartButton_->setObjectName(QStringLiteral("startCaptureButton"));

    captureBackButton_ = buttons->addButton(QDialogButtonBox::Cancel);
    captureBackButton_->setText(QStringLiteral("Voltar"));
    captureBackButton_->setObjectName(QStringLiteral("captureBackButton"));

    captureStopButton_ = buttons->addButton(
        QStringLiteral("Parar após o quadro atual"),
        QDialogButtonBox::ActionRole);
    captureStopButton_->setObjectName(QStringLiteral("stopCaptureButton"));
    captureStopButton_->hide();

    connect(captureStopButton_, &QPushButton::clicked, this, &CaptureConfigurationView::stopCaptureRequested);
    connect(captureBackButton_, &QPushButton::clicked, this, &CaptureConfigurationView::backRequested);
    connect(captureStartButton_, &QPushButton::clicked, this, &CaptureConfigurationView::onStartCaptureClicked);

    layout->addWidget(buttons);
    setWidget(page);
}

void CaptureConfigurationView::setCapabilities(
    const domain::CameraCapabilities& capabilities,
    const domain::CameraSettings& currentSettings)
{
    exposureAvailable_ = capabilities.exposure.state != domain::CapabilityState::Unavailable;
    exposureInput_->setEnabled(exposureAvailable_);
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
    if (capabilities.exposure.state == domain::CapabilityState::ReadOnly) {
        exposureInput_->setReadOnly(true);
    }

    cameraStatusLabel_->setText(
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
                    : QStringLiteral("suportado")));

    isoInput_->clear();
    const bool isoAvailable = capabilities.iso.state != domain::CapabilityState::Unavailable;
    if (isoAvailable) {
        if (capabilities.iso.state == domain::CapabilityState::Supported) {
            isoInput_->addItem(QStringLiteral("Não informar"), QVariant{});
            for (const unsigned int iso : capabilities.iso.values) {
                isoInput_->addItem(QString::number(iso), iso);
            }
        } else if (currentSettings.iso) {
            isoInput_->addItem(QString::number(*currentSettings.iso), *currentSettings.iso);
            isoInput_->setEnabled(false);
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
        isoInput_->setEnabled(false);
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
    bool captureRunning)
{
    const bool cameraBusy = captureRunning || pending || active;
    if (liveViewStartButton_) {
        liveViewStartButton_->setEnabled(available && !pending && !active && !captureRunning);
    }
    if (liveViewStopButton_) {
        liveViewStopButton_->setEnabled(available && active && !pending && !captureRunning);
    }
    if (captureStartButton_) {
        captureStartButton_->setEnabled(exposureAvailable_ && !active && !pending && !captureRunning);
    }
    if (captureBackButton_) {
        captureBackButton_->setEnabled(!cameraBusy);
    }
}

void CaptureConfigurationView::setValidationError(const QString& error)
{
    if (errorLabel_) {
        errorLabel_->setText(error);
    }
}

void CaptureConfigurationView::setCaptureStatus(const QString& status, const QString& color)
{
    if (captureStatusLabel_) {
        captureStatusLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(color));
        captureStatusLabel_->setText(status);
    }
}

void CaptureConfigurationView::setSequenceMetrics(const QString& metrics)
{
    if (captureSequenceMetricsLabel_) {
        captureSequenceMetricsLabel_->setText(metrics);
    }
}

void CaptureConfigurationView::setElapsedTime(const QString& elapsed)
{
    if (captureElapsedLabel_) {
        captureElapsedLabel_->setText(elapsed);
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
    if (capturePreviewImageLabel_) {
        capturePreviewImageLabel_->setPixmap(
            QPixmap::fromImage(image).scaled(
                QSize(960, 640),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation));
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
    if (capturePreviewImageLabel_) {
        capturePreviewImageLabel_->clear();
    }
}

void CaptureConfigurationView::onStartCaptureClicked()
{
    errorLabel_->clear();
    captureStatusLabel_->clear();
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

void CaptureConfigurationView::setCaptureStarted(const QString& exposureText)
{
    Q_UNUSED(exposureText);
    captureStatusLabel_->setStyleSheet(QStringLiteral("color:#49e3b6;"));
    captureStatusLabel_->setText(QStringLiteral("Executando sequência…"));
    captureSequenceMetricsLabel_->setText(QStringLiteral("Preparando sequência…"));
    captureProgressBar_->setRange(0, 0);
    captureProgressBar_->setFormat(QStringLiteral("Preparando sequência…"));
    captureProgressBar_->show();
    captureStartButton_->setEnabled(false);
    captureBackButton_->setEnabled(false);
    captureStopButton_->setEnabled(true);
    captureStopButton_->show();
}

void CaptureConfigurationView::updateProgress(
    quint32 completed,
    quint32 planned,
    quint32 failed,
    const QString& exposureText,
    const QString& details,
    qint64 elapsedMilliseconds)
{
    if (planned == 0) {
        captureProgressBar_->setRange(0, 0);
        captureProgressBar_->setFormat(QStringLiteral("Captura contínua"));
        captureSequenceMetricsLabel_->setText(
            QStringLiteral("Concluídos: %1 · Falhas: %2 · Exposição: %3%4")
                .arg(completed)
                .arg(failed)
                .arg(exposureText)
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
                         ? QStringLiteral(" · Exposição: %1").arg(exposureText)
                         : QStringLiteral(" · Exposição: %1\n%2")
                               .arg(exposureText, details)));
    }
    const qint64 elapsedSeconds = elapsedMilliseconds / 1000;
    captureElapsedLabel_->setText(
        QStringLiteral("Tempo decorrido: %1:%2")
            .arg(elapsedSeconds / 60, 2, 10, QLatin1Char('0'))
            .arg(elapsedSeconds % 60, 2, 10, QLatin1Char('0')));
}

void CaptureConfigurationView::setCaptureFinished()
{
    captureStartButton_->setEnabled(true);
    captureBackButton_->setEnabled(true);
    captureStopButton_->setEnabled(false);
    captureStopButton_->hide();
    captureProgressBar_->show();
}

} // namespace presentation
