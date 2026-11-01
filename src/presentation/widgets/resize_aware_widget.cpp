#include "presentation/widgets/resize_aware_widget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QStyleOption>

#include <utility>

namespace presentation {

ResizeAwareWidget::ResizeAwareWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
}

void ResizeAwareWidget::setResizeHandler(ResizeHandler handler)
{
    resizeHandler_ = std::move(handler);
}

void ResizeAwareWidget::paintEvent(QPaintEvent* event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

void ResizeAwareWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (resizeHandler_) {
        resizeHandler_(event->size().width());
    }
}

} // namespace presentation
