#include "presentation/widgets/dark_combo_box.h"

#include <QFrame>
#include <QListView>
#include <QPalette>

namespace presentation {

DarkComboBox::DarkComboBox(QWidget* parent)
    : QComboBox(parent)
{
    auto* listView = new QListView(this);
    listView->setFrameShape(QFrame::NoFrame);
    listView->setLineWidth(0);
    listView->setMidLineWidth(0);
    listView->setSpacing(0);
    listView->setStyleSheet(QStringLiteral(
        "QListView { background:#071119; color:#e5edf3; border:0; outline:0; }"
        "QListView::item { background:#071119; padding:5px 8px; }"
        "QListView::item:selected { background:#0c2928; color:#49e3b6; }"));
    QPalette palette = listView->palette();
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#071119")));
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#071119")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#e5edf3")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#0c2928")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#49e3b6")));
    listView->setPalette(palette);
    listView->viewport()->setPalette(palette);
    listView->viewport()->setAutoFillBackground(true);
    setView(listView);
}

void DarkComboBox::showPopup()
{
    QComboBox::showPopup();
    QWidget* popup = view()->parentWidget();
    if (auto* popupFrame = qobject_cast<QFrame*>(popup)) {
        popupFrame->setFrameShape(QFrame::NoFrame);
        popupFrame->setLineWidth(0);
        popupFrame->setMidLineWidth(0);
        popupFrame->setAutoFillBackground(true);
        QPalette palette = popupFrame->palette();
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#071119")));
        popupFrame->setPalette(palette);
        popupFrame->setStyleSheet(QStringLiteral(
            "QFrame { background:#071119; border:0; padding:0; }"));
    }
}

} // namespace presentation
