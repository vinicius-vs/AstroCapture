#pragma once

#include "domain/capture_entities.h"

#include <QFrame>
#include <QLabel>
#include <QString>
#include <QWidget>

namespace presentation::theme {

extern const char* const DashboardStyle;

QString stageName(domain::SessionStage stage);

QFrame* makeCard(const QString& objectName, QWidget* parent = nullptr);

QLabel* makeTextLabel(
    const QString& text,
    const QString& objectName = {},
    QWidget* parent = nullptr);

} // namespace presentation::theme
