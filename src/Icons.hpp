#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

/** Line icons drawn from small SVG snippets (24 × 24, stroke width 2, round caps), tinted to the theme. */
namespace Icons {
QIcon get(const QString &name);
/** In a specific colour, for example red for a muted speaker. */
QIcon tinted(const QString &name, const QColor &color);
QPixmap pixmap(const QString &name, int size, const QColor &color, qreal dpr = 2.0);
/** The app icon: "garnet" or "midnight". */
QIcon app(const QString &scheme);
QString appSvg(const QString &scheme);
void setColors(const QColor &normal, const QColor &active);
}
