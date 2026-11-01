#pragma once

#include <QComboBox>

namespace presentation {

class DarkComboBox final : public QComboBox
{
    Q_OBJECT

public:
    explicit DarkComboBox(QWidget* parent = nullptr);

    void showPopup() override;
};

} // namespace presentation
