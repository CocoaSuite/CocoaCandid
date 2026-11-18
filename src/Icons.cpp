#include "Icons.hpp"

#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace {
QColor g_normal(0x9a, 0x9a, 0x9a), g_active(0xec, 0xec, 0xec);

const QHash<QString, QString> &paths()
{
	static const QHash<QString, QString> p = {
		{"plus", R"(<path d="M12 5v14M5 12h14"/>)"},
		{"minus", R"(<path d="M5 12h14"/>)"},
		{"x", R"(<path d="M6 6l12 12M18 6L6 18"/>)"},
		{"check", R"(<path d="M5 12l5 5 9-11"/>)"},
		{"chevron", R"(<path d="M6 9l6 6 6-6"/>)"},
		{"up", R"(<path d="M6 15l6-6 6 6"/>)"},
		{"down", R"(<path d="M6 9l6 6 6-6"/>)"},
		{"more", R"(<circle cx="5" cy="12" r="1"/><circle cx="12" cy="12" r="1"/><circle cx="19" cy="12" r="1"/>)"},
		{"scene", R"(<rect x="3" y="5" width="18" height="14" rx="2"/>)"},
		{"screen", R"(<rect x="3" y="4" width="18" height="12" rx="2"/><path d="M8 20h8M12 16v4"/>)"},
		{"window", R"(<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M3 9h18M7 6.5h.01M10 6.5h.01"/>)"},
		{"area", R"(<path d="M4 8V4h4M16 4h4v4M20 16v4h-4M8 20H4v-4"/><rect x="8" y="8" width="8" height="8" rx="1"/>)"},
		{"game", R"(<rect x="2" y="7" width="20" height="11" rx="5"/><path d="M7 11v3M5.5 12.5h3M15 12h.01M18 12h.01"/>)"},
		{"camera", R"(<rect x="3" y="6" width="13" height="12" rx="2"/><path d="M16 10l5-3v10l-5-3"/>)"},
		{"mic", R"(<rect x="9" y="3" width="6" height="11" rx="3"/><path d="M5 11a7 7 0 0 0 14 0M12 18v3"/>)"},
		{"speaker", R"(<path d="M11 5L6 9H2v6h4l5 4zM15.5 8.5a5 5 0 0 1 0 7M19 5a10 10 0 0 1 0 14"/>)"},
		{"muted", R"(<path d="M11 5L6 9H2v6h4l5 4zM23 9l-6 6M17 9l6 6"/>)"},
		{"appaudio", R"(<path d="M11 5L6 9H2v6h4l5 4zM15.5 8.5a5 5 0 0 1 0 7"/>)"},
		{"image", R"(<rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="9" cy="9" r="2"/><path d="M21 15l-5-5L5 21"/>)"},
		{"media", R"(<rect x="3" y="5" width="18" height="14" rx="2"/><path d="M10 9l5 3-5 3z"/>)"},
		{"text", R"(<path d="M5 6V4h14v2M12 4v16M9 20h6"/>)"},
		{"web", R"(<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3a14 14 0 0 1 0 18M12 3a14 14 0 0 0 0 18"/>)"},
		{"color", R"(<rect x="4" y="4" width="16" height="16" rx="3"/>)"},
		{"copy", R"(<rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2"/>)"},
		{"eye", R"(<path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/>)"},
		{"eyeoff", R"(<path d="M3 3l18 18M10.6 5.1A10 10 0 0 1 12 5c6.5 0 10 7 10 7a17 17 0 0 1-3.2 4.1M6.6 6.6A17 17 0 0 0 2 12s3.5 7 10 7a9.7 9.7 0 0 0 5.4-1.6"/>)"},
		{"lock", R"(<rect x="5" y="11" width="14" height="9" rx="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/>)"},
		{"unlock", R"(<rect x="5" y="11" width="14" height="9" rx="2"/><path d="M8 11V8a4 4 0 0 1 7.5-2"/>)"},
		{"gear", R"(<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/>)"},
		{"sliders", R"(<path d="M4 21v-7M4 10V3M12 21v-9M12 8V3M20 21v-5M20 12V3M1 14h6M9 8h6M17 16h6"/>)"},
		{"record", R"(<circle cx="12" cy="12" r="6" fill="currentColor" stroke="none"/>)"},
		{"stop", R"(<rect x="7" y="7" width="10" height="10" rx="1.5" fill="currentColor" stroke="none"/>)"},
		{"pause", R"(<path d="M9 5v14M15 5v14"/>)"},
		{"play", R"(<path d="M7 5l12 7-12 7z"/>)"},
		{"live", R"(<circle cx="12" cy="12" r="2"/><path d="M16.2 7.8a6 6 0 0 1 0 8.4M7.8 16.2a6 6 0 0 1 0-8.4M19 5a10 10 0 0 1 0 14M5 19A10 10 0 0 1 5 5"/>)"},
		{"snapshot", R"(<path d="M4 8h3l2-3h6l2 3h3v11H4z"/><circle cx="12" cy="13" r="3.5"/>)"},
		{"replay", R"(<path d="M3 12a9 9 0 1 0 3-6.7"/><path d="M3 4v4h4"/><path d="M12 8v4l3 2"/>)"},
		{"vcam", R"(<rect x="3" y="6" width="13" height="12" rx="2"/><path d="M16 10l5-3v10l-5-3"/><path d="M7 12h5"/>)"},
		{"studio", R"(<rect x="2" y="6" width="9" height="12" rx="1.5"/><rect x="13" y="6" width="9" height="12" rx="1.5"/>)"},
		{"arrowright", R"(<path d="M5 12h14M13 6l6 6-6 6"/>)"},
		{"crop", R"(<path d="M6 2v14a2 2 0 0 0 2 2h14M18 22V8a2 2 0 0 0-2-2H2"/>)"},
		{"fit", R"(<path d="M15 3h6v6M9 21H3v-6M21 3l-7 7M3 21l7-7"/>)"},
		{"center", R"(<path d="M12 3v18M3 12h18"/><rect x="7" y="7" width="10" height="10" rx="1"/>)"},
		{"filters", R"(<circle cx="9" cy="9" r="6"/><circle cx="15" cy="15" r="6"/>)"},
		{"trash", R"(<path d="M4 7h16M10 11v6M14 11v6M5 7l1 13h12l1-13M9 7V4h6v3"/>)"},
		{"edit", R"(<path d="M4 20h4L19 9l-4-4L4 16z"/>)"},
		{"folder", R"(<path d="M3 6a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>)"},
		{"help", R"(<circle cx="12" cy="12" r="9"/><path d="M9.5 9a2.5 2.5 0 1 1 3.5 2.3c-.6.3-1 .9-1 1.6v.6M12 17h.01"/>)"},
		{"group", R"(<path d="M3 7h7l2 2h9v10H3z"/>)"},
		{"bolt", R"(<path d="M13 2L4 14h7l-1 8 9-12h-7z"/>)"},
		{"key", R"(<circle cx="8" cy="15" r="4"/><path d="M11 12l9-9M17 6l3 3"/>)"},
	};
	return p;
}

QString wrap(const QString &body, const QColor &c)
{
	QString b = body;
	b.replace("currentColor", c.name());
	return QString(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">%2</svg>)")
		.arg(c.name(), b);
}
} // namespace

namespace Icons {

void setColors(const QColor &normal, const QColor &active)
{
	g_normal = normal;
	g_active = active;
}

QPixmap pixmap(const QString &name, int size, const QColor &color, qreal dpr)
{
	QSvgRenderer r(wrap(paths().value(name), color).toUtf8());
	QPixmap pm(int(size * dpr), int(size * dpr));
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	r.render(&p);
	pm.setDevicePixelRatio(dpr);
	return pm;
}

QIcon get(const QString &name)
{
	QIcon ic;
	for (int s : {16, 20, 24}) {
		ic.addPixmap(pixmap(name, s, g_normal), QIcon::Normal, QIcon::Off);
		ic.addPixmap(pixmap(name, s, g_active), QIcon::Active, QIcon::Off);
		ic.addPixmap(pixmap(name, s, g_active), QIcon::Normal, QIcon::On);
		ic.addPixmap(pixmap(name, s, g_active), QIcon::Selected, QIcon::Off);
		ic.addPixmap(pixmap(name, s, QColor(g_normal.red(), g_normal.green(), g_normal.blue(), 90)), QIcon::Disabled, QIcon::Off);
	}
	return ic;
}

QIcon tinted(const QString &name, const QColor &color)
{
	QIcon ic;
	for (int s : {16, 20, 24})
		ic.addPixmap(pixmap(name, s, color));
	return ic;
}

QString appSvg(const QString &scheme)
{
	const bool mid = scheme == "midnight";
	const QString tile = mid ? "#101521" : "#3A1424", glyph = mid ? "#D7DCE6" : "#E8AFC0", dot = mid ? "#FF6B70" : glyph;
	return QString(R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><rect width="100" height="100" rx="17.5" fill="%1"/><g transform="translate(19 22)"><path d="M44 6C39.5 1.9 34.5 0 28 0C12.5 0 0 12.5 0 28C0 43.5 12.5 56 28 56C34.5 56 39.5 54.1 44 50L36 40C33.8 42 31.3 43 28 43C19.7 43 13 36.3 13 28C13 19.7 19.7 13 28 13C31.3 13 33.8 14 36 16L44 6Z" fill="%2"/><circle cx="28" cy="28" r="8" fill="%3"/><path d="M49 18.5A16 16 0 0 1 49 37.5M55.5 13.5A24 24 0 0 1 55.5 42.5" fill="none" stroke="%2" stroke-width="4.5" stroke-linecap="round"/></g></svg>)svg")
		.arg(tile, glyph, dot);
}

QIcon app(const QString &scheme)
{
	QSvgRenderer r(appSvg(scheme).toUtf8());
	QIcon ic;
	for (int s : {16, 24, 32, 48, 64, 128, 256}) {
		QPixmap pm(s, s);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		r.render(&p);
		ic.addPixmap(pm);
	}
	return ic;
}

} // namespace Icons
