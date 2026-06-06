#pragma once

#include <QString>
#include <QUrl>

struct Fragment
{
    QUrl source;
    QString label;
    QString notes;
    QString delayColor = QStringLiteral("#000000");
    double start = 0.0;
    double end = 10.0;
    double delayBefore = 0.0;
    bool audioEnabled = true;
    double volume = 1.0;
    double speed = 1.0;
};
