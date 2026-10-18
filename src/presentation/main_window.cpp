#include "main_window.h"

#include "app/application_metadata.h"

#include <QLabel>
#include <QString>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(applicationTitle());

    auto* welcomeLabel = new QLabel(
        QStringLiteral("AstroCapture\nVersão %1").arg(QStringLiteral(ASTROCAPTURE_VERSION)),
        this);
    welcomeLabel->setAlignment(Qt::AlignCenter);
    setCentralWidget(welcomeLabel);
    resize(640, 400);
}
