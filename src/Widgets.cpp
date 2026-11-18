#include "Widgets.hpp"
#include "Icons.hpp"
#include "Style.hpp"

#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

QPushButton *iconButton(const QString &icon, const QString &tip, QWidget *parent, bool checkable)
{
	auto *b = new QPushButton(parent);
	b->setIcon(Icons::get(icon));
	b->setIconSize(QSize(16, 16));
	b->setToolTip(tip);
	b->setAccessibleName(tip);
	b->setCheckable(checkable);
	b->setCursor(Qt::PointingHandCursor);
	b->setFocusPolicy(Qt::TabFocus);
	setKind(b, "ic");
	return b;
}

QLabel *panelHeader(const QString &text, QWidget *parent)
{
	auto *l = new QLabel(text.toUpper(), parent);
	l->setObjectName("ph");
	return l;
}

QFrame *hline(QWidget *parent)
{
	auto *f = new QFrame(parent);
	f->setObjectName("line");
	f->setFixedHeight(1);
	return f;
}

QFrame *vline(QWidget *parent)
{
	auto *f = new QFrame(parent);
	f->setObjectName("vline");
	f->setFixedWidth(1);
	return f;
}

QString formatTime(qint64 s)
{
	return QString("%1:%2:%3")
		.arg(s / 3600, 2, 10, QChar('0'))
		.arg((s / 60) % 60, 2, 10, QChar('0'))
		.arg(s % 60, 2, 10, QChar('0'));
}

// Meter -----------------------------------------------------------------------------------------------

Meter::Meter(QWidget *parent) : QWidget(parent)
{
	setFixedHeight(8);
	setMinimumWidth(60);
	setAttribute(Qt::WA_OpaquePaintEvent, false);
	startTimer(33);
}

void Meter::setLevels(float mag, float peak)
{
	m_mag.store(mag);
	m_peak.store(peak);
}

void Meter::setMuted(bool m)
{
	m_muted = m;
	update();
}

void Meter::timerEvent(QTimerEvent *)
{
	const float mag = m_mag.load(), peak = m_peak.load();
	// Rise at once, fall smoothly.
	m_shownMag = mag > m_shownMag ? mag : std::max(mag, m_shownMag - 1.2f);
	m_shownPeak = peak > m_shownPeak ? peak : std::max(peak, m_shownPeak - 1.5f);
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	if (m_shownPeak >= m_hold || now - m_holdAt > 1500) {
		m_hold = m_shownPeak;
		m_holdAt = now;
	}
	update();
}

static double toX(float db, int w)
{
	const double f = std::clamp((double(db) + 60.0) / 60.0, 0.0, 1.0);
	return f * w;
}

void Meter::paintEvent(QPaintEvent *)
{
	const Theme &t = Theme::current();
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	const int w = width(), h = height();
	QPainterPath clip;
	clip.addRoundedRect(QRectF(0, 0, w, h), h / 2.0, h / 2.0);
	p.setClipPath(clip);
	p.fillRect(rect(), t.dark ? QColor("#2a2a2a") : QColor("#e2e2df"));

	const double g = toX(-20, w), y = toX(-9, w);
	const QColor green = m_muted ? t.mute : QColor("#4f9a63"), yellow = m_muted ? t.mute : QColor("#c9a24a"),
		     red = m_muted ? t.mute : QColor("#d95c5c");
	const double x = toX(m_shownPeak, w);
	auto band = [&](double a, double b, QColor c) {
		if (x <= a)
			return;
		c.setAlpha(m_muted ? 90 : 255);
		p.fillRect(QRectF(a, 0, std::min(x, b) - a, h), c);
	};
	band(0, g, green);
	band(g, y, yellow);
	band(y, w, red);
	// The average loudness as a darker inner bar.
	const double mx = toX(m_shownMag, w);
	p.fillRect(QRectF(0, h * 0.35, mx, h * 0.3), QColor(0, 0, 0, 60));
	if (m_hold > -59) {
		const double hx = toX(m_hold, w);
		p.fillRect(QRectF(std::max(0.0, hx - 2), 0, 2, h), m_hold > -9 ? red : t.fg);
	}
}

// ElidedLabel ------------------------------------------------------------------------------------------

void ElidedLabel::setFullText(const QString &t)
{
	m_full = t;
	refit();
}

void ElidedLabel::resizeEvent(QResizeEvent *e)
{
	QLabel::resizeEvent(e);
	refit();
}

void ElidedLabel::refit()
{
	const QString e = fontMetrics().elidedText(m_full, Qt::ElideRight, std::max(10, width()));
	QLabel::setText(e);
	setToolTip(e == m_full ? QString() : m_full);
}
