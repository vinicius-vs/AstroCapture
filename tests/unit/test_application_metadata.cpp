#include "app/application_metadata.h"
#include "application/capture_frame_service.h"
#include "application/capture_configuration_service.h"
#include "application/capture_preview_service.h"
#include "application/session_recovery_service.h"
#include "application/session_service.h"
#include "infrastructure/camera/simulated_camera.h"
#include "infrastructure/logging/json_lines_logging_service.h"
#include "infrastructure/persistence/sqlite_database.h"
#include "infrastructure/persistence/sqlite_repositories.h"
#include "infrastructure/recovery/session_file_inspector.h"
#include "infrastructure/storage/image_storage.h"
#include "infrastructure/storage/storage_layout.h"
#include "presentation/capture_worker.h"
#include "presentation/main_window.h"

#include <QtTest>

#include <QComboBox>
#include <QBoxLayout>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolButton>

class ApplicationMetadataTest final : public QObject
{
    Q_OBJECT

private slots:
    void windowOpensAndShowsApplicationNameAndVersion()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        infrastructure::SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("test.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository repository(database);
        infrastructure::SqliteCaptureRunRepository captureRunRepository(database);
        infrastructure::SqliteFrameRepository frameRepository(database);
        infrastructure::SimulatedCamera camera;
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService service(repository, storageLayout);
        application::CaptureConfigurationService captureConfigurationService(
            repository, captureRunRepository, camera);
        MainWindow window(service, captureConfigurationService, camera, directory.path());
        QCOMPARE(window.windowTitle(), applicationTitle());

        const auto* welcomeTitle = window.findChild<QLabel*>(QStringLiteral("welcomeTitle"));
        QVERIFY(welcomeTitle);
        QCOMPARE(welcomeTitle->text(), QStringLiteral("Bem-vindo ao\nAstroCapture"));
        const auto* pages = window.findChild<QStackedWidget*>(QStringLiteral("mainPages"));
        QVERIFY(pages);
        QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("homePage"));
        const auto* homePage = qobject_cast<QScrollArea*>(pages->currentWidget());
        QVERIFY(homePage);
        QCOMPARE(
            homePage->viewport()->palette().color(QPalette::Window),
            QColor(QStringLiteral("#050d13")));

        window.show();
        QVERIFY(window.isVisible());
        window.close();
        QVERIFY(window.isHidden());
    }

    void captureWorkerKeepsUiResponsiveAndStopsAtFrameBoundary()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        infrastructure::SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("astrocapture.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository sessionRepository(database);
        infrastructure::SqliteCaptureRunRepository captureRunRepository(database);
        infrastructure::SqliteFrameRepository frameRepository(database);
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService sessionService(sessionRepository, storageLayout);
        const auto session = sessionService.createSession(
            QStringLiteral("M42"),
            QStringLiteral("Worker test"),
            domain::SessionStage::Capture);

        infrastructure::SimulatedCameraOptions cameraOptions;
        cameraOptions.exposureDelay = std::chrono::milliseconds{250};
        infrastructure::SimulatedCamera camera(cameraOptions);
        application::CaptureConfigurationService configurationService(
            sessionRepository,
            captureRunRepository,
            camera);
        const domain::CaptureRun captureRun = configurationService.configureSequence(
            session.session.id(),
            domain::CaptureSettings{
                domain::FrameType::Light,
                std::chrono::milliseconds{10},
                800,
                domain::CaptureMode::Continuous,
                std::nullopt});

        auto stopRequested = std::make_shared<std::atomic_bool>(false);
        auto* worker = new presentation::CaptureWorker(
            directory.path(),
            camera,
            captureRun,
            stopRequested);
        QThread thread;
        worker->moveToThread(&thread);

        QSignalSpy completedSpy(worker, &presentation::CaptureWorker::captureCompleted);
        QSignalSpy failedSpy(worker, &presentation::CaptureWorker::captureFailed);
        QSignalSpy threadFinishedSpy(&thread, &QThread::finished);
        QTimer heartbeat;
        heartbeat.setInterval(10);
        int heartbeatCount = 0;
        connect(&heartbeat, &QTimer::timeout, this, [&heartbeatCount] {
            ++heartbeatCount;
        });
        connect(&thread, &QThread::started, worker, &presentation::CaptureWorker::execute);
        connect(
            worker,
            &presentation::CaptureWorker::progressChanged,
            this,
            [stopRequested](quint32, quint32, quint32, const QString& details, qint64) {
                if (details.contains(QStringLiteral("Expondo"))) {
                    stopRequested->store(true);
                }
            },
            Qt::QueuedConnection);
        connect(
            worker,
            &presentation::CaptureWorker::finished,
            &thread,
            &QThread::quit,
            Qt::DirectConnection);
        connect(&thread, &QThread::finished, worker, &QObject::deleteLater);

        heartbeat.start();
        thread.start();
        QTRY_VERIFY_WITH_TIMEOUT(threadFinishedSpy.count() > 0, 3000);
        thread.wait();
        heartbeat.stop();

        QVERIFY2(failedSpy.isEmpty(), failedSpy.isEmpty() ? "" : qPrintable(failedSpy.first().first().toString()));
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.first().at(0).toUInt(), 1U);
        QCOMPARE(completedSpy.first().at(1).toBool(), true);
        QVERIFY(heartbeatCount > 0);
        const auto frames = frameRepository.findByCaptureRunId(captureRun.id());
        QCOMPARE(frames.size(), std::size_t{1});
        QCOMPARE(frames.front().state(), domain::FrameState::Completed);
        const auto persistedRun = captureRunRepository.findById(captureRun.id());
        QVERIFY(persistedRun);
        QCOMPARE(persistedRun->state(), domain::CaptureRunState::Paused);
    }

    void openingIncompleteSessionOffersAndExecutesSafeContinuation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        infrastructure::SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("astrocapture.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository sessionRepository(database);
        infrastructure::SqliteCaptureRunRepository captureRunRepository(database);
        infrastructure::SqliteFrameRepository frameRepository(database);
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService sessionService(sessionRepository, storageLayout);
        const auto session = sessionService.createSession(
            QStringLiteral("M42"),
            QStringLiteral("Recovery test"),
            domain::SessionStage::Capture);

        infrastructure::SimulatedCamera camera;
        application::CaptureConfigurationService configurationService(
            sessionRepository,
            captureRunRepository,
            camera);
        const domain::CaptureRun captureRun = configurationService.configureSequence(
            session.session.id(),
            domain::CaptureSettings{
                domain::FrameType::Light,
                std::chrono::milliseconds{10},
                800,
                domain::CaptureMode::Finite,
                2});

        infrastructure::JsonLinesLoggingService loggingService(directory.path());
        infrastructure::ImageStorage imageStorage(storageLayout);
        const domain::Target target = sessionService.targetForSession(session.session.id());
        domain::CaptureRun initialRun = *captureRunRepository.findById(captureRun.id());
        application::CaptureFrameService frameService(
            camera,
            captureRunRepository,
            frameRepository,
            imageStorage,
            loggingService,
            target,
            session.session,
            initialRun);
        const auto plannedFrames = frameRepository.findByCaptureRunId(captureRun.id());
        QCOMPARE(plannedFrames.size(), std::size_t{2});
        captureRunRepository.updateState(
            captureRun.id(),
            domain::CaptureRunState::Running);
        initialRun.start();
        const domain::Frame completedFrame = frameService.execute(plannedFrames.front().id());
        QCOMPARE(completedFrame.state(), domain::FrameState::Completed);

        const QString orphanDirectory = QDir(sessionService.sessionDirectory(session))
                                            .filePath(
                                                QStringLiteral("nights/2026-10-25/Light"));
        QVERIFY(QDir().mkpath(orphanDirectory));
        QFile orphanFile(QDir(orphanDirectory).filePath(QStringLiteral("orphan.pgm")));
        QVERIFY(orphanFile.open(QIODevice::WriteOnly));
        QCOMPARE(orphanFile.write("orphan"), qint64{6});
        orphanFile.close();

        infrastructure::LocalSessionFileInspector fileInspector;
        application::SessionRecoveryService recoveryService(
            captureRunRepository,
            frameRepository,
            fileInspector,
            loggingService);
        MainWindow window(
            sessionService,
            configurationService,
            camera,
            directory.path(),
            &recoveryService);
        window.show();

        auto* sessionList = window.findChild<QListWidget*>(QStringLiteral("sessionList"));
        QVERIFY(sessionList);
        QCOMPARE(sessionList->count(), 1);
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("sessionRecoveryDialog"));
            const auto* summary =
                dialog->findChild<QLabel*>(QStringLiteral("recoverySummary"));
            QVERIFY(summary);
            QVERIFY(summary->text().contains(QStringLiteral("não serão repetidos")));
            const auto* runSelector =
                dialog->findChild<QComboBox*>(QStringLiteral("recoveryRunSelector"));
            QVERIFY(runSelector);
            QCOMPARE(runSelector->count(), 1);
            QVERIFY(runSelector->itemText(0).contains(QStringLiteral("1 pendente(s)")));
            const auto* findings =
                dialog->findChild<QListWidget*>(QStringLiteral("recoveryFindings"));
            QVERIFY(findings);
            QVERIFY(findings->count() >= 2);
            bool orphanExplained = false;
            for (int index = 0; index < findings->count(); ++index) {
                orphanExplained = orphanExplained
                    || findings->item(index)->text().contains(
                        QStringLiteral("No completed frame record references this file."));
            }
            QVERIFY(orphanExplained);
            auto* continueButton =
                dialog->findChild<QPushButton*>(QStringLiteral("continueRecoveryButton"));
            QVERIFY(continueButton);
            QVERIFY(continueButton->isEnabled());
            continueButton->click();
        });
        QVERIFY(QMetaObject::invokeMethod(
            sessionList,
            "itemActivated",
            Qt::DirectConnection,
            Q_ARG(QListWidgetItem*, sessionList->item(0))));

        auto* status =
            window.findChild<QLabel*>(QStringLiteral("captureConfigurationStatus"));
        QVERIFY(status);
        QTRY_VERIFY2_WITH_TIMEOUT(
            status->text().contains(QStringLiteral("Captura concluída")),
            qPrintable(status->text()),
            3000);
        QTRY_VERIFY_WITH_TIMEOUT([&window] {
            auto* thread = window.findChild<QThread*>(
                QStringLiteral("captureExecutionThread"));
            return !thread || !thread->isRunning();
        }(), 3000);

        const auto persistedFrames = frameRepository.findByCaptureRunId(captureRun.id());
        QCOMPARE(persistedFrames.size(), std::size_t{2});
        QCOMPARE(persistedFrames[0].state(), domain::FrameState::Completed);
        QCOMPARE(persistedFrames[0].id(), completedFrame.id());
        QCOMPARE(persistedFrames[1].state(), domain::FrameState::Completed);
        QCOMPARE(captureRunRepository.findBySessionId(session.session.id()).size(), std::size_t{1});
        const auto persistedRun = captureRunRepository.findById(captureRun.id());
        QVERIFY(persistedRun);
        QCOMPARE(persistedRun->state(), domain::CaptureRunState::Completed);

        QFile logFile(loggingService.activeLogPath());
        QVERIFY(logFile.open(QIODevice::ReadOnly));
        const QByteArray logContent = logFile.readAll();
        QVERIFY(logContent.contains("\"event\":\"session.recovery.decision\""));
        QVERIFY(logContent.contains("\"state\":\"Continue\""));
        window.close();
    }

    void homeScreenAdaptsToNarrowWindowWidths()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        infrastructure::SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("test.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository repository(database);
        infrastructure::SqliteCaptureRunRepository captureRunRepository(database);
        infrastructure::SimulatedCamera camera;
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService service(repository, storageLayout);
        application::CaptureConfigurationService captureConfigurationService(
            repository, captureRunRepository, camera);
        MainWindow window(service, captureConfigurationService, camera, directory.path());

        auto* sidebar = window.findChild<QFrame*>(QStringLiteral("sidebar"));
        auto* homePage = window.findChild<QScrollArea*>(QStringLiteral("homePage"));
        auto* contentLayout = window.findChild<QBoxLayout*>(QStringLiteral("homeContentLayout"));
        auto* actionGrid = window.findChild<QGridLayout*>(QStringLiteral("homeActionGrid"));
        auto* thirdAction = window.findChild<QPushButton*>(QStringLiteral("sequencesAction"));
        QVERIFY(sidebar);
        QVERIFY(homePage);
        QVERIFY(contentLayout);
        QVERIFY(actionGrid);
        QVERIFY(thirdAction);

        window.show();
        window.resize(760, 600);
        QCoreApplication::processEvents();
        QCOMPARE(sidebar->width(), 72);
        QCOMPARE(contentLayout->direction(), QBoxLayout::TopToBottom);
        int row = -1;
        int column = -1;
        int rowSpan = 0;
        int columnSpan = 0;
        actionGrid->getItemPosition(
            actionGrid->indexOf(thirdAction->parentWidget()),
            &row,
            &column,
            &rowSpan,
            &columnSpan);
        QCOMPARE(row, 1);
        QCOMPARE(column, 0);

        window.resize(460, 600);
        QCoreApplication::processEvents();
        QCOMPARE(sidebar->width(), 56);
        QCOMPARE(homePage->widget()->width(), homePage->viewport()->width());
        actionGrid->getItemPosition(
            actionGrid->indexOf(thirdAction->parentWidget()),
            &row,
            &column,
            &rowSpan,
            &columnSpan);
        QCOMPARE(row, 2);
        QCOMPARE(column, 0);
        QVERIFY(homePage->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff);

        for (QFrame* frame : homePage->widget()->findChildren<QFrame*>()) {
            const QRect bounds(frame->mapTo(homePage->widget(), QPoint{}), frame->size());
            QVERIFY2(
                bounds.left() >= 0 && bounds.right() < homePage->widget()->width(),
                qPrintable(QStringLiteral("%1 exceeds the home content width").arg(frame->objectName())));
        }
        QVERIFY(homePage->verticalScrollBar()->maximum() > 0);

        window.resize(320, 320);
        QCoreApplication::processEvents();
        QCOMPARE(window.width(), 320);
        QCOMPARE(homePage->widget()->width(), homePage->viewport()->width());
        QVERIFY(homePage->verticalScrollBar()->maximum() > 0);
        for (QFrame* frame : homePage->widget()->findChildren<QFrame*>()) {
            const QRect bounds(frame->mapTo(homePage->widget(), QPoint{}), frame->size());
            QVERIFY2(
                bounds.left() >= 0 && bounds.right() < homePage->widget()->width(),
                qPrintable(QStringLiteral("%1 exceeds the narrow home content width").arg(frame->objectName())));
        }
    }

    void createsListsAndOpensSessions()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        infrastructure::SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("test.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository repository(database);
        infrastructure::SqliteCaptureRunRepository captureRunRepository(database);
        infrastructure::SqliteFrameRepository frameRepository(database);
        infrastructure::SimulatedCamera camera;
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService service(repository, storageLayout);
        service.createSession(
            QStringLiteral("M42"),
            QStringLiteral("Outubro"),
            domain::SessionStage::Stacking);
        application::CaptureConfigurationService captureConfigurationService(
            repository, captureRunRepository, camera);
        application::CapturePreviewService previewService(
            captureRunRepository,
            frameRepository);
        MainWindow window(
            service,
            captureConfigurationService,
            camera,
            directory.path(),
            nullptr,
            &previewService);

        auto* libraryNavigation =
            window.findChild<QPushButton*>(QStringLiteral("libraryNavigationButton"));
        auto* sessionList = window.findChild<QListWidget*>(QStringLiteral("sessionList"));
        auto* activeSession = window.findChild<QLabel*>(QStringLiteral("activeSessionLabel"));
        QVERIFY(libraryNavigation);
        QVERIFY(sessionList);
        QVERIFY(activeSession);
        QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("openSessionButton")));
        QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("createSessionButton")));
        QVERIFY(!window.findChild<QLineEdit*>(QStringLiteral("targetNameInput")));
        QVERIFY(!window.findChild<QLineEdit*>(QStringLiteral("sessionNameInput")));
        QVERIFY(!window.findChild<QComboBox*>(QStringLiteral("workflowInput")));
        libraryNavigation->click();
        QCOMPARE(
            window.findChild<QStackedWidget*>(QStringLiteral("mainPages"))->currentWidget()->objectName(),
            QStringLiteral("sessionsPage"));
        QCOMPARE(libraryNavigation->property("active").toBool(), true);
        QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("homeNavigationButton"))
                     ->property("active").toBool());
        QCOMPARE(sessionList->count(), 1);
        const auto records = repository.findAll();
        QCOMPARE(records.size(), std::size_t{1});
        QCOMPARE(records.front().session.name(), std::string("Outubro"));
        QCOMPARE(records.front().session.stage(), domain::SessionStage::Stacking);
        QVERIFY(records.front().isOpen);

        const domain::Target target(
            records.front().session.targetId(),
            "M42");
        const QString sessionDirectory = storageLayout.sessionDirectory(target, records.front().session);
        QVERIFY(QDir(sessionDirectory).exists());

        auto* row = sessionList->itemWidget(sessionList->item(0));
        QVERIFY(row);
        auto* menuButton = row->findChild<QToolButton*>(QStringLiteral("sessionMenuButton"));
        QVERIFY(menuButton);
        QVERIFY(menuButton->menu());
        QCOMPARE(menuButton->menu()->actions().size(), 4);
        QCOMPARE(menuButton->menu()->actions().at(0)->text(), QStringLiteral("Abrir"));
        QCOMPARE(menuButton->menu()->actions().at(1)->text(), QStringLiteral("Informações"));
        QCOMPARE(menuButton->menu()->actions().at(3)->text(), QStringLiteral("Excluir"));

        repository.close(records.front().session.id());
        menuButton->menu()->actions().at(0)->trigger();
        auto* pages = window.findChild<QStackedWidget*>(QStringLiteral("mainPages"));
        QVERIFY(pages);
        QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("captureConfigurationPage"));
        QVERIFY(!QApplication::activeModalWidget());
        auto* liveViewStatus = window.findChild<QLabel*>(QStringLiteral("liveViewStatus"));
        auto* startLiveViewButton =
            window.findChild<QPushButton*>(QStringLiteral("startLiveViewButton"));
        auto* stopLiveViewButton =
            window.findChild<QPushButton*>(QStringLiteral("stopLiveViewButton"));
        QVERIFY(liveViewStatus);
        QVERIFY(startLiveViewButton);
        QVERIFY(stopLiveViewButton);
        QCOMPARE(liveViewStatus->text(), QStringLiteral("Live View indisponível nesta câmera."));
        QVERIFY(!startLiveViewButton->isEnabled());
        QVERIFY(!stopLiveViewButton->isEnabled());
        auto* previewStatus =
            window.findChild<QLabel*>(QStringLiteral("capturePreviewStatus"));
        QVERIFY(previewStatus);
        QTRY_COMPARE_WITH_TIMEOUT(
            previewStatus->text(),
            QStringLiteral("Nenhuma imagem disponível."),
            3000);
        QVERIFY(repository.findById(records.front().session.id())->isOpen);
        QVERIFY(activeSession->text().contains(QStringLiteral("Sessão aberta: Outubro")));
        auto* captureBackButton =
            window.findChild<QPushButton*>(QStringLiteral("captureBackButton"));
        QVERIFY(captureBackButton);
        captureBackButton->click();
        QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("sessionsPage"));

        row = sessionList->itemWidget(sessionList->item(0));
        QVERIFY(row);
        menuButton = row->findChild<QToolButton*>(QStringLiteral("sessionMenuButton"));
        QVERIFY(menuButton);
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Informações da sessão"));
            QVERIFY(dialog->text().contains(QStringLiteral("Nome da sessão: M42")));
            QVERIFY(dialog->text().contains(QStringLiteral("Descrição: Outubro")));
            dialog->accept();
        });
        menuButton->menu()->actions().at(1)->trigger();

        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Excluir sessão"));
            dialog->button(QMessageBox::Yes)->click();
        });
        menuButton->menu()->actions().at(3)->trigger();
        QVERIFY(repository.findAll().empty());
        QVERIFY(!QDir(sessionDirectory).exists());
        QCOMPARE(sessionList->count(), 0);
        QCOMPARE(activeSession->text(), QStringLiteral("Nenhuma sessão aberta"));
    }

    void opensLatestCompletedImageAsPreviewWithoutChangingOriginal()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        infrastructure::SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("astrocapture.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository sessionRepository(database);
        infrastructure::SqliteCaptureRunRepository captureRunRepository(database);
        infrastructure::SqliteFrameRepository frameRepository(database);
        infrastructure::SimulatedCamera camera;
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService sessionService(sessionRepository, storageLayout);
        const auto session = sessionService.createSession(
            QStringLiteral("M42"),
            QStringLiteral("Preview test"),
            domain::SessionStage::Capture);
        application::CaptureConfigurationService configurationService(
            sessionRepository,
            captureRunRepository,
            camera);
        const auto captureRun = configurationService.configureSequence(
            session.session.id(),
            domain::CaptureSettings{
                domain::FrameType::Light,
                std::chrono::seconds{1},
                std::nullopt,
                domain::CaptureMode::Finite,
                1u});

        const QString imagePath = QDir(directory.path()).filePath(QStringLiteral("original.png"));
        QImage original(64, 48, QImage::Format_RGB32);
        original.fill(Qt::darkBlue);
        QVERIFY(original.save(imagePath));
        QFile imageFile(imagePath);
        QVERIFY(imageFile.open(QIODevice::ReadOnly));
        const QByteArray originalBytes = imageFile.readAll();
        imageFile.close();

        domain::Frame frame(
            "preview-frame",
            captureRun.id(),
            domain::FrameType::Light,
            std::chrono::seconds{1});
        frameRepository.create(frame);
        frame.startExposure(std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now()));
        frameRepository.update(frame);
        frame.startTransfer();
        frameRepository.update(frame);
        frame.startValidation();
        frameRepository.update(frame);
        frame.completeValidation(
            std::filesystem::path(imagePath.toStdString()),
            static_cast<std::uint64_t>(originalBytes.size()));
        frameRepository.update(frame);

        application::CapturePreviewService previewService(
            captureRunRepository,
            frameRepository);
        MainWindow window(
            sessionService,
            configurationService,
            camera,
            directory.path(),
            nullptr,
            &previewService);
        window.findChild<QPushButton*>(QStringLiteral("libraryNavigationButton"))->click();
        auto* sessionList = window.findChild<QListWidget*>(QStringLiteral("sessionList"));
        QVERIFY(sessionList);
        QVERIFY(sessionList->itemWidget(sessionList->item(0)));
        auto* menuButton = sessionList->itemWidget(sessionList->item(0))
                               ->findChild<QToolButton*>(QStringLiteral("sessionMenuButton"));
        QVERIFY(menuButton);
        QVERIFY(menuButton->menu());
        menuButton->menu()->actions().at(0)->trigger();

        auto* previewStatus = window.findChild<QLabel*>(QStringLiteral("capturePreviewStatus"));
        QVERIFY(previewStatus);
        QTRY_COMPARE_WITH_TIMEOUT(
            previewStatus->text(),
            QStringLiteral("Último quadro concluído."),
            3000);
        QFile unchangedImage(imagePath);
        QVERIFY(unchangedImage.open(QIODevice::ReadOnly));
        QCOMPARE(unchangedImage.readAll(), originalBytes);
        unchangedImage.close();

        QVERIFY(unchangedImage.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(unchangedImage.write("invalid image data"), qint64(18));
        unchangedImage.close();
        window.findChild<QPushButton*>(QStringLiteral("captureBackButton"))->click();
        QVERIFY(sessionList);
        auto* row = sessionList->itemWidget(sessionList->item(0));
        QVERIFY(row);
        menuButton = row->findChild<QToolButton*>(QStringLiteral("sessionMenuButton"));
        QVERIFY(menuButton);
        QVERIFY(menuButton->menu());
        menuButton->menu()->actions().at(0)->trigger();
        previewStatus = window.findChild<QLabel*>(QStringLiteral("capturePreviewStatus"));
        QVERIFY(previewStatus);
        QTRY_COMPARE_WITH_TIMEOUT(
            previewStatus->text(),
            QStringLiteral(
                "Pré-visualização indisponível: arquivo inválido ou formato não suportado."),
            3000);
    }

    void liveViewIsManualAndDisabledDuringCapture()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        infrastructure::SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("astrocapture.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository sessionRepository(database);
        infrastructure::SqliteCaptureRunRepository captureRunRepository(database);
        infrastructure::SqliteFrameRepository frameRepository(database);
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService sessionService(sessionRepository, storageLayout);
        const auto session = sessionService.createSession(
            QStringLiteral("M42"),
            QStringLiteral("Live View test"),
            domain::SessionStage::Capture);
        infrastructure::SimulatedCameraOptions cameraOptions;
        cameraOptions.liveViewSupported = true;
        cameraOptions.exposureDelay = std::chrono::milliseconds{200};
        infrastructure::SimulatedCamera camera(cameraOptions);
        application::CaptureConfigurationService configurationService(
            sessionRepository,
            captureRunRepository,
            camera);
        application::CapturePreviewService previewService(
            captureRunRepository,
            frameRepository);
        MainWindow window(
            sessionService,
            configurationService,
            camera,
            directory.path(),
            nullptr,
            &previewService);
        window.findChild<QPushButton*>(QStringLiteral("libraryNavigationButton"))->click();
        auto* sessionList = window.findChild<QListWidget*>(QStringLiteral("sessionList"));
        QVERIFY(sessionList);
        auto* row = sessionList->itemWidget(sessionList->item(0));
        QVERIFY(row);
        auto* sessionMenu = row->findChild<QToolButton*>(QStringLiteral("sessionMenuButton"));
        QVERIFY(sessionMenu);
        sessionMenu->menu()->actions().at(0)->trigger();

        auto* liveViewStatus = window.findChild<QLabel*>(QStringLiteral("liveViewStatus"));
        auto* startLiveViewButton =
            window.findChild<QPushButton*>(QStringLiteral("startLiveViewButton"));
        auto* stopLiveViewButton =
            window.findChild<QPushButton*>(QStringLiteral("stopLiveViewButton"));
        auto* startCaptureButton =
            window.findChild<QPushButton*>(QStringLiteral("startCaptureButton"));
        QVERIFY(liveViewStatus);
        QVERIFY(startLiveViewButton);
        QVERIFY(stopLiveViewButton);
        QVERIFY(startCaptureButton);
        QVERIFY(startLiveViewButton->isEnabled());
        QVERIFY(!stopLiveViewButton->isEnabled());

        startLiveViewButton->click();
        QTRY_COMPARE_WITH_TIMEOUT(
            liveViewStatus->text(),
            QStringLiteral("Live View ativo."),
            3000);
        QVERIFY(!startLiveViewButton->isEnabled());
        QVERIFY(stopLiveViewButton->isEnabled());
        QVERIFY(!startCaptureButton->isEnabled());

        stopLiveViewButton->click();
        QTRY_COMPARE_WITH_TIMEOUT(
            liveViewStatus->text(),
            QStringLiteral("Live View desligado."),
            3000);
        QVERIFY(startLiveViewButton->isEnabled());
        QVERIFY(!stopLiveViewButton->isEnabled());
        QVERIFY(startCaptureButton->isEnabled());

        startCaptureButton->click();
        QVERIFY(!startLiveViewButton->isEnabled());
        QVERIFY(!stopLiveViewButton->isEnabled());
        auto* captureStatus =
            window.findChild<QLabel*>(QStringLiteral("captureConfigurationStatus"));
        QVERIFY(captureStatus);
        QTRY_VERIFY2_WITH_TIMEOUT(
            captureStatus->text().contains(QStringLiteral("Captura concluída")),
            qPrintable(captureStatus->text()),
            3000);
        QTRY_VERIFY_WITH_TIMEOUT([&window] {
            auto* thread = window.findChild<QThread*>(
                QStringLiteral("captureExecutionThread"));
            return !thread || !thread->isRunning();
        }(), 3000);
        QCOMPARE(liveViewStatus->text(), QStringLiteral("Live View desligado."));
        QVERIFY(startLiveViewButton->isEnabled());
        QVERIFY(!stopLiveViewButton->isEnabled());
        QVERIFY(startCaptureButton->isEnabled());
        window.show();
        startLiveViewButton->click();
        QTRY_COMPARE_WITH_TIMEOUT(
            liveViewStatus->text(),
            QStringLiteral("Live View ativo."),
            3000);
        window.close();
        QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(), 3000);
        QVERIFY(std::holds_alternative<domain::CameraCapture>(
            camera.capture(domain::CaptureRequest{})));
    }

    void startSessionActionOpensCaptureConfigurationPageAndCreatesSession()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        infrastructure::SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("astrocapture.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository repository(database);
        infrastructure::SqliteCaptureRunRepository captureRunRepository(database);
        infrastructure::SqliteFrameRepository frameRepository(database);
        infrastructure::SimulatedCamera camera;
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService service(repository, storageLayout);
        application::CaptureConfigurationService captureConfigurationService(
            repository, captureRunRepository, camera);
        application::CapturePreviewService previewService(
            captureRunRepository,
            frameRepository);
        MainWindow window(
            service,
            captureConfigurationService,
            camera,
            directory.path(),
            nullptr,
            &previewService);
        auto* startAction = window.findChild<QPushButton*>(QStringLiteral("startCaptureAction"));
        QVERIFY(startAction);

        QTimer::singleShot(0, [&window] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("sessionConfigurationDialog"));
            auto* targetInput =
                dialog->findChild<QLineEdit*>(QStringLiteral("sessionDialogTargetInput"));
            auto* sessionInput =
                dialog->findChild<QLineEdit*>(QStringLiteral("sessionDialogNameInput"));
            auto* workflowInput =
                dialog->findChild<QComboBox*>(QStringLiteral("sessionDialogWorkflowInput"));
            auto* startButton =
                dialog->findChild<QPushButton*>(QStringLiteral("createAndStartSessionButton"));
            QVERIFY(targetInput);
            QVERIFY(sessionInput);
            QVERIFY(workflowInput);
            QCOMPARE(workflowInput->count(), 2);
            QCOMPARE(workflowInput->findText(QStringLiteral("Edição")), -1);
            QVERIFY(startButton);
            QVERIFY(!startButton->isEnabled());
            targetInput->setText(QStringLiteral("M31"));
            sessionInput->setText(QStringLiteral("Sessão de teste"));
            workflowInput->setCurrentIndex(0);
            QVERIFY(startButton->isEnabled());
            startButton->click();
        });
        startAction->click();

        auto* pages = window.findChild<QStackedWidget*>(QStringLiteral("mainPages"));
        QVERIFY(pages);
        QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("captureConfigurationPage"));
        QVERIFY(!QApplication::activeModalWidget());
        window.show();
        window.resize(320, 320);
        QCoreApplication::processEvents();
        auto* captureScroll = qobject_cast<QScrollArea*>(pages->currentWidget());
        QVERIFY(captureScroll);
        QCOMPARE(captureScroll->widget()->width(), captureScroll->viewport()->width());
        QCOMPARE(captureScroll->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
        auto* capturePage = pages->currentWidget();
        auto* cameraInput = capturePage->findChild<QComboBox*>(QStringLiteral("captureCameraInput"));
        auto* frameTypeInput = capturePage->findChild<QComboBox*>(QStringLiteral("frameTypeInput"));
        auto* modeInput = capturePage->findChild<QComboBox*>(QStringLiteral("captureModeInput"));
        auto* exposureInput = capturePage->findChild<QDoubleSpinBox*>(QStringLiteral("exposureInput"));
        auto* isoInput = capturePage->findChild<QComboBox*>(QStringLiteral("isoInput"));
        auto* countInput = capturePage->findChild<QSpinBox*>(QStringLiteral("frameCountInput"));
        auto* prepareButton = capturePage->findChild<QPushButton*>(QStringLiteral("startCaptureButton"));
        QVERIFY(cameraInput);
        QCOMPARE(cameraInput->count(), 2);
        auto* cameraModel = qobject_cast<QStandardItemModel*>(cameraInput->model());
        QVERIFY(cameraModel);
        QVERIFY(!(cameraModel->item(1)->flags() & Qt::ItemIsEnabled));
        QVERIFY(frameTypeInput);
        QVERIFY(modeInput);
        QVERIFY(exposureInput);
        QVERIFY(isoInput);
        QVERIFY(countInput);
        QVERIFY(prepareButton);
        QVERIFY(exposureInput->minimum() >= 0.001);
        QVERIFY(exposureInput->maximum() <= 1800.0);
        frameTypeInput->setCurrentIndex(0);
        modeInput->setCurrentIndex(0);
        exposureInput->setValue(2.0);
        isoInput->setCurrentIndex(isoInput->findData(800u));
        countInput->setValue(3);
        prepareButton->click();
        auto* captureStatus =
            capturePage->findChild<QLabel*>(QStringLiteral("captureConfigurationStatus"));
        QVERIFY(captureStatus);
        QTRY_VERIFY2_WITH_TIMEOUT(
            captureStatus->text().contains(QStringLiteral("Captura concluída")),
            qPrintable(captureStatus->text()),
            3000);
        QTRY_VERIFY_WITH_TIMEOUT([&window] {
            auto* thread = window.findChild<QThread*>(
                QStringLiteral("captureExecutionThread"));
            return !thread || !thread->isRunning();
        }(), 3000);
        auto* statisticsLabel =
            window.findChild<QLabel*>(QStringLiteral("captureStatistics"));
        QVERIFY(statisticsLabel);
        const QString statisticsText = statisticsLabel->text();
        QVERIFY(statisticsText.contains(QStringLiteral("Sessão: 3 concluídos")));
        QVERIFY(statisticsText.contains(QStringLiteral("Light: 3")));
        QVERIFY(statisticsText.contains(QStringLiteral("6.00 s de exposição")));
        QVERIFY(statisticsText.contains(QStringLiteral("Noite 25/10/2026: 3 concluídos")));
        auto* sequenceMetrics =
            window.findChild<QLabel*>(QStringLiteral("captureSequenceMetrics"));
        QVERIFY(sequenceMetrics);
        QVERIFY(sequenceMetrics->text().contains(QStringLiteral("Pendentes: 0")));
        QVERIFY(sequenceMetrics->text().contains(QStringLiteral("Falhas: 0")));
        auto* previewStatus =
            window.findChild<QLabel*>(QStringLiteral("capturePreviewStatus"));
        QVERIFY(previewStatus);
        QTRY_COMPARE_WITH_TIMEOUT(
            previewStatus->text(),
            QStringLiteral("Último quadro concluído."),
            3000);

        const auto sessions = repository.findAll();
        QCOMPARE(sessions.size(), std::size_t{1});
        QCOMPARE(sessions.front().session.name(), std::string("Sessão de teste"));
        QCOMPARE(sessions.front().session.stage(), domain::SessionStage::Capture);
        QVERIFY(sessions.front().isOpen);
        QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("captureConfigurationPage"));
        const auto runs = captureRunRepository.findBySessionId(sessions.front().session.id());
        QCOMPARE(runs.size(), std::size_t{1});
        QCOMPARE(runs.front().settings().plannedFrameCount, std::optional<std::uint32_t>{3});
        QCOMPARE(frameRepository.findByCaptureRunId(runs.front().id()).size(), std::size_t{3});
        QVERIFY_EXCEPTION_THROWN(
            captureConfigurationService.configureSequence(
                sessions.front().session.id(),
                domain::CaptureSettings{
                    domain::FrameType::Light,
                    std::chrono::minutes{31},
                    800u,
                    domain::CaptureMode::Finite,
                    1u}),
            std::invalid_argument);
        QCOMPARE(captureRunRepository.findBySessionId(sessions.front().session.id()).size(), std::size_t{1});
        window.close();
    }
};
QTEST_MAIN(ApplicationMetadataTest)
#include "test_application_metadata.moc"
