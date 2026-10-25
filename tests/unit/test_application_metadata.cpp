#include "app/application_metadata.h"
#include "application/capture_configuration_service.h"
#include "application/session_service.h"
#include "infrastructure/camera/simulated_camera.h"
#include "infrastructure/persistence/sqlite_database.h"
#include "infrastructure/persistence/sqlite_repositories.h"
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
#include <QFrame>
#include <QGridLayout>
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
            [stopRequested](quint32, quint32, const QString& details) {
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
        MainWindow window(service, captureConfigurationService, camera, directory.path());

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
        MainWindow window(service, captureConfigurationService, camera, directory.path());
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
