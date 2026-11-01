#pragma once

#include <QFrame>

namespace presentation {

class StarfieldBanner final : public QFrame
{
    Q_OBJECT

public:
    explicit StarfieldBanner(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace presentation
