#include "SourceList.hpp"
#include "Engine.hpp"
#include "Icons.hpp"
#include "Style.hpp"
#include "Widgets.hpp"

#include <QContextMenuEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <graphics/matrix4.h>

// Item operations -------------------------------------------------------------------------------------

static QRectF box(obs_sceneitem_t *item)
{
	matrix4 m;
	obs_sceneitem_get_box_transform(item, &m);
	double l = 1e9, t = 1e9, r = -1e9, b = -1e9;
	for (auto [x, y] : {std::pair{0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}, {1.f, 1.f}}) {
		vec3 v;
		vec3_set(&v, x, y, 0);
		vec3_transform(&v, &v, &m);
		l = std::min(l, double(v.x));
		r = std::max(r, double(v.x));
		t = std::min(t, double(v.y));
		b = std::max(b, double(v.y));
	}
	return QRectF(QPointF(l, t), QPointF(r, b));
}

static void keepCenter(obs_sceneitem_t *item, const QPointF &before)
{
	const QPointF after = box(item).center();
	vec2 p;
	obs_sceneitem_get_pos(item, &p);
	p.x += float(before.x() - after.x());
	p.y += float(before.y() - after.y());
	obs_sceneitem_set_pos(item, &p);
}

namespace ItemOps {

static void toCanvas(obs_sceneitem_t *item, obs_bounds_type type)
{
	const QSize c = ENGINE.canvasSize();
	obs_transform_info info;
	candid_get_info(item, &info);
	vec2_set(&info.pos, 0, 0);
	vec2_set(&info.scale, 1, 1);
	info.rot = 0;
	info.alignment = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
	info.bounds_type = type;
	info.bounds_alignment = OBS_ALIGN_CENTER;
	vec2_set(&info.bounds, float(c.width()), float(c.height()));
	candid_set_info(item, &info);
	ENGINE.save();
}

void fit(obs_sceneitem_t *item)
{
	toCanvas(item, OBS_BOUNDS_SCALE_INNER);
}

void stretch(obs_sceneitem_t *item)
{
	toCanvas(item, OBS_BOUNDS_STRETCH);
}

void center(obs_sceneitem_t *item)
{
	const QSize c = ENGINE.canvasSize();
	keepCenter(item, QPointF(c.width() / 2.0, c.height() / 2.0));
	ENGINE.save();
}

void reset(obs_sceneitem_t *item)
{
	obs_transform_info info;
	candid_get_info(item, &info);
	vec2_set(&info.pos, 0, 0);
	vec2_set(&info.scale, 1, 1);
	info.rot = 0;
	info.alignment = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
	info.bounds_type = OBS_BOUNDS_NONE;
	candid_set_info(item, &info);
	obs_sceneitem_crop crop = {};
	obs_sceneitem_set_crop(item, &crop);
	ENGINE.save();
}

void flip(obs_sceneitem_t *item, bool horizontal)
{
	const QPointF c = box(item).center();
	vec2 s;
	obs_sceneitem_get_scale(item, &s);
	if (horizontal)
		s.x = -s.x;
	else
		s.y = -s.y;
	obs_sceneitem_set_scale(item, &s);
	keepCenter(item, c);
	ENGINE.save();
}

void rotate(obs_sceneitem_t *item, float degrees)
{
	const QPointF c = box(item).center();
	float r = std::fmod(obs_sceneitem_get_rot(item) + degrees, 360.0f);
	if (r < 0)
		r += 360.0f;
	obs_sceneitem_set_rot(item, r);
	keepCenter(item, c);
	ENGINE.save();
}

void cropCanvas(obs_sceneitem_t *item, QWidget *parent)
{
	QString err;
	if (!ENGINE.cropCanvasTo(item, &err))
		QMessageBox::information(parent, QObject::tr("Crop canvas to feed"), err);
}

bool confirmRemove(obs_sceneitem_t *item, QWidget *parent)
{
	if (!item)
		return false;
	const QString name = QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item)));
	const auto r = QMessageBox::question(parent, QObject::tr("Remove source"), QObject::tr("Remove “%1” from this scene?").arg(name));
	if (r != QMessageBox::Yes)
		return false;
	ENGINE.removeItem(item);
	return true;
}

} // namespace ItemOps

// List ------------------------------------------------------------------------------------------------

QString SourceList::iconFor(obs_source_t *s)
{
	if (!s)
		return "scene";
	if (obs_source_is_scene(s))
		return "scene";
	if (obs_source_is_group(s))
		return "group";
	OBSDataAutoRelease priv = obs_source_get_private_settings(s);
	if (obs_data_get_bool(priv, "candid_area"))
		return "area";
	const QString id = QString::fromUtf8(obs_source_get_unversioned_id(s));
	if (id.contains("monitor") || id.contains("xshm") || id.contains("screen") || id.contains("desktop-capture"))
		return "screen";
	if (id.contains("window"))
		return "window";
	if (id.contains("game"))
		return "game";
	if (id.contains("dshow") || id.contains("v4l2") || id.contains("av_capture"))
		return "camera";
	if (id.contains("process_output"))
		return "appaudio";
	if (id.contains("input_capture"))
		return "mic";
	if (id.contains("output_capture"))
		return "speaker";
	if (id.contains("image") || id.contains("slideshow"))
		return "image";
	if (id.contains("ffmpeg") || id.contains("vlc"))
		return "media";
	if (id.contains("text"))
		return "text";
	if (id.contains("browser"))
		return "web";
	if (id.contains("color"))
		return "color";
	return "scene";
}

SourceList::SourceList(QWidget *parent) : QListWidget(parent)
{
	setDragDropMode(QAbstractItemView::InternalMove);
	setDefaultDropAction(Qt::MoveAction);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setUniformItemSizes(true);

	connect(this, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *cur) {
		if (m_updating)
			return;
		ENGINE.select(itemFor(cur));
	});
	connect(this, &QListWidget::itemDoubleClicked, this, [this] { renameSelected(); });
	connect(&ENGINE, &Engine::itemsChanged, this, &SourceList::refresh);
	connect(&ENGINE, &Engine::editSceneChanged, this, &SourceList::refresh);
	connect(&ENGINE, &Engine::selectionChanged, this, [this] {
		m_updating = true;
		obs_sceneitem_t *sel = ENGINE.selectedItem();
		const int64_t id = sel ? obs_sceneitem_get_id(sel) : -1;
		QListWidgetItem *match = nullptr;
		for (int i = 0; i < count(); i++)
			if (item(i)->data(Qt::UserRole).toLongLong() == id)
				match = item(i);
		setCurrentItem(match);
		if (!match)
			clearSelection();
		m_updating = false;
	});
}

obs_sceneitem_t *SourceList::itemFor(QListWidgetItem *it) const
{
	obs_scene_t *scene = ENGINE.editSceneObj();
	if (!it || !scene)
		return nullptr;
	return obs_scene_find_sceneitem_by_id(scene, it->data(Qt::UserRole).toLongLong());
}

void SourceList::refresh()
{
	m_updating = true;
	clear();
	obs_scene_t *scene = ENGINE.editSceneObj();
	if (!scene) {
		m_updating = false;
		return;
	}
	QVector<obs_sceneitem_t *> items;
	obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *it, void *p) {
		static_cast<QVector<obs_sceneitem_t *> *>(p)->prepend(it);
		return true;
	}, &items);

	obs_sceneitem_t *sel = ENGINE.selectedItem();
	for (obs_sceneitem_t *si : items) {
		obs_source_t *src = obs_sceneitem_get_source(si);
		auto *it = new QListWidgetItem(this);
		it->setData(Qt::UserRole, qlonglong(obs_sceneitem_get_id(si)));
		it->setSizeHint(QSize(0, 34));

		auto *row = new QWidget;
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(8, 0, 2, 0);
		h->setSpacing(8);
		auto *icon = new QLabel;
		icon->setPixmap(Icons::pixmap(iconFor(src), 16, si == sel ? Theme::current().fg : Theme::current().mute, devicePixelRatioF()));
		auto *name = new ElidedLabel;
		name->setFullText(QString::fromUtf8(obs_source_get_name(src)));
		const bool vis = obs_sceneitem_visible(si);
		if (!vis)
			name->setStyleSheet(QString("color: %1").arg(Theme::current().mute.name()));
		auto *eye = iconButton(vis ? "eye" : "eyeoff", vis ? tr("Hide") : tr("Show"), row);
		auto *lock = iconButton(obs_sceneitem_locked(si) ? "lock" : "unlock", obs_sceneitem_locked(si) ? tr("Unlock") : tr("Lock"), row);
		if (!obs_sceneitem_locked(si))
			lock->setStyleSheet("QPushButton { qproperty-iconSize: 14px 14px; } QPushButton:!hover { color: transparent; }");
		eye->setIconSize(QSize(14, 14));
		lock->setIconSize(QSize(14, 14));
		const int64_t id = obs_sceneitem_get_id(si);
		connect(eye, &QPushButton::clicked, this, [id] {
			if (obs_sceneitem_t *x = obs_scene_find_sceneitem_by_id(ENGINE.editSceneObj(), id)) {
				obs_sceneitem_set_visible(x, !obs_sceneitem_visible(x));
				ENGINE.save();
			}
		});
		connect(lock, &QPushButton::clicked, this, [id] {
			if (obs_sceneitem_t *x = obs_scene_find_sceneitem_by_id(ENGINE.editSceneObj(), id)) {
				obs_sceneitem_set_locked(x, !obs_sceneitem_locked(x));
				ENGINE.save();
			}
		});
		h->addWidget(icon);
		h->addWidget(name, 1);
		h->addWidget(eye);
		h->addWidget(lock);
		row->setAttribute(Qt::WA_TransparentForMouseEvents, false);
		setItemWidget(it, row);
		if (si == sel)
			setCurrentItem(it);
	}
	m_updating = false;
}

void SourceList::dropEvent(QDropEvent *e)
{
	QListWidget::dropEvent(e);
	obs_scene_t *scene = ENGINE.editSceneObj();
	if (!scene)
		return;
	// The list is top first; libobs wants bottom first.
	QVector<obs_sceneitem_t *> order;
	for (int i = count() - 1; i >= 0; i--)
		if (obs_sceneitem_t *si = itemFor(item(i)))
			order.append(si);
	obs_scene_reorder_items(scene, order.data(), size_t(order.size()));
	ENGINE.save();
	refresh();
}

void SourceList::renameSelected()
{
	obs_sceneitem_t *si = ENGINE.selectedItem();
	if (!si)
		return;
	obs_source_t *src = obs_sceneitem_get_source(si);
	bool ok = false;
	const QString n = QInputDialog::getText(this, tr("Rename source"), tr("Name"), QLineEdit::Normal,
						QString::fromUtf8(obs_source_get_name(src)), &ok);
	if (ok && !ENGINE.rename(src, n))
		QMessageBox::information(this, tr("Rename source"), tr("That name is already used, or empty."));
}

void SourceList::keyPressEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_Delete) {
		ItemOps::confirmRemove(ENGINE.selectedItem(), this);
		return;
	}
	if (e->key() == Qt::Key_F2) {
		renameSelected();
		return;
	}
	QListWidget::keyPressEvent(e);
}

void SourceList::fillItemMenu(QMenu &m, obs_sceneitem_t *item, QWidget *parent)
{
	if (!item)
		return;
	const int64_t id = obs_sceneitem_get_id(item);
	auto with = [id](auto fn) {
		return [id, fn] {
			if (obs_sceneitem_t *x = obs_scene_find_sceneitem_by_id(ENGINE.editSceneObj(), id))
				fn(x);
		};
	};
	auto *tr = m.addMenu(Icons::get("fit"), QObject::tr("Transform"));
	tr->addAction(QObject::tr("Fit to canvas"), with(ItemOps::fit));
	tr->addAction(QObject::tr("Stretch to canvas"), with(ItemOps::stretch));
	tr->addAction(QObject::tr("Center"), with(ItemOps::center));
	tr->addSeparator();
	tr->addAction(QObject::tr("Rotate 90° right"), with([](obs_sceneitem_t *x) { ItemOps::rotate(x, 90); }));
	tr->addAction(QObject::tr("Rotate 90° left"), with([](obs_sceneitem_t *x) { ItemOps::rotate(x, -90); }));
	tr->addAction(QObject::tr("Flip horizontally"), with([](obs_sceneitem_t *x) { ItemOps::flip(x, true); }));
	tr->addAction(QObject::tr("Flip vertically"), with([](obs_sceneitem_t *x) { ItemOps::flip(x, false); }));
	tr->addSeparator();
	tr->addAction(QObject::tr("Reset"), with(ItemOps::reset));

	auto *ord = m.addMenu(Icons::get("up"), QObject::tr("Order"));
	ord->addAction(QObject::tr("Bring to front"), with([](obs_sceneitem_t *x) { obs_sceneitem_set_order(x, OBS_ORDER_MOVE_TOP); }));
	ord->addAction(QObject::tr("Bring forward"), with([](obs_sceneitem_t *x) { obs_sceneitem_set_order(x, OBS_ORDER_MOVE_UP); }));
	ord->addAction(QObject::tr("Send backward"), with([](obs_sceneitem_t *x) { obs_sceneitem_set_order(x, OBS_ORDER_MOVE_DOWN); }));
	ord->addAction(QObject::tr("Send to back"), with([](obs_sceneitem_t *x) { obs_sceneitem_set_order(x, OBS_ORDER_MOVE_BOTTOM); }));

	m.addAction(Icons::get("crop"), QObject::tr("Crop canvas to this source"), with([parent](obs_sceneitem_t *x) { ItemOps::cropCanvas(x, parent); }));
	m.addSeparator();
	const bool vis = obs_sceneitem_visible(item), locked = obs_sceneitem_locked(item);
	m.addAction(Icons::get(vis ? "eyeoff" : "eye"), vis ? QObject::tr("Hide") : QObject::tr("Show"),
		    with([](obs_sceneitem_t *x) { obs_sceneitem_set_visible(x, !obs_sceneitem_visible(x)); }));
	m.addAction(Icons::get(locked ? "unlock" : "lock"), locked ? QObject::tr("Unlock") : QObject::tr("Lock"),
		    with([](obs_sceneitem_t *x) { obs_sceneitem_set_locked(x, !obs_sceneitem_locked(x)); }));
	m.addSeparator();
	m.addAction(Icons::get("trash"), QObject::tr("Remove"), with([parent](obs_sceneitem_t *x) { ItemOps::confirmRemove(x, parent); }));
}

void SourceList::contextMenuEvent(QContextMenuEvent *e)
{
	QMenu m(this);
	QListWidgetItem *it = itemAt(e->pos());
	m.addAction(Icons::get("plus"), tr("Add source"), this, &SourceList::addRequested);
	if (obs_sceneitem_t *si = itemFor(it)) {
		ENGINE.select(si);
		m.addAction(Icons::get("edit"), tr("Rename"), this, &SourceList::renameSelected);
		m.addAction(Icons::get("filters"), tr("Filters"), this, &SourceList::filtersRequested);
		m.addSeparator();
		fillItemMenu(m, si, this);
	}
	m.exec(e->globalPos());
}
