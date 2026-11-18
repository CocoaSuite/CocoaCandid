#include "AreaPicker.hpp"
#include "Engine.hpp"
#include "Icons.hpp"
#include "Style.hpp"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QWidget>

class AreaOverlay : public QWidget {
public:
	AreaOverlay(AreaPicker *owner, QScreen *screen) : QWidget(nullptr), m_owner(owner), m_screen(screen)
	{
		setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
		setAttribute(Qt::WA_DeleteOnClose);
		setCursor(Qt::CrossCursor);
		setMouseTracking(true);
		m_shot = screen->grabWindow(0);
		setGeometry(screen->geometry());

		// Mode bar, top centre.
		m_bar = new QWidget(this);
		m_bar->setObjectName("panel");
		m_bar->setAttribute(Qt::WA_StyledBackground);
		m_bar->setStyleSheet(QString("QWidget#panel { background: %1; border: 1px solid %2; border-radius: 11px; }")
					     .arg(Theme::current().panel.name(), Theme::current().line.name()));
		auto *h = new QHBoxLayout(m_bar);
		h->setContentsMargins(4, 4, 10, 4);
		h->setSpacing(4);
		auto *rectBtn = new QPushButton(Icons::get("scene"), tr("Rectangle"));
		auto *full = new QPushButton(Icons::get("screen"), tr("Full screen"));
		for (auto *b : {rectBtn, full}) {
			setKind(b, "seg");
			b->setCheckable(true);
			h->addWidget(b);
		}
		rectBtn->setChecked(true);
		auto *hint = new QLabel(tr("Drag to draw. Esc cancels."));
		hint->setProperty("mute", true);
		h->addSpacing(6);
		h->addWidget(hint);
		connect(full, &QPushButton::clicked, this, [this] {
			m_rect = rect();
			confirm();
		});
		addShadow(m_bar, 28, 8);
		m_bar->adjustSize();
		m_bar->move((width() - m_bar->width()) / 2, 22);

		// Confirm bar, shown under the rectangle once drawn.
		m_actions = new QWidget(this);
		m_actions->setObjectName("panel");
		m_actions->setAttribute(Qt::WA_StyledBackground);
		m_actions->setStyleSheet(m_bar->styleSheet());
		auto *a = new QHBoxLayout(m_actions);
		a->setContentsMargins(4, 4, 4, 4);
		a->setSpacing(6);
		auto *redraw = new QPushButton(tr("Redraw"));
		auto *cancel = new QPushButton(tr("Cancel"));
		auto *add = new QPushButton(Icons::tinted("plus", Theme::current().prifg), tr("Add as source"));
		setKind(redraw, "seg");
		setKind(cancel, "seg");
		setKind(add, "pri");
		a->addWidget(redraw);
		a->addWidget(cancel);
		a->addWidget(add);
		addShadow(m_actions, 28, 8);
		m_actions->adjustSize();
		m_actions->hide();
		connect(redraw, &QPushButton::clicked, this, [this] {
			m_rect = QRect();
			m_actions->hide();
			update();
		});
		connect(cancel, &QPushButton::clicked, this, [this] { m_owner->finish(nullptr, {}); });
		connect(add, &QPushButton::clicked, this, [this] { m_owner->finish(m_screen, m_rect); });
	}

	void confirm()
	{
		if (m_rect.width() < 8 || m_rect.height() < 8) {
			m_rect = QRect();
			m_actions->hide();
			update();
			return;
		}
		QPoint p(m_rect.right() - m_actions->width(), m_rect.bottom() + 10);
		if (p.y() + m_actions->height() > height() - 8)
			p.setY(m_rect.bottom() - m_actions->height() - 10);
		p.setX(std::max(8, p.x()));
		m_actions->move(p);
		m_actions->show();
		m_actions->raise();
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.drawPixmap(rect(), m_shot);
		QRegion dim(rect());
		if (!m_rect.isNull())
			dim -= QRegion(m_rect);
		p.setClipRegion(dim);
		p.fillRect(rect(), QColor(0, 0, 0, 140));
		p.setClipping(false);
		if (m_rect.isNull())
			return;
		p.setPen(QPen(Qt::white, 2));
		p.drawRect(m_rect.adjusted(-1, -1, 0, 0));
		p.setPen(QPen(QColor(0, 0, 0, 100), 1));
		p.setBrush(Qt::white);
		for (QPoint c : {m_rect.topLeft(), m_rect.topRight(), m_rect.bottomLeft(), m_rect.bottomRight(),
				 QPoint(m_rect.center().x(), m_rect.top()), QPoint(m_rect.center().x(), m_rect.bottom()),
				 QPoint(m_rect.left(), m_rect.center().y()), QPoint(m_rect.right(), m_rect.center().y())})
			p.drawRoundedRect(QRectF(c.x() - 5, c.y() - 5, 10, 10), 2, 2);
		const qreal r = m_screen->devicePixelRatio();
		const QString size = QString("%1 × %2").arg(int(m_rect.width() * r)).arg(int(m_rect.height() * r));
		QFont f = font();
		f.setPixelSize(12);
		p.setFont(f);
		const QRect tr = p.fontMetrics().boundingRect(size).adjusted(-8, -4, 8, 4);
		QPoint at(m_rect.left(), m_rect.top() - tr.height() - 8);
		if (at.y() < 4)
			at.setY(m_rect.top() + 8);
		const QRect box(at, tr.size());
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(20, 20, 20, 235));
		p.drawRoundedRect(box, 6, 6);
		p.setPen(QColor("#ececec"));
		p.drawText(box, Qt::AlignCenter, size);
	}

	void mousePressEvent(QMouseEvent *e) override
	{
		if (e->button() != Qt::LeftButton)
			return;
		m_handle = hitHandle(e->position().toPoint());
		m_start = e->position().toPoint();
		m_orig = m_rect;
		if (m_handle < 0 && m_rect.contains(m_start) && !m_rect.isNull())
			m_handle = 8; // move
		if (m_handle < 0)
			m_rect = QRect(m_start, QSize(1, 1));
		m_actions->hide();
		m_dragging = true;
	}

	void mouseMoveEvent(QMouseEvent *e) override
	{
		const QPoint p = e->position().toPoint();
		if (!m_dragging) {
			const int h = hitHandle(p);
			static const Qt::CursorShape s[8] = {Qt::SizeFDiagCursor, Qt::SizeBDiagCursor, Qt::SizeBDiagCursor, Qt::SizeFDiagCursor,
							     Qt::SizeVerCursor,   Qt::SizeVerCursor,   Qt::SizeHorCursor,   Qt::SizeHorCursor};
			if (h >= 0)
				setCursor(s[h]);
			else if (!m_rect.isNull() && m_rect.contains(p))
				setCursor(Qt::SizeAllCursor);
			else
				setCursor(Qt::CrossCursor);
			return;
		}
		QRect r = m_orig;
		switch (m_handle) {
		case -1:
			m_rect = QRect(m_start, p).normalized();
			break;
		case 8:
			r.translate(p - m_start);
			r.moveLeft(std::clamp(r.left(), 0, width() - r.width()));
			r.moveTop(std::clamp(r.top(), 0, height() - r.height()));
			m_rect = r;
			break;
		default:
			if (m_handle == 0 || m_handle == 2 || m_handle == 6)
				r.setLeft(p.x());
			if (m_handle == 1 || m_handle == 3 || m_handle == 7)
				r.setRight(p.x());
			if (m_handle == 0 || m_handle == 1 || m_handle == 4)
				r.setTop(p.y());
			if (m_handle == 2 || m_handle == 3 || m_handle == 5)
				r.setBottom(p.y());
			m_rect = r.normalized();
		}
		m_rect = m_rect.intersected(rect());
		update();
	}

	void mouseReleaseEvent(QMouseEvent *) override
	{
		if (!m_dragging)
			return;
		m_dragging = false;
		confirm();
	}

	void keyPressEvent(QKeyEvent *e) override
	{
		if (e->key() == Qt::Key_Escape)
			m_owner->finish(nullptr, {});
		else if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !m_rect.isNull())
			m_owner->finish(m_screen, m_rect);
	}

private:
	int hitHandle(const QPoint &p) const
	{
		if (m_rect.isNull())
			return -1;
		const QPoint pts[8] = {m_rect.topLeft(), m_rect.topRight(), m_rect.bottomLeft(), m_rect.bottomRight(),
				       QPoint(m_rect.center().x(), m_rect.top()), QPoint(m_rect.center().x(), m_rect.bottom()),
				       QPoint(m_rect.left(), m_rect.center().y()), QPoint(m_rect.right(), m_rect.center().y())};
		// 0 TL, 1 TR, 2 BL, 3 BR, 4 T, 5 B, 6 L, 7 R
		for (int i = 0; i < 8; i++)
			if ((pts[i] - p).manhattanLength() <= 10)
				return i;
		return -1;
	}

	AreaPicker *m_owner;
	QScreen *m_screen;
	QPixmap m_shot;
	QRect m_rect, m_orig;
	QPoint m_start;
	int m_handle = -1;
	bool m_dragging = false;
	QWidget *m_bar, *m_actions;
};

AreaPicker::AreaPicker(Done done) : m_done(std::move(done)) {}

void AreaPicker::start(Done done)
{
	auto *picker = new AreaPicker(std::move(done));
	for (QScreen *s : QGuiApplication::screens()) {
		auto *o = new AreaOverlay(picker, s);
		picker->m_overlays.append(o);
		o->show();
		o->raise();
		o->activateWindow();
	}
}

void AreaPicker::finish(QScreen *screen, const QRect &r)
{
	for (auto &o : m_overlays)
		if (o)
			o->close();
	Done done = m_done;
	deleteLater();
	if (done)
		done(screen, r);
}

/** Finds the capture setting that means this screen, from the plugin's own list of displays. */
static void chooseMonitor(obs_source_t *src, QScreen *screen)
{
	obs_properties_t *props = obs_source_properties(src);
	if (!props)
		return;
	OBSDataAutoRelease set = obs_source_get_settings(src);
	const char *keys[] = {"monitor_id", "monitor", "screen"};
	const QRect g = screen->geometry();
	const qreal dpr = screen->devicePixelRatio();
	const int index = QGuiApplication::screens().indexOf(screen);
	for (const char *key : keys) {
		obs_property_t *p = obs_properties_get(props, key);
		if (!p || obs_property_get_type(p) != OBS_PROPERTY_LIST)
			continue;
		const size_t n = obs_property_list_item_count(p);
		int best = -1;
		static const QRegularExpression at(R"(@\s*(-?\d+)\s*,\s*(-?\d+))"), size(R"((\d+)\s*x\s*(\d+))");
		for (size_t i = 0; i < n && best < 0; i++) {
			const QString label = QString::fromUtf8(obs_property_list_item_name(p, i));
			const auto m = at.match(label);
			if (m.hasMatch() && m.captured(1).toInt() == g.x() && m.captured(2).toInt() == g.y())
				best = int(i);
		}
		for (size_t i = 0; i < n && best < 0; i++) {
			const QString label = QString::fromUtf8(obs_property_list_item_name(p, i));
			const auto m = size.match(label);
			if (m.hasMatch() && m.captured(1).toInt() == int(g.width() * dpr) && m.captured(2).toInt() == int(g.height() * dpr))
				best = int(i);
		}
		if (best < 0 && index >= 0 && size_t(index) < n)
			best = index;
		if (best >= 0) {
			if (obs_property_list_format(p) == OBS_COMBO_FORMAT_STRING)
				obs_data_set_string(set, key, obs_property_list_item_string(p, size_t(best)));
			else
				obs_data_set_int(set, key, obs_property_list_item_int(p, size_t(best)));
			obs_source_update(src, set);
		}
		break;
	}
	obs_properties_destroy(props);
}

bool AreaPicker::addArea(QScreen *screen, const QRect &r, QString *error)
{
	const QString type = Engine::firstType({"monitor_capture", "xshm_input", "screen_capture"});
	if (type.isEmpty() || !screen) {
		if (error)
			*error = QObject::tr("Screen capture isn't available.");
		return false;
	}
	const QString name = ENGINE.uniqueName(QObject::tr("Area"));
	OBSSourceAutoRelease src = obs_source_create(type.toUtf8().constData(), name.toUtf8().constData(), nullptr, nullptr);
	if (!src)
		return false;
	chooseMonitor(src, screen);
	OBSDataAutoRelease priv = obs_source_get_private_settings(src);
	obs_data_set_bool(priv, "candid_area", true);

	obs_sceneitem_t *item = obs_scene_add(ENGINE.editSceneObj(), src);
	if (!item)
		return false;
	const qreal dpr = screen->devicePixelRatio();
	const int W = int(screen->geometry().width() * dpr), H = int(screen->geometry().height() * dpr);
	obs_sceneitem_crop c;
	c.left = int(r.left() * dpr);
	c.top = int(r.top() * dpr);
	c.right = std::max(0, W - int((r.left() + r.width()) * dpr));
	c.bottom = std::max(0, H - int((r.top() + r.height()) * dpr));
	obs_sceneitem_set_crop(item, &c);
	vec2 zero;
	vec2_set(&zero, 0, 0);
	obs_sceneitem_set_pos(item, &zero);
	ENGINE.select(item);
	emit ENGINE.itemsChanged();
	ENGINE.save();
	return true;
}
