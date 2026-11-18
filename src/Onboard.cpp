#include "Onboard.hpp"
#include "Config.hpp"
#include "Style.hpp"

#include <QApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWindow>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

static const QColor kBrand("#E0A3B3");

// The handwritten greeting, one continuous stroke (the same lettering as CocoaShot and CocoaCanvas).
static const char *kHello =
	"M14 132 C 34 112, 64 60, 64 34 C 64 14, 46 14, 41 38 C 36 68, 36 116, 34 148 C 41 114, 56 94, 74 94 C 91 94, 91 114, 89 134 "
	"C 88 149, 96 151, 108 144 C 121 137, 141 124, 141 109 C 141 97, 124 97, 118 111 C 111 129, 121 151, 141 149 C 161 147, 176 129, 186 109 "
	"C 201 79, 211 44, 206 29 C 201 17, 186 24, 184 54 C 182 89, 184 129, 196 147 C 206 157, 221 144, 231 124 C 246 89, 254 49, 248 29 "
	"C 242 17, 228 24, 227 54 C 225 89, 227 129, 239 147 C 249 157, 264 147, 274 131 C 281 111, 296 99, 311 101 C 331 104, 331 144, 308 149 "
	"C 288 153, 281 127, 296 109 C 304 101, 318 99, 334 104";

static QPainterPath helloPath()
{
	QPainterPath p;
	static const QRegularExpression num(R"(-?\d+(\.\d+)?)");
	const QString s = QString::fromLatin1(kHello);
	QList<double> v;
	for (auto it = num.globalMatch(s); it.hasNext();)
		v.append(it.next().captured(0).toDouble());
	if (v.size() < 2)
		return p;
	p.moveTo(v[0], v[1]);
	for (int i = 2; i + 5 < v.size(); i += 6)
		p.cubicTo(v[i], v[i + 1], v[i + 2], v[i + 3], v[i + 4], v[i + 5]);
	return p;
}

static double ease(double t)
{
	t = std::clamp(t, 0.0, 1.0);
	return t < 0.5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
}

static bool reducedMotion()
{
#ifdef _WIN32
	BOOL anim = TRUE;
	SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &anim, 0);
	return !anim;
#else
	return false;
#endif
}

// Little animated pictures for each step ------------------------------------------------------------

class TourArt : public QWidget {
public:
	explicit TourArt(QWidget *parent) : QWidget(parent)
	{
		setFixedHeight(136);
		m_clock.start();
		startTimer(16);
	}
	void setStep(int s)
	{
		m_step = s;
		m_clock.restart();
		update();
	}

protected:
	void timerEvent(QTimerEvent *) override { update(); }
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		QPainterPath clip;
		clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 18, 18);
		p.fillPath(clip, QColor("#141414"));
		p.setPen(QPen(QColor("#2c2c2c"), 1));
		p.drawPath(clip);
		p.setClipPath(clip);
		// Draw in a 240 × 120 box, centred.
		const double s = std::min(width() / 240.0, height() / 120.0);
		p.translate((width() - 240 * s) / 2, (height() - 120 * s) / 2);
		p.scale(s, s);
		const double t = m_clock.elapsed() / 1000.0;
		const QColor line("#555"), fill("#262626"), dim("#999");
		auto box = [&](QRectF r, QColor f, QColor b, double rad = 6) {
			p.setPen(QPen(b, 1.5));
			p.setBrush(f);
			p.drawRoundedRect(r, rad, rad);
		};
		auto bar = [&](QRectF r, QColor c) {
			p.setPen(Qt::NoPen);
			p.setBrush(c);
			p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
		};
		auto key = [&](double x, double y, const QString &label, double delay) {
			const double ph = std::fmod(t + 2.25 - delay, 2.25) / 2.25;
			const double dy = ph < 0.08 ? ph / 0.08 * 3 : ph < 0.16 ? (0.16 - ph) / 0.08 * 3 : 0;
			box(QRectF(x, y + dy, 44, 34), fill, line, 7);
			p.setPen(QColor("#ddd"));
			QFont f = font();
			f.setPixelSize(13);
			f.setWeight(QFont::DemiBold);
			p.setFont(f);
			p.drawText(QRectF(x, y + dy, 44, 34), Qt::AlignCenter, label);
		};
		switch (m_step) {
		case 0: {
			const double ph = std::fmod(t, 2.8) / 2.8;
			const double lift = ph < 0.2 ? 0 : ph < 0.45 ? ease((ph - 0.2) / 0.25) : ph < 0.7 ? 1 : 1 - ease((ph - 0.7) / 0.3);
			box(QRectF(30, 20, 110, 22), fill, line);
			bar(QRectF(40, 29, 50, 5), dim);
			box(QRectF(30, 76, 110, 22), fill, line);
			bar(QRectF(40, 85, 44, 5), dim);
			box(QRectF(30, 48 - 28 * lift, 110, 22), QColor("#383838"), kBrand);
			bar(QRectF(40, 57 - 28 * lift, 64, 5), QColor("#ddd"));
			key(160, 30, "F2", 0);
			key(160, 70, "F3", 0.6);
			break;
		}
		case 1: {
			const double ph = std::fmod(t, 3.0) / 3.0;
			const double k = ph < 0.25 ? 0 : ph < 0.55 ? ease((ph - 0.25) / 0.3) : ph < 0.85 ? 1 : 1 - ease((ph - 0.85) / 0.15);
			const double w = 150 - 32 * k;
			box(QRectF(45, 18, w, 84), Qt::black, line, 4);
			p.setPen(QPen(kBrand, 2));
			p.setBrush(QColor(29, 63, 156, 140));
			p.drawRoundedRect(QRectF(45, 18, 118, 84), 4, 4);
			break;
		}
		case 2: {
			box(QRectF(60, 8, 120, 104), QColor("#1c1c1c"), QColor("#444"), 10);
			for (int i = 0; i < 4; i++) {
				bar(QRectF(74, 26 + i * 22, 92, 4), QColor("#444"));
				const double ph = std::fmod(t + 2.4 - i * 0.35, 2.4) / 2.4;
				const double x = 96 + i * 7 + 34 * (0.5 - 0.5 * std::cos(ph * 2 * M_PI));
				p.setPen(QPen(kBrand, 2));
				p.setBrush(Qt::white);
				p.drawEllipse(QPointF(x, 28 + i * 22), 6, 6);
			}
			break;
		}
		case 3: {
			const QColor c[3] = {QColor("#4f9a63"), QColor("#4f9a63"), QColor("#c9a24a")};
			for (int i = 0; i < 3; i++) {
				const double ph = std::fmod(t + 1.4 - i * 0.3, 1.4) / 1.4;
				const double h = 80 * (0.35 + 0.65 * (0.5 - 0.5 * std::cos(ph * 2 * M_PI)));
				bar(QRectF(72 + i * 24, 100 - h, 16, h), c[i]);
			}
			p.setPen(QPen(QColor("#ff8a73"), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			p.setBrush(Qt::NoBrush);
			QPainterPath sp;
			sp.moveTo(170, 50);
			sp.lineTo(178, 44);
			sp.lineTo(178, 76);
			sp.lineTo(170, 70);
			sp.lineTo(164, 70);
			sp.lineTo(164, 50);
			sp.closeSubpath();
			p.drawPath(sp);
			p.drawLine(QPointF(186, 54), QPointF(196, 66));
			p.drawLine(QPointF(196, 54), QPointF(186, 66));
			break;
		}
		case 4: {
			const double pulse = 0.65 + 0.35 * std::cos(std::fmod(t, 1.2) / 1.2 * 2 * M_PI);
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(255, 90, 95, int(255 * pulse)));
			p.drawEllipse(QPointF(92, 60), 20, 20);
			for (int i = 0; i < 2; i++) {
				const double ph = std::fmod(t + 1.8 - i * 0.4, 1.8) / 1.8;
				QColor c = kBrand;
				c.setAlphaF(ph < 0.4 ? ph / 0.4 : 1 - (ph - 0.4) / 0.6);
				p.setPen(QPen(c, 4, Qt::SolidLine, Qt::RoundCap));
				p.setBrush(Qt::NoBrush);
				const double r = 30 + i * 16;
				p.drawArc(QRectF(98 - r, 60 - r, 2 * r, 2 * r), -45 * 16, 90 * 16);
			}
			break;
		}
		default: {
			for (int i = 0; i < 3; i++) {
				const double ph = std::fmod(t + 2.4 - i * 0.8, 2.4) / 2.4;
				p.setOpacity(ph < 0.2 ? 0.55 + 0.45 * ph / 0.2 : ph < 0.4 ? 1 : 1 - 0.45 * std::min(1.0, (ph - 0.4) / 0.2));
				const double x = 18 + i * 74;
				box(QRectF(x, 12, 68, 96), QColor("#1c1c1c"), line, 8);
				p.setPen(Qt::NoPen);
				if (i == 0) {
					p.setBrush(QColor("#444"));
					p.drawRoundedRect(QRectF(x + 6, 20, 14, 80), 3, 3);
					p.drawRoundedRect(QRectF(x + 52, 20, 10, 80), 3, 3);
					p.setBrush(QColor("#3a3a3a"));
					p.drawRoundedRect(QRectF(x + 24, 20, 24, 56), 3, 3);
				} else if (i == 1) {
					p.setBrush(QColor("#3a3a3a"));
					p.drawRoundedRect(QRectF(x + 6, 20, 56, 62), 3, 3);
					p.setBrush(QColor("#444"));
					p.drawRoundedRect(QRectF(x + 6, 86, 56, 14), 3, 3);
				} else {
					p.setBrush(QColor("#2d3a4d"));
					p.drawRoundedRect(QRectF(x + 6, 20, 26, 40), 3, 3);
					p.setBrush(QColor("#3a3a3a"));
					p.drawRoundedRect(QRectF(x + 36, 20, 26, 40), 3, 3);
					p.setBrush(QColor("#444"));
					p.drawRoundedRect(QRectF(x + 6, 66, 56, 34), 3, 3);
				}
			}
			p.setOpacity(1);
		}
		}
	}

private:
	int m_step = 0;
	QElapsedTimer m_clock;
};

// Onboard ---------------------------------------------------------------------------------------------

void Onboard::run(QWidget *main, const QVector<Step> &steps, std::function<void(const QString &)> done, bool hello)
{
	auto *o = new Onboard(main, steps, std::move(done), hello);
	o->QWidget::show();
	o->raise();
	o->activateWindow();
	o->setFocus();
}

Onboard::Onboard(QWidget *main, const QVector<Step> &steps, std::function<void(const QString &)> done, bool hello)
	: QWidget(nullptr), m_main(main), m_steps(steps), m_done(std::move(done))
{
	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_DeleteOnClose);
	setFocusPolicy(Qt::StrongFocus);
	m_layout = CFG.str("general", "layout");

	// A blurred copy of the window as it is on screen, preview included.
	const QRect g = main->frameGeometry();
	setGeometry(main->geometry());
	if (QScreen *s = main->screen()) {
		const QPixmap shot = s->grabWindow(0, main->geometry().x() - s->geometry().x(), main->geometry().y() - s->geometry().y(),
						   main->width(), main->height());
		QImage small = shot.toImage().scaled(std::max(1, shot.width() / 14), std::max(1, shot.height() / 14), Qt::IgnoreAspectRatio,
						    Qt::SmoothTransformation);
		small = small.scaled(small.width() * 2, small.height() * 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		m_blur = small.scaled(main->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	(void)g;
	main->installEventFilter(this);

	// The tour card.
	m_card = new QFrame(this);
	m_card->setObjectName("card");
	m_card->setStyleSheet("QFrame#card { background: rgba(30,30,30,245); border: 1px solid #3a3a3a; border-radius: 26px; }"
			      "QFrame#card QLabel { background: transparent; }"
			      "QPushButton#nav { background: transparent; border: none; color: #a8a8a8; padding: 10px 16px; border-radius: 12px; font-size: 14px; }"
			      "QPushButton#nav:hover { color: #f3f3f3; }"
			      "QPushButton#next { background: #f2f2f2; border: none; color: #111; padding: 10px 24px; border-radius: 12px; font-size: 14px; font-weight: 600; }"
			      "QPushButton#lay { background: #262626; border: 1px solid #3a3a3a; color: #ddd; border-radius: 10px; padding: 8px 6px; font-size: 12.5px; }"
			      "QPushButton#lay:checked { border-color: #E0A3B3; background: #333333; color: #fff; }");
	m_card->setFixedWidth(400);
	addShadow(m_card, 60, 20);
	auto *v = new QVBoxLayout(m_card);
	v->setContentsMargins(18, 18, 18, 18);
	v->setSpacing(0);
	m_art = new TourArt(m_card);
	v->addWidget(m_art);
	v->addSpacing(16);
	m_title = new QLabel;
	m_title->setStyleSheet("font-size: 20px; font-weight: 600; color: #f3f3f3;");
	v->addWidget(m_title);
	v->addSpacing(6);
	m_text = new QLabel;
	m_text->setWordWrap(true);
	m_text->setStyleSheet("font-size: 14px; color: #aaaaaa;");
	m_text->setMinimumHeight(44);
	v->addWidget(m_text);
	m_layouts = new QWidget;
	auto *lh = new QHBoxLayout(m_layouts);
	lh->setContentsMargins(0, 12, 0, 0);
	lh->setSpacing(6);
	for (auto [key, label] : {std::pair{"classic", tr("Classic")}, {"compact", tr("Compact")}, {"studio", tr("Studio")}}) {
		auto *b = new QPushButton(label);
		b->setObjectName("lay");
		b->setCheckable(true);
		b->setAutoExclusive(true);
		b->setChecked(m_layout == key);
		const QString k = key;
		connect(b, &QPushButton::clicked, this, [this, k] { m_layout = k; });
		lh->addWidget(b, 1);
	}
	v->addWidget(m_layouts);
	m_dots = new QWidget;
	auto *dh = new QHBoxLayout(m_dots);
	dh->setContentsMargins(0, 16, 0, 14);
	dh->setSpacing(6);
	for (int i = 0; i < m_steps.size(); i++) {
		auto *d = new QLabel;
		d->setFixedHeight(7);
		dh->addWidget(d);
	}
	dh->addStretch(1);
	v->addWidget(m_dots);
	auto *row = new QHBoxLayout;
	auto *skip = new QPushButton(tr("Skip"));
	skip->setObjectName("nav");
	m_back = new QPushButton(tr("Back"));
	m_back->setObjectName("nav");
	m_next = new QPushButton(tr("Next"));
	m_next->setObjectName("next");
	for (auto *b : {skip, m_back, m_next})
		b->setCursor(Qt::PointingHandCursor);
	row->addWidget(skip);
	row->addStretch(1);
	row->addWidget(m_back);
	row->addWidget(m_next);
	v->addLayout(row);
	connect(skip, &QPushButton::clicked, this, &Onboard::finish);
	connect(m_back, &QPushButton::clicked, this, [this] {
		if (m_index > 0)
			show(m_index - 1);
	});
	connect(m_next, &QPushButton::clicked, this, [this] {
		if (m_index < m_steps.size() - 1)
			show(m_index + 1);
		else
			finish();
	});
	m_card->hide();

	m_t0 = QDateTime::currentMSecsSinceEpoch();
	startTimer(16);
	if (!hello) {
		m_fade = 1;
		m_phase = 0;
		m_write = 1;
		QMetaObject::invokeMethod(this, [this] { toCard(); }, Qt::QueuedConnection);
	}
}

bool Onboard::eventFilter(QObject *o, QEvent *e)
{
	if (o == m_main && (e->type() == QEvent::Move || e->type() == QEvent::Resize)) {
		setGeometry(m_main->geometry());
		if (m_phase == 1)
			place();
	}
	return false;
}

void Onboard::playSound()
{
#ifdef _WIN32
	// The sound ships inside the app; MCI plays MP3 without any extra library.
	const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("cocoacandid-hello.mp3");
	if (!QFile::exists(path))
		QFile::copy(":/sounds/hello.mp3", path);
	const std::wstring open = L"open \"" + QDir::toNativeSeparators(path).toStdWString() + L"\" type mpegvideo alias candidhello";
	mciSendStringW(L"close candidhello", nullptr, 0, nullptr);
	if (mciSendStringW(open.c_str(), nullptr, 0, nullptr) == 0) {
		mciSendStringW(L"setaudio candidhello volume to 850", nullptr, 0, nullptr);
		mciSendStringW(L"play candidhello", nullptr, 0, nullptr);
	}
#endif
}

void Onboard::begin(bool)
{
	if (m_phase != -1)
		return;
	m_phase = 0;
	m_t0 = QDateTime::currentMSecsSinceEpoch();
	playSound();
}

void Onboard::timerEvent(QTimerEvent *)
{
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	const double el = double(now - m_t0);
	const bool rm = reducedMotion();
	if (m_phase == -1) {
		m_fade = rm ? 1 : std::min(1.0, el / 700.0);
		if (el > 450)
			begin(false);
	} else if (m_phase == 0) {
		m_fade = 1;
		m_write = rm ? 1 : ease((el - 120) / 2900.0);
		m_tap = el > 3700 ? std::min(1.0, (el - 3700) / 700.0) : 0;
		if (el > 6300)
			toCard();
	} else if (m_phase == 1) {
		m_helloOut = std::min(1.0, m_helloOut + 0.05);
		// The ring glides to its target.
		auto lerp = [](int a, int b) { return a + int(std::round((b - a) * 0.22)); };
		if (m_ring.isNull())
			m_ring = m_ringTarget;
		else
			m_ring = QRect(QPoint(lerp(m_ring.left(), m_ringTarget.left()), lerp(m_ring.top(), m_ringTarget.top())),
				       QPoint(lerp(m_ring.right(), m_ringTarget.right()), lerp(m_ring.bottom(), m_ringTarget.bottom())));
	} else if (m_phase == 2) {
		m_fade = std::max(0.0, m_fade - 0.06);
		if (m_fade <= 0) {
			close();
			return;
		}
	}
	update();
}

QRect Onboard::targetRect() const
{
	const Step &st = m_steps[m_index];
	if (!st.targets || !m_main)
		return {};
	QRect r;
	for (QWidget *w : st.targets()) {
		if (!w || !w->isVisible() || w->width() < 4)
			continue;
		const QRect g(w->mapTo(m_main, QPoint(0, 0)), w->size());
		r = r.isNull() ? g : r.united(g);
	}
	if (r.isNull())
		return {};
	return r.adjusted(-6, -6, 6, 6).intersected(rect().adjusted(3, 3, -3, -3));
}

void Onboard::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setCompositionMode(QPainter::CompositionMode_Source);
	p.fillRect(rect(), Qt::transparent);
	p.setCompositionMode(QPainter::CompositionMode_SourceOver);
	p.setRenderHint(QPainter::Antialiasing);
	const bool ringed = m_phase == 1 && !m_ringTarget.isNull();
	const double blurAmt = m_phase == 1 ? (ringed ? 0.0 : 1.0) : m_fade;

	if (blurAmt > 0 && !m_blur.isNull()) {
		p.setOpacity(blurAmt);
		p.drawImage(rect(), m_blur);
		p.fillRect(rect(), QColor(8, 8, 8, 107));
		p.setOpacity(1);
	}
	if (ringed) {
		QPainterPath all, hole;
		all.addRect(rect());
		hole.addRoundedRect(m_ring, 12, 12);
		p.fillPath(all.subtracted(hole), QColor(8, 8, 8, 148));
		p.setPen(QPen(QColor(224, 163, 179, 70), 8));
		p.drawRoundedRect(m_ring, 12, 12);
		p.setPen(QPen(kBrand, 2));
		p.drawRoundedRect(m_ring, 12, 12);
	}
	if (m_phase == 0 || (m_phase == 1 && m_helloOut < 1)) {
		const double out = m_phase == 1 ? m_helloOut : 0;
		const QPainterPath path = helloPath();
		const double w = std::min(width() * 0.72, 440.0), s = w / 360.0;
		p.save();
		p.setOpacity(1 - out);
		p.translate((width() - w) / 2, height() / 2 - 120 * s - 18 * out);
		p.scale(s, s);
		QPainterPath part;
		const int steps = 700;
		const int upto = int(steps * m_write);
		for (int i = 0; i <= upto; i++) {
			const QPointF pt = path.pointAtPercent(path.percentAtLength(path.length() * i / steps));
			i == 0 ? part.moveTo(pt) : part.lineTo(pt);
		}
		p.setPen(QPen(QColor(0, 0, 0, 70), 9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		p.translate(0, 2);
		p.drawPath(part);
		p.translate(0, -2);
		p.setPen(QPen(Qt::white, 6.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		p.drawPath(part);
		p.restore();
		if (m_tap > 0 && m_phase == 0) {
			p.setOpacity(m_tap);
			QFont f = font();
			f.setPixelSize(15);
			p.setFont(f);
			p.setPen(QColor(255, 255, 255, 184));
			p.drawText(QRectF(0, height() / 2 + 60 * s + 34 + 6 * (1 - m_tap), width(), 24), Qt::AlignHCenter, tr("Click or press any key"));
			p.setOpacity(1);
		}
	}
	if (m_phase == 2)
		setWindowOpacity(m_fade);
}

void Onboard::mousePressEvent(QMouseEvent *)
{
	if (m_phase == -1)
		begin(true);
	else if (m_phase == 0)
		toCard();
}

void Onboard::keyPressEvent(QKeyEvent *e)
{
	if (m_phase == -1)
		return begin(true);
	if (m_phase == 0)
		return toCard();
	if (m_phase != 1)
		return;
	if (e->key() == Qt::Key_Escape)
		finish();
	else if (e->key() == Qt::Key_Right || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space)
		m_index < m_steps.size() - 1 ? show(m_index + 1) : finish();
	else if (e->key() == Qt::Key_Left && m_index > 0)
		show(m_index - 1);
}

void Onboard::toCard()
{
	if (m_phase == 1 || m_phase == 2)
		return;
	m_phase = 1;
	m_helloOut = 0;
	m_card->show();
	show(0);
	m_next->setFocus();
}

void Onboard::show(int i)
{
	m_index = i;
	const Step &st = m_steps[i];
	m_art->setStep(i == m_steps.size() - 1 ? 5 : std::min(i, 4));
	m_title->setText(st.title);
	m_text->setText(st.text);
	m_layouts->setVisible(i == m_steps.size() - 1);
	const auto dots = m_dots->findChildren<QLabel *>();
	for (int k = 0; k < dots.size(); k++) {
		dots[k]->setFixedWidth(k == i ? 22 : 7);
		dots[k]->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(k == i ? kBrand.name() : "#444444"));
	}
	m_back->setVisible(i > 0);
	m_next->setText(i == m_steps.size() - 1 ? tr("Start") : tr("Next"));
	place();
}

void Onboard::place()
{
	m_ringTarget = targetRect();
	m_card->adjustSize();
	const int cw = m_card->width(), ch = m_card->height(), gap = 18, M = 12;
	const QRect b = m_ringTarget;
	int x, y;
	if (b.isNull()) {
		x = (width() - cw) / 2;
		y = (height() - ch) / 2;
	} else if (b.right() + gap + cw < width() - M) {
		x = b.right() + gap;
		y = b.center().y() - ch / 2;
	} else if (b.left() - gap - cw > M) {
		x = b.left() - gap - cw;
		y = b.center().y() - ch / 2;
	} else {
		x = b.center().x() - cw / 2;
		if (b.bottom() + gap + ch < height() - M)
			y = b.bottom() + gap;
		else if (b.top() - gap - ch > M)
			y = b.top() - gap - ch;
		else
			y = b.bottom() - ch - 28;
	}
	m_card->move(std::clamp(x, M, std::max(M, width() - cw - M)), std::clamp(y, M, std::max(M, height() - ch - M)));
}

void Onboard::finish()
{
	if (m_phase == 2)
		return;
	m_phase = 2;
	m_card->hide();
	m_ringTarget = QRect();
	CFG.setFlag("general", "onboarded", true);
	CFG.save();
	if (m_main)
		m_main->removeEventFilter(this);
	if (m_done)
		m_done(m_layout);
}
