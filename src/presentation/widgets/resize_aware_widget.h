#pragma once

#include <QResizeEvent>
#include <QWidget>

#include <functional>

namespace presentation {

class ResizeAwareWidget final : public QWidget
{
    Q_OBJECT

public:
    using ResizeHandler = std::function<void(int)>;

    explicit ResizeAwareWidget(QWidget* parent = nullptr);

    void setResizeHandler(ResizeHandler handler);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    ResizeHandler resizeHandler_;
};

} // namespace presentation
