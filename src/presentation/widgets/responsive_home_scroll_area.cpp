#include "presentation/widgets/responsive_home_scroll_area.h"

#include <utility>

namespace presentation {

ResponsiveHomeScrollArea::ResponsiveHomeScrollArea(QWidget* parent)
    : QScrollArea(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
}

void ResponsiveHomeScrollArea::setResizeHandler(ResizeHandler handler)
{
    resizeHandler_ = std::move(handler);
}

void ResponsiveHomeScrollArea::resizeEvent(QResizeEvent* event)
{
    QScrollArea::resizeEvent(event);
    if (resizeHandler_) {
        resizeHandler_(viewport()->width());
    }
}

} // namespace presentation
