#pragma once

#include <QResizeEvent>
#include <QScrollArea>

#include <functional>

namespace presentation {

class ResponsiveHomeScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    using ResizeHandler = std::function<void(int)>;

    explicit ResponsiveHomeScrollArea(QWidget* parent = nullptr);

    void setResizeHandler(ResizeHandler handler);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    ResizeHandler resizeHandler_;
};

} // namespace presentation
