#include "Preview.hpp"
#include "Config.hpp"
#include "Engine.hpp"
#include "Style.hpp"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWindow>
#include <cmath>
#include <graphics/matrix4.h>
#include <graphics/vec4.h>

#if defined(__linux__) || defined(__FreeBSD__)
#include <obs-nix-platform.h>
#endif

static constexpr float kMargin = 18.0f; // logical px around the canvas

Preview::Preview(Mode mode, QWidget *parent) : QWidget(parent), m_mode(mode)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);
	setMouseTracking(true);
	setFocusPolicy(mode == Edit ? Qt::ClickFocus : Qt::NoFocus);
	setMinimumSize(160, 90);
}

Preview::~Preview()
{
	if (m_display)
		obs_display_remove_draw_callback(m_display, draw, this);
	m_display = nullptr;
}

void Preview::setScene(obs_source_t *scene)
{
	QMutexLocker l(&m_lock);
	m_scene = scene;
}

bool Preview::event(QEvent *e)
{
	// The native window may be recreated when the widget moves between screens or is re-parented.
	if (e->type() == QEvent::WinIdChange || e->type() == QEvent::PlatformSurface) {
		if (m_display) {
			obs_display_remove_draw_callback(m_display, draw, this);
			m_display = nullptr;
		}
		if (isVisible())
			QMetaObject::invokeMethod(this, [this] { createDisplay(); }, Qt::QueuedConnection);
	}
	return QWidget::event(e);
}

void Preview::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	createDisplay();
}

void Preview::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	m_dpr = float(devicePixelRatioF());
	if (m_display) {
		const qreal r = devicePixelRatioF();
		obs_display_resize(m_display, uint32_t(width() * r), uint32_t(height() * r));
	}
}

void Preview::createDisplay()
{
	if (m_display || !ENGINE.running() || !windowHandle() || width() < 2 || height() < 2)
		return;
	const qreal r = devicePixelRatioF();
	m_dpr = float(r);
	gs_init_data info = {};
	info.cx = uint32_t(width() * r);
	info.cy = uint32_t(height() * r);
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
#ifdef _WIN32
	info.window.hwnd = reinterpret_cast<HWND>(winId());
#elif defined(__APPLE__)
	info.window.view = reinterpret_cast<id>(winId());
#else
	info.window.id = uint32_t(winId());
	info.window.display = obs_get_nix_platform_display();
#endif
	const QColor bg = Theme::current().desk;
	const uint32_t bgr = uint32_t(bg.blue()) << 16 | uint32_t(bg.green()) << 8 | uint32_t(bg.red());
	m_display = obs_display_create(&info, bgr);
	if (m_display)
		obs_display_add_draw_callback(m_display, draw, this);
}

// Drawing (graphics thread) ----------------------------------------------------------------------------

namespace {
gs_effect_t *solid()
{
	return obs_get_base_effect(OBS_EFFECT_SOLID);
}

void setColor(uint32_t argb)
{
	vec4 c;
	vec4_from_rgba(&c, (argb & 0xff00ff00) | ((argb >> 16) & 0xff) | ((argb & 0xff) << 16));
	gs_effect_set_vec4(gs_effect_get_param_by_name(solid(), "color"), &c);
}

void quad(float x1, float y1, float x2, float y2)
{
	gs_render_start(true);
	gs_vertex2f(x1, y1);
	gs_vertex2f(x2, y1);
	gs_vertex2f(x1, y2);
	gs_vertex2f(x2, y2);
	gs_render_stop(GS_TRISTRIP);
}

void line(float x1, float y1, float x2, float y2, float t)
{
	const float dx = x2 - x1, dy = y2 - y1, len = std::sqrt(dx * dx + dy * dy);
	if (len < 0.01f)
		return;
	const float nx = -dy / len * t * 0.5f, ny = dx / len * t * 0.5f;
	gs_render_start(true);
	gs_vertex2f(x1 + nx, y1 + ny);
	gs_vertex2f(x2 + nx, y2 + ny);
	gs_vertex2f(x1 - nx, y1 - ny);
	gs_vertex2f(x2 - nx, y2 - ny);
	gs_render_stop(GS_TRISTRIP);
}

struct DrawCtx {
	float vx, vy, s, dpr;
	int64_t hover;
};

void boxPoints(obs_sceneitem_t *item, const DrawCtx &c, vec2 out[4])
{
	matrix4 m;
	obs_sceneitem_get_box_transform(item, &m);
	const float pts[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
	for (int i = 0; i < 4; i++) {
		vec3 v;
		vec3_set(&v, pts[i][0], pts[i][1], 0);
		vec3_transform(&v, &v, &m);
		out[i].x = c.vx + v.x * c.s;
		out[i].y = c.vy + v.y * c.s;
	}
}

bool drawItemBox(obs_scene_t *, obs_sceneitem_t *item, void *p)
{
	const DrawCtx &c = *static_cast<DrawCtx *>(p);
	if (!obs_sceneitem_visible(item))
		return true;
	const bool sel = obs_sceneitem_selected(item);
	const bool hov = obs_sceneitem_get_id(item) == c.hover;
	if (!sel && !hov)
		return true;
	vec2 q[4];
	boxPoints(item, c, q);
	const float t = (sel ? 1.6f : 1.0f) * c.dpr;
	setColor(sel ? 0xffE0A3B3 : 0x99E0A3B3);
	while (gs_effect_loop(solid(), "Solid"))
		for (int i = 0; i < 4; i++)
			line(q[i].x, q[i].y, q[(i + 1) % 4].x, q[(i + 1) % 4].y, t);
	if (sel && !obs_sceneitem_locked(item)) {
		const float h = 4.5f * c.dpr, b = 1.2f * c.dpr;
		vec2 hs[8];
		for (int i = 0; i < 4; i++) {
			hs[i * 2] = q[i];
			hs[i * 2 + 1].x = (q[i].x + q[(i + 1) % 4].x) / 2;
			hs[i * 2 + 1].y = (q[i].y + q[(i + 1) % 4].y) / 2;
		}
		setColor(0xffE0A3B3);
		while (gs_effect_loop(solid(), "Solid"))
			for (auto &v : hs)
				quad(v.x - h - b, v.y - h - b, v.x + h + b, v.y + h + b);
		setColor(0xffffffff);
		while (gs_effect_loop(solid(), "Solid"))
			for (auto &v : hs)
				quad(v.x - h, v.y - h, v.x + h, v.y + h);
	}
	return true;
}
} // namespace

void Preview::draw(void *data, uint32_t cx, uint32_t cy)
{
	auto *self = static_cast<Preview *>(data);
	obs_video_info ovi;
	if (!obs_get_video_info(&ovi))
		return;
	const float bw = float(ovi.base_width), bh = float(ovi.base_height);
	const float dpr = self->m_dpr.load();
	const float m = kMargin * dpr;
	const float s = std::max(0.01f, std::min((float(cx) - 2 * m) / bw, (float(cy) - 2 * m) / bh));
	const float vw = std::floor(bw * s), vh = std::floor(bh * s);
	const float vx = std::floor((float(cx) - vw) / 2), vy = std::floor((float(cy) - vh) / 2);
	self->m_viewX = vx;
	self->m_viewY = vy;
	self->m_scale = s;

	OBSSource scene;
	{
		QMutexLocker l(&self->m_lock);
		scene = self->m_scene;
	}

	gs_viewport_push();
	gs_projection_push();
	gs_set_viewport(int(vx), int(vy), int(vw), int(vh));
	gs_ortho(0.0f, bw, 0.0f, bh, -100.0f, 100.0f);
	setColor(0xff000000);
	while (gs_effect_loop(solid(), "Solid"))
		quad(0, 0, bw, bh);
	if (self->m_mode == Program)
		obs_render_main_texture();
	else if (scene)
		obs_source_video_render(scene);
	gs_projection_pop();
	gs_viewport_pop();

	// Outlines in window pixels, so they stay crisp at any zoom.
	gs_viewport_push();
	gs_projection_push();
	gs_set_viewport(0, 0, int(cx), int(cy));
	gs_ortho(0.0f, float(cx), 0.0f, float(cy), -100.0f, 100.0f);
	setColor(0xff3a3a3a);
	while (gs_effect_loop(solid(), "Solid")) {
		const float t = dpr;
		line(vx - t, vy - t / 2, vx + vw + t, vy - t / 2, t);
		line(vx - t, vy + vh + t / 2, vx + vw + t, vy + vh + t / 2, t);
		line(vx - t / 2, vy, vx - t / 2, vy + vh, t);
		line(vx + vw + t / 2, vy, vx + vw + t / 2, vy + vh, t);
	}
	if (self->m_mode == Edit && scene) {
		DrawCtx c{vx, vy, s, dpr, self->m_hoverId.load()};
		obs_scene_enum_items(obs_scene_from_source(scene), drawItemBox, &c);
	}
	gs_projection_pop();
	gs_viewport_pop();
}

// Mouse ------------------------------------------------------------------------------------------------

QPointF Preview::toCanvas(const QPointF &p) const
{
	const qreal r = devicePixelRatioF();
	return QPointF((p.x() * r - m_viewX) / m_scale, (p.y() * r - m_viewY) / m_scale);
}

double Preview::canvasToWidget() const
{
	return m_scale / devicePixelRatioF();
}

static QRectF itemBox(obs_sceneitem_t *item, QPointF corners[4] = nullptr)
{
	matrix4 m;
	obs_sceneitem_get_box_transform(item, &m);
	const float pts[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
	double l = 1e9, t = 1e9, r = -1e9, b = -1e9;
	for (int i = 0; i < 4; i++) {
		vec3 v;
		vec3_set(&v, pts[i][0], pts[i][1], 0);
		vec3_transform(&v, &v, &m);
		if (corners)
			corners[i] = QPointF(v.x, v.y);
		l = std::min(l, double(v.x));
		r = std::max(r, double(v.x));
		t = std::min(t, double(v.y));
		b = std::max(b, double(v.y));
	}
	return QRectF(QPointF(l, t), QPointF(r, b));
}

static bool inside(obs_sceneitem_t *item, const QPointF &p)
{
	matrix4 m, inv;
	obs_sceneitem_get_box_transform(item, &m);
	matrix4_inv(&inv, &m);
	vec3 v;
	vec3_set(&v, float(p.x()), float(p.y()), 0);
	vec3_transform(&v, &v, &inv);
	return v.x >= 0 && v.x <= 1 && v.y >= 0 && v.y <= 1;
}

obs_sceneitem_t *Preview::itemAt(const QPointF &cp) const
{
	OBSSource scene;
	{
		QMutexLocker l(&m_lock);
		scene = m_scene;
	}
	if (!scene)
		return nullptr;
	struct Ctx {
		QPointF p;
		obs_sceneitem_t *hit;
	} c{cp, nullptr};
	// Items are enumerated bottom to top; the last hit is the one on top.
	obs_scene_enum_items(obs_scene_from_source(scene), [](obs_scene_t *, obs_sceneitem_t *it, void *d) {
		auto *c = static_cast<Ctx *>(d);
		if (obs_sceneitem_visible(it) && inside(it, c->p))
			c->hit = it;
		return true;
	}, &c);
	return c.hit;
}

int Preview::handleAt(obs_sceneitem_t *item, const QPointF &wp) const
{
	if (!item || obs_sceneitem_locked(item))
		return -1;
	QPointF c[4];
	itemBox(item, c);
	const double k = canvasToWidget();
	QPointF hs[8];
	for (int i = 0; i < 4; i++) {
		hs[i * 2] = c[i];
		hs[i * 2 + 1] = (c[i] + c[(i + 1) % 4]) / 2;
	}
	const QPointF cp = toCanvas(wp);
	for (int i = 0; i < 8; i++) {
		const QPointF d = (hs[i] - cp) * k;
		if (std::abs(d.x()) <= 8 && std::abs(d.y()) <= 8)
			return i; // 0 TL, 1 T, 2 TR, 3 R, 4 BR, 5 B, 6 BL, 7 L
	}
	return -1;
}

void Preview::beginResize(obs_sceneitem_t *item)
{
	// Resizing works on the visible box: put the item's anchor at its top left without moving it.
	const QRectF box = itemBox(item);
	obs_transform_info info;
	candid_get_info(item, &info);
	m_boundsMode = info.bounds_type != OBS_BOUNDS_NONE;
	obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);
	obs_source_t *src = obs_sceneitem_get_source(item);
	m_baseSize = QSizeF(std::max(1, int(obs_source_get_width(src)) - crop.left - crop.right),
			    std::max(1, int(obs_source_get_height(src)) - crop.top - crop.bottom));
	if (m_boundsMode) {
		info.bounds_alignment = OBS_ALIGN_CENTER;
		info.bounds_type = OBS_BOUNDS_STRETCH;
		vec2_set(&info.bounds, float(box.width()), float(box.height()));
	}
	info.alignment = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
	vec2_set(&info.pos, float(box.left()), float(box.top()));
	candid_set_info(item, &info);
	m_resizeStart = box;
}

void Preview::applyResize(const QPointF &cp, bool freeAspect)
{
	obs_sceneitem_t *item = m_dragItem;
	QRectF r = m_resizeStart;
	const int h = m_handle;
	double l = r.left(), t = r.top(), rr = r.right(), b = r.bottom();
	if (h == 0 || h == 6 || h == 7)
		l = std::min(cp.x(), rr - 4);
	if (h == 2 || h == 3 || h == 4)
		rr = std::max(cp.x(), l + 4);
	if (h == 0 || h == 1 || h == 2)
		t = std::min(cp.y(), b - 4);
	if (h == 4 || h == 5 || h == 6)
		b = std::max(cp.y(), t + 4);
	QRectF n(QPointF(l, t), QPointF(rr, b));
	const bool corner = h % 2 == 0;
	if (corner && !freeAspect && r.height() > 0) {
		// Corners keep the shape; the larger change wins.
		const double ar = r.width() / r.height();
		const double w = std::max(n.width(), n.height() * ar);
		const double hh = w / ar;
		if (h == 0)
			n = QRectF(r.right() - w, r.bottom() - hh, w, hh);
		else if (h == 2)
			n = QRectF(r.left(), r.bottom() - hh, w, hh);
		else if (h == 4)
			n = QRectF(r.left(), r.top(), w, hh);
		else
			n = QRectF(r.right() - w, r.top(), w, hh);
	}
	obs_transform_info info;
	candid_get_info(item, &info);
	vec2_set(&info.pos, float(n.left()), float(n.top()));
	if (m_boundsMode) {
		vec2_set(&info.bounds, float(n.width()), float(n.height()));
	} else {
		vec2_set(&info.scale, float(n.width() / m_baseSize.width()), float(n.height() / m_baseSize.height()));
	}
	candid_set_info(item, &info);
}

QPointF Preview::snap(obs_sceneitem_t *item, const QPointF &pos) const
{
	if (!CFG.flag("general", "snap") || (QGuiApplication::keyboardModifiers() & Qt::ControlModifier))
		return pos;
	vec2 cur;
	obs_sceneitem_get_pos(item, &cur);
	const QRectF box = itemBox(item).translated(pos.x() - cur.x, pos.y() - cur.y);
	const QSize c = ENGINE.canvasSize();
	const double tol = 10.0 / canvasToWidget();
	double dx = 0, dy = 0, bestX = tol, bestY = tol;
	for (double target : {0.0, double(c.width()), c.width() / 2.0})
		for (double edge : {box.left(), box.right(), box.center().x()})
			if (std::abs(target - edge) < bestX) {
				bestX = std::abs(target - edge);
				dx = target - edge;
			}
	for (double target : {0.0, double(c.height()), c.height() / 2.0})
		for (double edge : {box.top(), box.bottom(), box.center().y()})
			if (std::abs(target - edge) < bestY) {
				bestY = std::abs(target - edge);
				dy = target - edge;
			}
	return pos + QPointF(dx, dy);
}

void Preview::mousePressEvent(QMouseEvent *e)
{
	if (m_mode != Edit)
		return;
	setFocus();
	const QPointF cp = toCanvas(e->position());
	obs_sceneitem_t *sel = ENGINE.selectedItem();
	if (e->button() == Qt::LeftButton) {
		const int h = handleAt(sel, e->position());
		if (h >= 0) {
			obs_transform_info info;
			candid_get_info(sel, &info);
			if (std::fmod(std::abs(info.rot), 360.0f) < 0.01f) {
				m_drag = Resize;
				m_handle = h;
				m_dragItem = sel;
				beginResize(sel);
				return;
			}
		}
	}
	obs_sceneitem_t *hit = itemAt(cp);
	// Clicking inside the selected item keeps it, even if something above overlaps.
	if (sel && inside(sel, cp) && obs_sceneitem_visible(sel))
		hit = sel;
	if (hit != sel)
		ENGINE.select(hit);
	if (e->button() == Qt::RightButton) {
		emit contextMenu(hit, e->globalPosition().toPoint());
		return;
	}
	if (e->button() == Qt::LeftButton && hit && !obs_sceneitem_locked(hit)) {
		m_drag = Move;
		m_dragItem = hit;
		m_dragStart = cp;
		vec2 p;
		obs_sceneitem_get_pos(hit, &p);
		m_itemStart = QPointF(p.x, p.y);
	}
}

void Preview::mouseMoveEvent(QMouseEvent *e)
{
	if (m_mode != Edit)
		return;
	const QPointF cp = toCanvas(e->position());
	if (m_drag == Move && m_dragItem) {
		QPointF np = m_itemStart + (cp - m_dragStart);
		np = snap(m_dragItem, np);
		vec2 v;
		vec2_set(&v, float(std::round(np.x())), float(std::round(np.y())));
		obs_sceneitem_set_pos(m_dragItem, &v);
		return;
	}
	if (m_drag == Resize && m_dragItem) {
		applyResize(cp, e->modifiers() & Qt::ShiftModifier);
		return;
	}
	obs_sceneitem_t *sel = ENGINE.selectedItem();
	const int h = handleAt(sel, e->position());
	static const Qt::CursorShape shapes[8] = {Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor,
						  Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor};
	obs_sceneitem_t *hit = itemAt(cp);
	if (h >= 0)
		setCursor(shapes[h]);
	else if (hit && !obs_sceneitem_locked(hit))
		setCursor(Qt::SizeAllCursor);
	else
		unsetCursor();
	m_hoverId = hit ? obs_sceneitem_get_id(hit) : -1;
}

void Preview::mouseReleaseEvent(QMouseEvent *)
{
	if (m_drag != None) {
		m_drag = None;
		m_dragItem = nullptr;
		m_handle = -1;
		ENGINE.save();
		emit ENGINE.itemTransformed();
	}
}

void Preview::mouseDoubleClickEvent(QMouseEvent *e)
{
	if (m_mode != Edit)
		return;
	if (obs_sceneitem_t *hit = itemAt(toCanvas(e->position())))
		emit itemDoubleClicked(hit);
}

void Preview::leaveEvent(QEvent *e)
{
	m_hoverId = -1;
	QWidget::leaveEvent(e);
}

void Preview::keyPressEvent(QKeyEvent *e)
{
	obs_sceneitem_t *sel = ENGINE.selectedItem();
	if (m_mode != Edit || !sel) {
		QWidget::keyPressEvent(e);
		return;
	}
	if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
		emit deletePressed();
		return;
	}
	const float step = (e->modifiers() & Qt::ShiftModifier) ? 10.0f : 1.0f;
	float dx = 0, dy = 0;
	switch (e->key()) {
	case Qt::Key_Left:
		dx = -step;
		break;
	case Qt::Key_Right:
		dx = step;
		break;
	case Qt::Key_Up:
		dy = -step;
		break;
	case Qt::Key_Down:
		dy = step;
		break;
	default:
		QWidget::keyPressEvent(e);
		return;
	}
	if (obs_sceneitem_locked(sel))
		return;
	vec2 p;
	obs_sceneitem_get_pos(sel, &p);
	p.x += dx;
	p.y += dy;
	obs_sceneitem_set_pos(sel, &p);
	ENGINE.save();
}
