#include "presentation/main_window.h"

#include "app/application_metadata.h"
#include "application/capture_configuration_service.h"
#include "application/capture_preview_service.h"
#include "application/session_recovery_service.h"
#include "application/session_service.h"
#include "presentation/capture_worker.h"
#include "presentation/dialogs/session_configuration_dialog.h"
#include "presentation/dialogs/session_recovery_dialog.h"
#include "presentation/styles/theme.h"
#include "presentation/views/capture_configuration_view.h"
#include "presentation/views/home_view.h"
#include "presentation/views/sessions_view.h"
#include "presentation/widgets/resize_aware_widget.h"

#include <QBoxLayout>
#include <QCloseEvent>
#include <QDateTime>
#include <QFrame>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <utility>
#include <vector>

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

    QPalette windowPalette = palette();
    windowPalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    windowPalette.setColor(QPalette::WindowText, QColor(QStringLiteral("#e5edf3")));
    windowPalette.setColor(QPalette::Base, QColor(QStringLiteral("#071119")));
    setPalette(windowPalette);
    setAutoFillBackground(true);
    setStyleSheet(QString::fromLatin1(presentation::theme::DashboardStyle));

    auto* shell = new presentation::ResizeAwareWidget(this);
    shell->setObjectName(QStringLiteral("applicationShell"));
    shell->setMinimumWidth(0);
    shell->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    shell->setStyleSheet(QString::fromLatin1(presentation::theme::DashboardStyle));

    auto* shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(10, 10, 10, 10);
    shellLayout->setSpacing(12);

    auto* sidebar = new QFrame(shell);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(225);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(15, 22, 15, 14);
    sidebarLayout->setSpacing(8);

    auto* brandMark = presentation::theme::makeTextLabel(
        QStringLiteral("◉ AstroCapture"),
        QStringLiteral("brandMark"),
        sidebar);
    auto* brandTagline = presentation::theme::makeTextLabel(
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
    auto* readyIndicator = presentation::theme::makeTextLabel(QStringLiteral("●"), {}, sidebar);
    readyIndicator->setStyleSheet(QStringLiteral("color:#40e0b1;"));
    readyLayout->addWidget(readyIndicator);
    auto* readyStatusText = presentation::theme::makeTextLabel(QStringLiteral("Aplicativo pronto"), {}, sidebar);
    readyLayout->addWidget(readyStatusText);
    readyLayout->addStretch();
    sidebarLayout->addLayout(readyLayout);

    auto* sidebarStatusDetail = presentation::theme::makeTextLabel(
        QStringLiteral("A câmera será conectada no fluxo de captura."),
        QStringLiteral("sidebarStatusDetail"),
        sidebar);
    sidebarLayout->addWidget(sidebarStatusDetail);

    pages_ = new QStackedWidget(shell);
    pages_->setObjectName(QStringLiteral("mainPages"));
    pages_->setMinimumWidth(0);
    pages_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    pages_->setAutoFillBackground(true);
    QPalette pagesPalette = pages_->palette();
    pagesPalette.setColor(QPalette::Window, QColor(QStringLiteral("#050d13")));
    pages_->setPalette(pagesPalette);

    homeView_ = new presentation::HomeView(sessionService_, pages_);
    sessionsView_ = new presentation::SessionsView(sessionService_, pages_);
    pages_->addWidget(homeView_);
    pages_->addWidget(sessionsView_);
    pages_->setCurrentWidget(homeView_);

    shellLayout->addWidget(sidebar);
    shellLayout->addWidget(pages_, 1);
    setCentralWidget(shell);

    connect(homeNavigationButton_, &QPushButton::clicked, this, &MainWindow::showHomePage);
    connect(libraryNavigationButton_, &QPushButton::clicked, this, &MainWindow::showSessionsPage);
    connect(homeView_, &presentation::HomeView::startSessionRequested, this, &MainWindow::openSessionConfigurationDialog);
    connect(homeView_, &presentation::HomeView::openLibraryRequested, this, &MainWindow::showSessionsPage);
    connect(homeView_, &presentation::HomeView::openCurrentSessionRequested, this,
            [this](const std::string& sessionId) { openSession(sessionId); });
    connect(sessionsView_, &presentation::SessionsView::openSessionRequested, this, &MainWindow::openSession);
    connect(sessionsView_, &presentation::SessionsView::showInformationRequested, this, &MainWindow::showSessionInformation);
    connect(sessionsView_, &presentation::SessionsView::deleteSessionRequested, this, &MainWindow::deleteSession);

    captureUiTimer_ = new QTimer(this);
    captureUiTimer_->setInterval(1000);
    connect(captureUiTimer_, &QTimer::timeout, this, [this] {
        if (captureConfigurationView_ && captureElapsedTimer_.isValid()) {
            const qint64 elapsedSeconds = captureElapsedTimer_.elapsed() / 1000;
            captureConfigurationView_->setElapsedTime(
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
    if (captureThread_) {
        closeAfterCapture_ = true;
        requestCaptureStop();
        if (captureConfigurationView_) {
            captureConfigurationView_->setCaptureStatus(
                QStringLiteral("Aguardando o quadro atual terminar antes de fechar."));
        }
        event->ignore();
        return;
    }
    if (liveViewOperationPending_) {
        closeAfterLiveViewStop_ = true;
        event->ignore();
        return;
    }
    if (liveViewActive_) {
        closeAfterLiveViewStop_ = true;
        event->ignore();
        beginLiveViewOperation(false);
        return;
    }
    event->accept();
}

void MainWindow::openSessionConfigurationDialog()
{
    presentation::SessionConfigurationDialog dialog(sessionService_, this);
    if (dialog.exec() == QDialog::Accepted) {
        activeSessionId_ = dialog.createdSessionId();
        showCaptureConfigurationPage(activeSessionId_, true);
    }
}

void MainWindow::showCaptureConfigurationPage(const std::string& sessionId, bool returnHome)
{
    activeSessionId_ = sessionId;
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

    liveViewAvailable_ = capabilities.liveView == domain::CapabilityState::Supported;
    if (!liveViewAvailable_) {
        liveViewActive_ = false;
    }
    liveViewStatusText_ = liveViewAvailable_
        ? QStringLiteral("Live View desligado.")
        : QStringLiteral("Live View indisponível nesta câmera.");

    ++previewRequestId_;
    if (captureConfigurationView_) {
        pages_->removeWidget(captureConfigurationView_);
        delete captureConfigurationView_;
        captureConfigurationView_ = nullptr;
    }

    captureConfigurationView_ = new presentation::CaptureConfigurationView(pages_);
    captureConfigurationView_->setCapabilities(capabilities, currentSettings);
    captureConfigurationView_->setLiveViewStatusText(liveViewStatusText_);

    connect(captureConfigurationView_, &presentation::CaptureConfigurationView::startCaptureRequested,
            this, [this, sessionId](domain::CaptureSettings settings) {
        try {
            const auto captureRun = captureConfigurationService_.configureSequence(sessionId, settings);
            captureConfigurationView_->setValidationError({});
            startCapture(captureRun);
        } catch (const std::exception& error) {
            captureConfigurationView_->setValidationError(QString::fromUtf8(error.what()));
        }
    });
    connect(captureConfigurationView_, &presentation::CaptureConfigurationView::stopCaptureRequested,
            this, &MainWindow::requestCaptureStop);
    connect(captureConfigurationView_, &presentation::CaptureConfigurationView::backRequested,
            this, [this, returnHome] {
        if (returnHome) {
            showHomePage();
        } else {
            showSessionsPage();
        }
    });
    connect(captureConfigurationView_, &presentation::CaptureConfigurationView::startLiveViewRequested,
            this, [this] { beginLiveViewOperation(true); });
    connect(captureConfigurationView_, &presentation::CaptureConfigurationView::stopLiveViewRequested,
            this, [this] { beginLiveViewOperation(false); });

    pages_->addWidget(captureConfigurationView_);
    pages_->setCurrentWidget(captureConfigurationView_);
    setNavigationSelection(returnHome);
    updateLiveViewControls();

    QTimer::singleShot(0, this, [this, sessionId] {
        loadCapturePreview(sessionId);
    });
}

void MainWindow::beginLiveViewOperation(bool start)
{
    if (!liveViewAvailable_ || liveViewOperationPending_
        || (start && liveViewActive_) || (!start && !liveViewActive_)
        || captureThread_) {
        return;
    }

    liveViewOperationPending_ = true;
    liveViewStatusText_ = start
        ? QStringLiteral("Iniciando Live View…")
        : QStringLiteral("Encerrando Live View…");
    updateLiveViewControls();

    auto* watcher = new QFutureWatcher<domain::CameraOperationResult>(this);
    connect(watcher, &QFutureWatcher<domain::CameraOperationResult>::finished, this,
            [this, watcher, start] {
        const domain::CameraOperationResult result = watcher->result();
        watcher->deleteLater();
        liveViewOperationPending_ = false;

        const auto* error = std::get_if<domain::CameraError>(&result);
        if (error) {
            if (start && error->code == domain::CameraErrorCode::UnsupportedOperation) {
                liveViewAvailable_ = false;
                liveViewStatusText_ = QStringLiteral("Live View indisponível nesta câmera.");
            } else {
                liveViewStatusText_ = QStringLiteral("%1 falhou: %2")
                    .arg(
                        start ? QStringLiteral("Iniciar Live View")
                              : QStringLiteral("Encerrar Live View"),
                        QString::fromStdString(error->message));
            }
        } else {
            liveViewActive_ = start;
            liveViewStatusText_ = liveViewActive_
                ? QStringLiteral("Live View ativo.")
                : QStringLiteral("Live View desligado.");
        }
        updateLiveViewControls();

        if (!closeAfterLiveViewStop_) {
            return;
        }
        if (liveViewActive_) {
            if (!error) {
                beginLiveViewOperation(false);
            } else {
                closeAfterLiveViewStop_ = false;
                QMessageBox::warning(
                    this,
                    QStringLiteral("Não foi possível encerrar o Live View"),
                    QString::fromStdString(error->message));
            }
            return;
        }
        closeAfterLiveViewStop_ = false;
        QTimer::singleShot(0, this, &QWidget::close);
    });

    watcher->setFuture(QtConcurrent::run([this, start] {
        try {
            return start ? camera_.startLiveView() : camera_.stopLiveView();
        } catch (const std::exception& error) {
            return domain::CameraOperationResult{domain::CameraError{
                domain::CameraErrorCode::Unknown,
                error.what(),
                start ? "startLiveView" : "stopLiveView",
                std::nullopt}};
        }
    }));
}

void MainWindow::updateLiveViewControls()
{
    const bool captureRunning = captureThread_ != nullptr;
    const bool cameraBusy = captureRunning || liveViewOperationPending_ || liveViewActive_;

    if (captureConfigurationView_) {
        captureConfigurationView_->setLiveViewStatusText(liveViewStatusText_);
        captureConfigurationView_->updateLiveViewControls(
            liveViewAvailable_,
            liveViewActive_,
            liveViewOperationPending_,
            captureRunning);
    }

    if (homeNavigationButton_) {
        homeNavigationButton_->setEnabled(!cameraBusy);
    }
    if (libraryNavigationButton_) {
        libraryNavigationButton_->setEnabled(!cameraBusy);
    }
}

void MainWindow::loadCapturePreview(const std::string& sessionId)
{
    if (!captureConfigurationView_) {
        return;
    }
    if (!capturePreviewService_) {
        captureConfigurationView_->clearPreview();
        captureConfigurationView_->setPreviewStatus(QStringLiteral("Nenhuma imagem disponível."));
        return;
    }

    try {
        const auto imagePath = capturePreviewService_->latestCompletedImagePath(sessionId);
        if (!imagePath) {
            ++previewRequestId_;
            captureConfigurationView_->clearPreview();
            captureConfigurationView_->setPreviewStatus(QStringLiteral("Nenhuma imagem disponível."));
            return;
        }
        decodeCapturePreview(QString::fromStdString(imagePath->string()));
    } catch (const std::exception& error) {
        ++previewRequestId_;
        captureConfigurationView_->clearPreview();
        captureConfigurationView_->setPreviewStatus(
            QStringLiteral("Não foi possível localizar a pré-visualização: %1")
                .arg(QString::fromUtf8(error.what())));
    }
}

void MainWindow::decodeCapturePreview(const QString& imagePath)
{
    const quint64 requestId = ++previewRequestId_;
    if (captureConfigurationView_) {
        captureConfigurationView_->clearPreview();
        captureConfigurationView_->setPreviewStatus(QStringLiteral("Carregando pré-visualização…"));
    }

    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this,
            [this, watcher, requestId] {
        const QImage image = watcher->result();
        watcher->deleteLater();
        if (requestId != previewRequestId_ || !captureConfigurationView_) {
            return;
        }
        if (image.isNull()) {
            captureConfigurationView_->clearPreview();
            captureConfigurationView_->setPreviewStatus(
                QStringLiteral("Pré-visualização indisponível: arquivo inválido ou formato não suportado."));
            return;
        }
        captureConfigurationView_->setPreviewImage(image);
        captureConfigurationView_->setPreviewStatus(QStringLiteral("Último quadro concluído."));
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
    if (captureThread_) {
        throw std::logic_error("A capture sequence is already running");
    }
    if (liveViewActive_ || liveViewOperationPending_) {
        throw std::logic_error("Stop live view before starting a capture sequence");
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
            if (captureConfigurationView_) {
                captureConfigurationView_->updateProgress(
                    completed,
                    planned,
                    failed,
                    captureExposureText_,
                    details,
                    elapsedMilliseconds);
            }
        },
        Qt::QueuedConnection);
    connect(
        worker,
        &presentation::CaptureWorker::statisticsChanged,
        this,
        [this](const QString& details) {
            if (captureConfigurationView_) {
                captureConfigurationView_->setStatistics(details);
            }
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
            if (captureConfigurationView_) {
                captureConfigurationView_->setCaptureStatus(
                    paused
                        ? QStringLiteral("Captura pausada · %1 quadros concluídos.").arg(completed)
                        : QStringLiteral("Captura concluída · %1 quadros salvos.").arg(completed));
            }
        },
        Qt::QueuedConnection);
    connect(
        worker,
        &presentation::CaptureWorker::captureFailed,
        this,
        [this](const QString& message) {
            if (captureConfigurationView_) {
                captureConfigurationView_->setCaptureStatus(
                    QStringLiteral("A captura foi interrompida: %1").arg(message),
                    QStringLiteral("#ff8e8e"));
            }
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
        if (captureConfigurationView_) {
            captureConfigurationView_->setCaptureFinished();
        }
        homeNavigationButton_->setEnabled(true);
        libraryNavigationButton_->setEnabled(true);
        captureStopRequested_.reset();
        captureUiTimer_->stop();
        if (closeAfterCapture_) {
            QTimer::singleShot(0, this, &QWidget::close);
        }
        updateLiveViewControls();
    });

    if (captureConfigurationView_) {
        captureConfigurationView_->setCaptureStarted(captureExposureText_);
    }
    captureElapsedTimer_.start();
    captureUiTimer_->start();
    updateLiveViewControls();
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
    if (captureConfigurationView_) {
        captureConfigurationView_->setCaptureStatus(
            QStringLiteral("Parada solicitada; o quadro em andamento será concluído."));
    }
}

void MainWindow::openSession(const std::string& sessionId)
{
    try {
        const auto record = sessionService_.openSession(sessionId);
        activeSessionId_ = sessionId;
        sessionsView_->setActiveSessionText(
            QStringLiteral("Sessão aberta: %1 (%2)")
                .arg(QString::fromStdString(record.session.name()),
                     presentation::theme::stageName(record.session.stage())));
        sessionsView_->refresh();
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

    presentation::SessionRecoveryDialog dialog(report, this);
    const int choice = dialog.exec();
    if (choice == QDialog::Accepted) {
        const auto selectedRunId = dialog.selectedRunId();
        if (!selectedRunId) {
            throw std::runtime_error("A sequência selecionada não é válida.");
        }
        const auto captureRun = captureConfigurationService_.findCaptureRun(*selectedRunId);
        if (!captureRun || !report.canContinue(*selectedRunId)
            || captureRun->state() == domain::CaptureRunState::Completed
            || captureRun->state() == domain::CaptureRunState::Failed) {
            throw std::runtime_error(
                "A sequência selecionada não pode mais ser retomada; reabra a sessão.");
        }
        sessionRecoveryService_->recordDecision(
            report,
            application::RecoveryDecision::Continue,
            *selectedRunId);
        showCaptureConfigurationPage(session.session.id(), false);
        startCapture(*captureRun);
        return;
    }
    if (choice == presentation::SessionRecoveryDialog::NewSequenceCode) {
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
                presentation::theme::stageName(session->session.stage()),
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
            sessionsView_->setActiveSessionText(QStringLiteral("Nenhuma sessão aberta"));
        }
        sessionsView_->refresh();
    } catch (const std::exception& error) {
        QMessageBox::critical(
            this,
            QStringLiteral("Não foi possível excluir a sessão"),
            QString::fromUtf8(error.what()));
        try {
            sessionsView_->refresh();
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
    homeView_->refresh();
    pages_->setCurrentWidget(homeView_);
}

void MainWindow::showSessionsPage()
{
    setNavigationSelection(false);
    sessionsView_->refresh();
    pages_->setCurrentWidget(sessionsView_);
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
