#include "presentation/widgets/starfield_banner.h"

#include <QColor>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QRadialGradient>
#include <QSizePolicy>

namespace presentation {

StarfieldBanner::StarfieldBanner(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("welcomeBanner"));
    setMinimumHeight(245);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void StarfieldBanner::paintEvent(QPaintEvent* event)
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

} // namespace presentation
