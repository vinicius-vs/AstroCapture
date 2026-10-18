#pragma once

#include <QString>

inline QString applicationTitle()
{
    return QStringLiteral("AstroCapture %1").arg(QStringLiteral(ASTROCAPTURE_VERSION));
}
