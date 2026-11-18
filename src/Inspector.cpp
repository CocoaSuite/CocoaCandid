#include "Inspector.hpp"
#include "Engine.hpp"
#include "Icons.hpp"
#include "PropertiesForm.hpp"
#include "SourceList.hpp"
#include "Style.hpp"
#include "Widgets.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <graphics/matrix4.h>

// Filters ---------------------------------------------------------------------------------------------

namespace {
class FiltersEditor : public QWidget {
public:
	FiltersEditor(obs_source_t *source, QWidget *parent) : QWidget(parent), m_source(source)
	{
		auto *v = new QVBoxLayout(this);
		v->setContentsMargins(0, 0, 0, 0);
		v->setSpacing(8);
		m_list = new QListWidget;
		m_list->setFixedHeight(150);
		m_list->setStyleSheet(QString("QListWidget { border: 1px solid %1; border-radius: 7px; padding: 3px; }").arg(Theme::current().line.name()));
		v->addWidget(m_list);

		auto *row = new QHBoxLayout;
		row->setSpacing(4);
		auto *add = new QPushButton(Icons::get("plus"), tr("Add filter"));
		auto *menu = new QMenu(add);
		add->setMenu(menu);
		connect(menu, &QMenu::aboutToShow, this, [this, menu] { fillAddMenu(menu); });
		auto *up = iconButton("up", tr("Move up"));
		auto *down = iconButton("down", tr("Move down"));
		auto *rm = iconButton("trash", tr("Remove filter"));
		row->addWidget(add);
		row->addStretch(1);
		row->addWidget(up);
		row->addWidget(down);
		row->addWidget(rm);
		v->addLayout(row);

		m_editor = new QVBoxLayout;
		m_editor->setContentsMargins(0, 6, 0, 0);
		v->addLayout(m_editor);
		v->addStretch(1);

		connect(m_list, &QListWidget::currentRowChanged, this, [this] { showEditor(); });
		connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem *it) {
			if (obs_source_t *f = filterAt(m_list->row(it)))
				obs_source_set_enabled(f, it->checkState() == Qt::Checked);
			ENGINE.save();
		});
		connect(up, &QPushButton::clicked, this, [this] { move(OBS_ORDER_MOVE_UP, -1); });
		connect(down, &QPushButton::clicked, this, [this] { move(OBS_ORDER_MOVE_DOWN, 1); });
		connect(rm, &QPushButton::clicked, this, [this] {
			if (obs_source_t *f = filterAt(m_list->currentRow())) {
				obs_source_filter_remove(m_source, f);
				refresh(-1);
				ENGINE.save();
			}
		});
		refresh(0);
	}

private:
	QVector<OBSSource> filters() const
	{
		QVector<OBSSource> out;
		obs_source_enum_filters(m_source, [](obs_source_t *, obs_source_t *f, void *p) {
			static_cast<QVector<OBSSource> *>(p)->append(OBSSource(f));
		}, &out);
		return out;
	}
	obs_source_t *filterAt(int row) const
	{
		const auto f = filters();
		return row >= 0 && row < f.size() ? f[row].Get() : nullptr;
	}
	void refresh(int select)
	{
		const QSignalBlocker b(m_list);
		m_list->clear();
		for (const auto &f : filters()) {
			auto *it = new QListWidgetItem(QString::fromUtf8(obs_source_get_name(f)), m_list);
			it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
			it->setCheckState(obs_source_enabled(f) ? Qt::Checked : Qt::Unchecked);
			it->setSizeHint(QSize(0, 30));
		}
		if (m_list->count() == 0) {
			auto *it = new QListWidgetItem(tr("No filters yet"), m_list);
			it->setFlags(Qt::NoItemFlags);
		}
		m_list->setCurrentRow(select < 0 ? m_list->count() - 1 : std::min(select, m_list->count() - 1));
		showEditor();
	}
	void showEditor()
	{
		while (QLayoutItem *i = m_editor->takeAt(0)) {
			if (i->widget())
				i->widget()->deleteLater();
			delete i;
		}
		if (obs_source_t *f = filterAt(m_list->currentRow())) {
			auto *head = panelHeader(QString::fromUtf8(obs_source_get_name(f)));
			m_editor->addWidget(head);
			m_editor->addWidget(PropertiesForm::forSource(f));
		}
	}
	void move(obs_order_movement mv, int delta)
	{
		const int row = m_list->currentRow();
		if (obs_source_t *f = filterAt(row)) {
			obs_source_filter_set_order(m_source, f, mv);
			refresh(row + delta);
			ENGINE.save();
		}
	}
	void fillAddMenu(QMenu *menu)
	{
		menu->clear();
		const uint32_t sf = obs_source_get_output_flags(m_source);
		QVector<QPair<QString, QString>> video, audio;
		const char *id;
		for (size_t i = 0; obs_enum_filter_types(i, &id); i++) {
			const uint32_t ff = obs_get_source_output_flags(id);
			if (ff & (OBS_SOURCE_DEPRECATED | OBS_SOURCE_CAP_DISABLED))
				continue;
			const char *name = obs_source_get_display_name(id);
			if (!name)
				continue;
			const bool v = (ff & OBS_SOURCE_VIDEO) && (sf & OBS_SOURCE_VIDEO);
			const bool a = (ff & OBS_SOURCE_AUDIO) && (sf & OBS_SOURCE_AUDIO);
			if (v)
				video.append({QString::fromUtf8(id), QString::fromUtf8(name)});
			else if (a)
				audio.append({QString::fromUtf8(id), QString::fromUtf8(name)});
		}
		auto byName = [](auto &a, auto &b) { return a.second.localeAwareCompare(b.second) < 0; };
		std::sort(video.begin(), video.end(), byName);
		std::sort(audio.begin(), audio.end(), byName);
		auto addAll = [&](const QVector<QPair<QString, QString>> &list) {
			for (const auto &[fid, fname] : list)
				menu->addAction(fname, this, [this, fid, fname] {
					QString name = fname;
					for (int n = 2; obs_source_get_filter_by_name(m_source, name.toUtf8().constData()); n++) {
						OBSSourceAutoRelease ex = obs_source_get_filter_by_name(m_source, name.toUtf8().constData());
						(void)ex;
						name = QString("%1 %2").arg(fname).arg(n);
					}
					OBSSourceAutoRelease f = obs_source_create(fid.toUtf8().constData(), name.toUtf8().constData(), nullptr, nullptr);
					if (f) {
						obs_source_filter_add(m_source, f);
						refresh(-1);
						ENGINE.save();
					}
				});
		};
		if (!video.isEmpty()) {
			menu->addSection(tr("Picture"));
			addAll(video);
		}
		if (!audio.isEmpty()) {
			menu->addSection(tr("Sound"));
			addAll(audio);
		}
	}

	OBSSource m_source;
	QListWidget *m_list;
	QVBoxLayout *m_editor;
};
} // namespace

QWidget *Inspector::filtersEditor(obs_source_t *source, QWidget *parent)
{
	return new FiltersEditor(source, parent);
}

// Inspector ------------------------------------------------------------------------------------------

Inspector::Inspector(QWidget *parent) : QWidget(parent)
{
	setObjectName("panel");
	setAttribute(Qt::WA_StyledBackground);
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(0);

	auto *tabs = new QWidget;
	auto *th = new QHBoxLayout(tabs);
	th->setContentsMargins(8, 6, 8, 0);
	th->setSpacing(2);
	m_tabProps = new QPushButton(tr("Properties"));
	m_tabFilters = new QPushButton(tr("Filters"));
	for (auto *b : {m_tabProps, m_tabFilters}) {
		b->setCheckable(true);
		setKind(b, "tab");
		th->addWidget(b);
	}
	th->addStretch(1);
	m_tabProps->setChecked(true);
	v->addWidget(tabs);
	v->addWidget(hline());

	m_scroll = new QScrollArea;
	m_scroll->setWidgetResizable(true);
	m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	v->addWidget(m_scroll, 1);

	connect(m_tabProps, &QPushButton::clicked, this, &Inspector::showProperties);
	connect(m_tabFilters, &QPushButton::clicked, this, &Inspector::showFilters);
	connect(&ENGINE, &Engine::selectionChanged, this, [this] {
		obs_sceneitem_t *it = ENGINE.selectedItem();
		const int64_t id = it ? obs_sceneitem_get_id(it) : -1;
		if (id != m_shownItem || (it && obs_sceneitem_get_source(it) != m_shownSource))
			rebuild();
	});
	connect(&ENGINE, &Engine::editSceneChanged, this, &Inspector::rebuild);
	connect(&ENGINE, &Engine::itemTransformed, this, &Inspector::refreshTransform);
	rebuild();
}

void Inspector::showFilters()
{
	m_filters = true;
	m_tabProps->setChecked(false);
	m_tabFilters->setChecked(true);
	rebuild();
}

void Inspector::showProperties()
{
	m_filters = false;
	m_tabProps->setChecked(true);
	m_tabFilters->setChecked(false);
	rebuild();
}

static QDoubleSpinBox *numBox(double min, double max, int decimals, const QString &suffix = {})
{
	auto *s = new QDoubleSpinBox;
	s->setRange(min, max);
	s->setDecimals(decimals);
	s->setSuffix(suffix);
	s->setKeyboardTracking(false);
	s->setButtonSymbols(QAbstractSpinBox::NoButtons);
	s->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	s->setMinimumWidth(48);
	return s;
}

static QRectF itemBox(obs_sceneitem_t *item)
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

QWidget *Inspector::buildTransform(obs_sceneitem_t *item)
{
	auto *w = new QWidget;
	auto *v = new QVBoxLayout(w);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(8);
	const int64_t id = obs_sceneitem_get_id(item);
	auto cur = [id]() { return obs_scene_find_sceneitem_by_id(ENGINE.editSceneObj(), id); };

	v->addWidget(panelHeader(tr("Position and size")));
	auto *g = new QGridLayout;
	g->setHorizontalSpacing(8);
	g->setVerticalSpacing(8);
	m_x = numBox(-100000, 100000, 0, " px");
	m_y = numBox(-100000, 100000, 0, " px");
	m_w = numBox(1, 100000, 0, " px");
	m_h = numBox(1, 100000, 0, " px");
	m_rot = numBox(-360, 360, 1, "°");
	const QList<QPair<QString, QDoubleSpinBox *>> cells = {{"X", m_x}, {"Y", m_y}, {"W", m_w}, {"H", m_h}};
	for (int i = 0; i < 4; i++) {
		auto *l = new QLabel(cells[i].first);
		l->setProperty("mute", true);
		g->addWidget(l, i / 2, (i % 2) * 2);
		g->addWidget(cells[i].second, i / 2, (i % 2) * 2 + 1);
	}
	auto *rl = new QLabel(tr("Rotation"));
	rl->setProperty("mute", true);
	g->addWidget(rl, 2, 0, 1, 1);
	g->addWidget(m_rot, 2, 1, 1, 3);
	g->setColumnStretch(1, 1);
	g->setColumnStretch(3, 1);
	v->addLayout(g);

	auto setPos = [cur, this] {
		if (obs_sceneitem_t *it = cur()) {
			vec2 p;
			vec2_set(&p, float(m_x->value()), float(m_y->value()));
			obs_sceneitem_set_pos(it, &p);
			ENGINE.save();
		}
	};
	connect(m_x, &QDoubleSpinBox::valueChanged, this, setPos);
	connect(m_y, &QDoubleSpinBox::valueChanged, this, setPos);
	auto setSize = [cur, this] {
		obs_sceneitem_t *it = cur();
		if (!it)
			return;
		obs_transform_info info;
		candid_get_info(it, &info);
		if (info.bounds_type != OBS_BOUNDS_NONE) {
			vec2_set(&info.bounds, float(m_w->value()), float(m_h->value()));
		} else {
			obs_sceneitem_crop crop;
			obs_sceneitem_get_crop(it, &crop);
			obs_source_t *s = obs_sceneitem_get_source(it);
			const double bw = std::max(1, int(obs_source_get_width(s)) - crop.left - crop.right);
			const double bh = std::max(1, int(obs_source_get_height(s)) - crop.top - crop.bottom);
			vec2_set(&info.scale, float(m_w->value() / bw) * (info.scale.x < 0 ? -1 : 1),
				 float(m_h->value() / bh) * (info.scale.y < 0 ? -1 : 1));
		}
		candid_set_info(it, &info);
		ENGINE.save();
	};
	connect(m_w, &QDoubleSpinBox::valueChanged, this, setSize);
	connect(m_h, &QDoubleSpinBox::valueChanged, this, setSize);
	connect(m_rot, &QDoubleSpinBox::valueChanged, this, [cur](double r) {
		if (obs_sceneitem_t *it = cur()) {
			obs_sceneitem_set_rot(it, float(r));
			ENGINE.save();
		}
	});

	auto *btns = new QHBoxLayout;
	btns->setSpacing(6);
	for (auto [label, fn] : {std::pair<QString, void (*)(obs_sceneitem_t *)>{tr("Fit"), ItemOps::fit},
				 {tr("Stretch"), ItemOps::stretch},
				 {tr("Center"), ItemOps::center},
				 {tr("Reset"), ItemOps::reset}}) {
		auto *b = new QPushButton(label);
		connect(b, &QPushButton::clicked, this, [cur, fn] {
			if (obs_sceneitem_t *it = cur())
				fn(it);
		});
		btns->addWidget(b);
	}
	btns->addStretch(1);
	v->addLayout(btns);

	v->addWidget(panelHeader(tr("Crop")));
	auto *cg = new QGridLayout;
	cg->setHorizontalSpacing(6);
	const QStringList sides = {tr("Left"), tr("Top"), tr("Right"), tr("Bottom")};
	for (int i = 0; i < 4; i++) {
		m_crop[i] = new QSpinBox;
		m_crop[i]->setRange(0, 16384);
		m_crop[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);
		m_crop[i]->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		m_crop[i]->setMinimumWidth(36);
		m_crop[i]->setKeyboardTracking(false);
		m_crop[i]->setToolTip(sides[i]);
		m_crop[i]->setAccessibleName(tr("Crop %1").arg(sides[i]));
		auto *l = new QLabel(sides[i]);
		l->setProperty("mute", true);
		l->setStyleSheet("font-size: 11px");
		cg->addWidget(m_crop[i], 0, i);
		cg->addWidget(l, 1, i, Qt::AlignHCenter);
		connect(m_crop[i], &QSpinBox::valueChanged, this, [cur, this] {
			if (obs_sceneitem_t *it = cur()) {
				obs_sceneitem_crop c = {m_crop[0]->value(), m_crop[1]->value(), m_crop[2]->value(), m_crop[3]->value()};
				obs_sceneitem_set_crop(it, &c);
				ENGINE.save();
			}
		});
	}
	v->addLayout(cg);

	v->addWidget(panelHeader(tr("Blending")));
	auto *bg = new QGridLayout;
	bg->setHorizontalSpacing(10);
	bg->setColumnMinimumWidth(0, 92);
	auto *blend = new QComboBox;
	blend->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	const QList<QPair<QString, obs_blending_type>> modes = {
		{tr("Normal"), OBS_BLEND_NORMAL},   {tr("Additive"), OBS_BLEND_ADDITIVE}, {tr("Subtract"), OBS_BLEND_SUBTRACT},
		{tr("Screen"), OBS_BLEND_SCREEN},   {tr("Multiply"), OBS_BLEND_MULTIPLY}, {tr("Lighten"), OBS_BLEND_LIGHTEN},
		{tr("Darken"), OBS_BLEND_DARKEN},
	};
	for (const auto &m : modes)
		blend->addItem(m.first, int(m.second));
	blend->setCurrentIndex(std::max(0, blend->findData(int(obs_sceneitem_get_blending_mode(item)))));
	connect(blend, &QComboBox::activated, this, [cur, blend](int i) {
		if (obs_sceneitem_t *it = cur()) {
			obs_sceneitem_set_blending_mode(it, obs_blending_type(blend->itemData(i).toInt()));
			ENGINE.save();
		}
	});
	auto *scale = new QComboBox;
	scale->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	const QList<QPair<QString, obs_scale_type>> scales = {
		{tr("Automatic"), OBS_SCALE_DISABLE}, {tr("Lanczos (sharpest)"), OBS_SCALE_LANCZOS}, {tr("Bicubic"), OBS_SCALE_BICUBIC},
		{tr("Bilinear"), OBS_SCALE_BILINEAR}, {tr("Area"), OBS_SCALE_AREA},                  {tr("Pixel art (point)"), OBS_SCALE_POINT},
	};
	for (const auto &s : scales)
		scale->addItem(s.first, int(s.second));
	scale->setCurrentIndex(std::max(0, scale->findData(int(obs_sceneitem_get_scale_filter(item)))));
	connect(scale, &QComboBox::activated, this, [cur, scale](int i) {
		if (obs_sceneitem_t *it = cur()) {
			obs_sceneitem_set_scale_filter(it, obs_scale_type(scale->itemData(i).toInt()));
			ENGINE.save();
		}
	});
	auto l1 = new QLabel(tr("Blend")), l2 = new QLabel(tr("Scaling"));
	l1->setProperty("mute", true);
	l2->setProperty("mute", true);
	bg->addWidget(l1, 0, 0);
	bg->addWidget(blend, 0, 1);
	bg->addWidget(l2, 1, 0);
	bg->addWidget(scale, 1, 1);
	v->addLayout(bg);

	refreshTransform();
	return w;
}

void Inspector::refreshTransform()
{
	obs_sceneitem_t *it = ENGINE.selectedItem();
	if (!it || !m_x || obs_sceneitem_get_id(it) != m_shownItem)
		return;
	vec2 pos;
	obs_sceneitem_get_pos(it, &pos);
	const QRectF b = itemBox(it);
	obs_sceneitem_crop c;
	obs_sceneitem_get_crop(it, &c);
	auto set = [](QAbstractSpinBox *box, double v) {
		if (box->hasFocus())
			return;
		const QSignalBlocker bl(box);
		if (auto *d = qobject_cast<QDoubleSpinBox *>(box))
			d->setValue(v);
		else if (auto *i = qobject_cast<QSpinBox *>(box))
			i->setValue(int(v));
	};
	set(m_x, pos.x);
	set(m_y, pos.y);
	set(m_w, b.width());
	set(m_h, b.height());
	set(m_rot, obs_sceneitem_get_rot(it));
	set(m_crop[0], c.left);
	set(m_crop[1], c.top);
	set(m_crop[2], c.right);
	set(m_crop[3], c.bottom);
}

void Inspector::rebuild()
{
	m_x = m_y = m_w = m_h = m_rot = nullptr;
	for (auto &c : m_crop)
		c = nullptr;
	obs_sceneitem_t *item = ENGINE.selectedItem();
	m_shownItem = item ? obs_sceneitem_get_id(item) : -1;
	m_shownSource = item ? obs_sceneitem_get_source(item) : nullptr;

	auto *content = new QWidget;
	auto *v = new QVBoxLayout(content);
	v->setContentsMargins(12, 12, 12, 16);
	v->setSpacing(10);

	if (!item) {
		auto *l = new QLabel(tr("Pick a source on the canvas or in Sources to see its settings here."));
		l->setWordWrap(true);
		l->setProperty("mute", true);
		v->addWidget(l);
		v->addStretch(1);
		m_scroll->setWidget(content);
		return;
	}

	obs_source_t *src = obs_sceneitem_get_source(item);
	auto *head = new QHBoxLayout;
	auto *name = new ElidedLabel;
	name->setObjectName("title");
	name->setFullText(QString::fromUtf8(obs_source_get_name(src)));
	auto *kind = new ElidedLabel;
	kind->setFullText(QString::fromUtf8(obs_source_get_display_name(obs_source_get_id(src))));
	kind->setMaximumWidth(130);
	kind->setProperty("mute", true);
	kind->setStyleSheet("font-size: 11.5px");
	head->addWidget(name, 1);
	head->addWidget(kind);
	v->addLayout(head);

	if (m_filters) {
		v->addWidget(filtersEditor(src, content));
	} else {
		auto *form = PropertiesForm::forSource(src, content);
		connect(form, &PropertiesForm::changed, this, [] { ENGINE.save(); });
		if (!form->isEmpty())
			v->addWidget(form);
		else
			delete form;
		const uint32_t flags = obs_source_get_output_flags(src);
		if (flags & OBS_SOURCE_VIDEO)
			v->addWidget(buildTransform(item));
		if (flags & OBS_SOURCE_AUDIO) {
			v->addWidget(panelHeader(tr("Sound")));
			auto *g = new QGridLayout;
			g->setColumnMinimumWidth(0, 92);
			auto *mon = new QComboBox;
			mon->addItem(tr("Off"), int(OBS_MONITORING_TYPE_NONE));
			mon->addItem(tr("Monitor only (mute output)"), int(OBS_MONITORING_TYPE_MONITOR_ONLY));
			mon->addItem(tr("Monitor and output"), int(OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT));
			mon->setCurrentIndex(std::max(0, mon->findData(int(obs_source_get_monitoring_type(src)))));
			OBSSource keep(src);
			connect(mon, &QComboBox::activated, this, [keep, mon](int i) {
				obs_source_set_monitoring_type(keep, obs_monitoring_type(mon->itemData(i).toInt()));
				ENGINE.save();
			});
			auto *sync = new QSpinBox;
			sync->setRange(-950, 20000);
			sync->setSuffix(" ms");
			sync->setValue(int(obs_source_get_sync_offset(src) / 1000000));
			connect(sync, &QSpinBox::valueChanged, this, [keep](int ms) {
				obs_source_set_sync_offset(keep, int64_t(ms) * 1000000);
				ENGINE.save();
			});
			auto *l1 = new QLabel(tr("Listen")), *l2 = new QLabel(tr("Sync delay"));
			l1->setProperty("mute", true);
			l2->setProperty("mute", true);
			g->addWidget(l1, 0, 0);
			g->addWidget(mon, 0, 1);
			g->addWidget(l2, 1, 0);
			g->addWidget(sync, 1, 1);
			v->addLayout(g);
		}
	}
	v->addStretch(1);
	m_scroll->setWidget(content);
}
