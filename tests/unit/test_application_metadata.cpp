#include "app/application_metadata.h"
#include "application/session_service.h"
#include "infrastructure/persistence/sqlite_database.h"
#include "infrastructure/persistence/sqlite_repositories.h"
#include "infrastructure/storage/storage_layout.h"
#include "presentation/main_window.h"

#include <QtTest>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTemporaryDir>
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
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService service(repository, storageLayout);
        MainWindow window(service);
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

    void createsListsAndOpensSessions()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        infrastructure::SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("test.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository repository(database);
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService service(repository, storageLayout);
        service.createSession(
            QStringLiteral("M42"),
            QStringLiteral("Outubro"),
            domain::SessionStage::Stacking);
        MainWindow window(service);

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
        QVERIFY(repository.findById(records.front().session.id())->isOpen);
        QVERIFY(activeSession->text().contains(QStringLiteral("Sessão aberta: Outubro")));

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

    void startSessionActionOpensConfigurationModalAndCreatesSession()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        infrastructure::SqliteDatabase database(
            QDir(directory.path()).filePath(QStringLiteral("test.sqlite3")));
        database.open();
        infrastructure::SqliteSessionRepository repository(database);
        infrastructure::StorageLayout storageLayout(
            QDir(directory.path()).filePath(QStringLiteral("images")));
        application::SessionService service(repository, storageLayout);
        MainWindow window(service);
        auto* startAction = window.findChild<QPushButton*>(QStringLiteral("startCaptureAction"));
        QVERIFY(startAction);

        QTimer::singleShot(0, [] {
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

        const auto sessions = repository.findAll();
        QCOMPARE(sessions.size(), std::size_t{1});
        QCOMPARE(sessions.front().session.name(), std::string("Sessão de teste"));
        QCOMPARE(sessions.front().session.stage(), domain::SessionStage::Capture);
        QVERIFY(sessions.front().isOpen);
        QCOMPARE(
            window.findChild<QStackedWidget*>(QStringLiteral("mainPages"))->currentWidget()->objectName(),
            QStringLiteral("homePage"));
        QVERIFY(window.findChild<QStackedWidget*>(QStringLiteral("mainPages"))
                    ->currentWidget()
                    ->findChild<QLabel*>(QStringLiteral("currentSessionName"))
                    ->text()
                    .contains(QStringLiteral("Sessão de teste")));
    }
};
QTEST_MAIN(ApplicationMetadataTest)
#include "test_application_metadata.moc"
