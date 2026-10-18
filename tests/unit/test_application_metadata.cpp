#include "app/application_metadata.h"
#include "presentation/main_window.h"

#include <QtTest>

#include <QLabel>

class ApplicationMetadataTest final : public QObject
{
    Q_OBJECT

private slots:
    void windowOpensAndShowsApplicationNameAndVersion()
    {
        MainWindow window;
        QCOMPARE(window.windowTitle(), applicationTitle());

        const auto* welcomeLabel = qobject_cast<QLabel*>(window.centralWidget());
        QVERIFY(welcomeLabel);
        QCOMPARE(welcomeLabel->text(), QStringLiteral("AstroCapture\nVersão 0.1.0"));

        window.show();
        QVERIFY(window.isVisible());
        window.close();
        QVERIFY(window.isHidden());
    }
};

QTEST_MAIN(ApplicationMetadataTest)

#include "test_application_metadata.moc"
