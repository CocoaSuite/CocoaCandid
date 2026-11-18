#include "PropertiesForm.hpp"
#include "Icons.hpp"
#include "Style.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

PropertiesForm::PropertiesForm(obs_data_t *settings, MakeProps make, OnChange onChange, void *object, QWidget *parent)
	: QWidget(parent), m_settings(settings), m_make(std::move(make)), m_onChange(std::move(onChange)), m_object(object)
{
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(0);
	rebuild();
}

PropertiesForm::~PropertiesForm()
{
	if (m_props)
		obs_properties_destroy(m_props);
}

PropertiesForm *PropertiesForm::forSource(obs_source_t *source, QWidget *parent)
{
	OBSDataAutoRelease s = obs_source_get_settings(source);
	OBSSource keep(source);
	return new PropertiesForm(
		s, [keep] { return obs_source_properties(keep); },
		[keep](obs_data_t *d) { obs_source_update(keep, d); }, source, parent);
}

void PropertiesForm::scheduleRebuild()
{
	if (m_rebuildQueued)
		return;
	m_rebuildQueued = true;
	QTimer::singleShot(0, this, [this] {
		m_rebuildQueued = false;
		rebuild();
	});
}

void PropertiesForm::rebuild()
{
	if (m_body) {
		m_body->hide();
		m_body->deleteLater();
		m_body = nullptr;
	}
	if (m_props)
		obs_properties_destroy(m_props);
	m_props = m_make ? m_make() : nullptr;
	m_rows = 0;
	m_body = new QWidget(this);
	auto *grid = new QGridLayout(m_body);
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setHorizontalSpacing(10);
	grid->setVerticalSpacing(8);
	grid->setColumnMinimumWidth(0, 92);
	grid->setColumnStretch(1, 1);
	m_body->setMinimumWidth(0);
	int row = 0;
	if (m_props) {
		obs_properties_apply_settings(m_props, m_settings);
		for (obs_property_t *p = obs_properties_first(m_props); p; obs_property_next(&p))
			addProperty(p, grid, row);
	}
	m_rows = row;
	static_cast<QVBoxLayout *>(layout())->addWidget(m_body);
}

void PropertiesForm::commit(obs_property_t *p)
{
	if (m_onChange)
		m_onChange(m_settings);
	if (p && obs_property_modified(p, m_settings))
		scheduleRebuild();
	emit changed();
}

static QLabel *rowLabel(const QString &t)
{
	auto *l = new QLabel(t);
	l->setWordWrap(true);
	l->setStyleSheet(QString("color: %1").arg(Theme::current().dark ? "#b5b5b5" : "#4a4a4a"));
	return l;
}

static QString colorText(long long c)
{
	const QColor q(int(c & 0xff), int((c >> 8) & 0xff), int((c >> 16) & 0xff));
	return q.name().toUpper();
}

void PropertiesForm::addProperty(obs_property_t *p, QGridLayout *grid, int &row)
{
	if (!obs_property_visible(p))
		return;
	const QByteArray name = obs_property_name(p);
	const QString desc = QString::fromUtf8(obs_property_description(p));
	const QString tip = QString::fromUtf8(obs_property_long_description(p));
	const bool enabled = obs_property_enabled(p);
	obs_data_t *S = m_settings;
	auto prop = [this, name]() { return obs_properties_get(m_props, name.constData()); };

	QWidget *w = nullptr;
	bool fullRow = false;

	switch (obs_property_get_type(p)) {
	case OBS_PROPERTY_BOOL: {
		auto *c = new QCheckBox(desc);
		c->setChecked(obs_data_get_bool(S, name));
		connect(c, &QCheckBox::toggled, this, [this, S, name, prop](bool v) {
			obs_data_set_bool(S, name, v);
			commit(prop());
		});
		w = c;
		fullRow = true;
		break;
	}
	case OBS_PROPERTY_INT: {
		auto *box = new QWidget;
		auto *h = new QHBoxLayout(box);
		h->setContentsMargins(0, 0, 0, 0);
		auto *sp = new QSpinBox;
		sp->setRange(obs_property_int_min(p), obs_property_int_max(p));
		sp->setSingleStep(std::max(1, obs_property_int_step(p)));
		sp->setSuffix(QString::fromUtf8(obs_property_int_suffix(p)));
		sp->setValue(int(obs_data_get_int(S, name)));
		sp->setKeyboardTracking(false);
		sp->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		sp->setMinimumWidth(60);
		if (obs_property_int_type(p) == OBS_NUMBER_SLIDER) {
			auto *sl = new QSlider(Qt::Horizontal);
			sl->setRange(sp->minimum(), sp->maximum());
			sl->setSingleStep(sp->singleStep());
			sl->setValue(sp->value());
			connect(sl, &QSlider::valueChanged, sp, &QSpinBox::setValue);
			connect(sp, &QSpinBox::valueChanged, sl, [sl](int v) {
				const QSignalBlocker b(sl);
				sl->setValue(v);
			});
			h->addWidget(sl, 1);
			sp->setFixedWidth(76);
		}
		h->addWidget(sp, obs_property_int_type(p) == OBS_NUMBER_SLIDER ? 0 : 1);
		connect(sp, &QSpinBox::valueChanged, this, [this, S, name, prop](int v) {
			obs_data_set_int(S, name, v);
			commit(prop());
		});
		w = box;
		break;
	}
	case OBS_PROPERTY_FLOAT: {
		auto *box = new QWidget;
		auto *h = new QHBoxLayout(box);
		h->setContentsMargins(0, 0, 0, 0);
		auto *sp = new QDoubleSpinBox;
		const double step = obs_property_float_step(p);
		sp->setRange(obs_property_float_min(p), obs_property_float_max(p));
		sp->setSingleStep(step > 0 ? step : 0.1);
		sp->setDecimals(step >= 1 ? 0 : step >= 0.1 ? 1 : step >= 0.01 ? 2 : 3);
		sp->setSuffix(QString::fromUtf8(obs_property_float_suffix(p)));
		sp->setValue(obs_data_get_double(S, name));
		sp->setKeyboardTracking(false);
		sp->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		sp->setMinimumWidth(60);
		if (obs_property_float_type(p) == OBS_NUMBER_SLIDER) {
			auto *sl = new QSlider(Qt::Horizontal);
			const double k = 1.0 / (step > 0 ? step : 0.01);
			sl->setRange(int(sp->minimum() * k), int(sp->maximum() * k));
			sl->setValue(int(sp->value() * k));
			connect(sl, &QSlider::valueChanged, sp, [sp, k](int v) { sp->setValue(v / k); });
			connect(sp, &QDoubleSpinBox::valueChanged, sl, [sl, k](double v) {
				const QSignalBlocker b(sl);
				sl->setValue(int(v * k));
			});
			h->addWidget(sl, 1);
			sp->setFixedWidth(76);
		}
		h->addWidget(sp, obs_property_float_type(p) == OBS_NUMBER_SLIDER ? 0 : 1);
		connect(sp, &QDoubleSpinBox::valueChanged, this, [this, S, name, prop](double v) {
			obs_data_set_double(S, name, v);
			commit(prop());
		});
		w = box;
		break;
	}
	case OBS_PROPERTY_TEXT: {
		const obs_text_type tt = obs_property_text_type(p);
		if (tt == OBS_TEXT_INFO) {
			auto *l = new QLabel(desc);
			l->setWordWrap(true);
			l->setTextFormat(Qt::PlainText);
			l->setProperty("mute", true);
			w = l;
			fullRow = true;
		} else if (tt == OBS_TEXT_MULTILINE) {
			auto *e = new QPlainTextEdit(QString::fromUtf8(obs_data_get_string(S, name)));
			e->setFixedHeight(84);
			connect(e, &QPlainTextEdit::textChanged, this, [this, e, S, name, prop] {
				obs_data_set_string(S, name, e->toPlainText().toUtf8().constData());
				commit(prop());
			});
			w = e;
		} else {
			auto *e = new QLineEdit(QString::fromUtf8(obs_data_get_string(S, name)));
			if (tt == OBS_TEXT_PASSWORD)
				e->setEchoMode(QLineEdit::Password);
			connect(e, &QLineEdit::textEdited, this, [this, S, name, prop](const QString &t) {
				obs_data_set_string(S, name, t.toUtf8().constData());
				commit(prop());
			});
			w = e;
		}
		break;
	}
	case OBS_PROPERTY_PATH: {
		auto *box = new QWidget;
		auto *h = new QHBoxLayout(box);
		h->setContentsMargins(0, 0, 0, 0);
		auto *e = new QLineEdit(QString::fromUtf8(obs_data_get_string(S, name)));
		auto *b = new QPushButton(tr("Browse"));
		h->addWidget(e, 1);
		h->addWidget(b);
		const obs_path_type pt = obs_property_path_type(p);
		const QString filter = QString::fromUtf8(obs_property_path_filter(p)).replace(";;", ";;");
		const QString def = QString::fromUtf8(obs_property_path_default_path(p));
		connect(e, &QLineEdit::editingFinished, this, [this, e, S, name, prop] {
			obs_data_set_string(S, name, e->text().toUtf8().constData());
			commit(prop());
		});
		connect(b, &QPushButton::clicked, this, [this, e, S, name, prop, pt, filter, def, desc] {
			const QString start = e->text().isEmpty() ? def : e->text();
			QString path;
			if (pt == OBS_PATH_DIRECTORY)
				path = QFileDialog::getExistingDirectory(this, desc, start);
			else if (pt == OBS_PATH_FILE_SAVE)
				path = QFileDialog::getSaveFileName(this, desc, start, filter);
			else
				path = QFileDialog::getOpenFileName(this, desc, start, filter);
			if (path.isEmpty())
				return;
			e->setText(path);
			obs_data_set_string(S, name, path.toUtf8().constData());
			commit(prop());
		});
		w = box;
		break;
	}
	case OBS_PROPERTY_LIST: {
		auto *c = new QComboBox;
		c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		c->setMinimumContentsLength(6);
		c->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		const obs_combo_format fmt = obs_property_list_format(p);
		const bool editable = obs_property_list_type(p) == OBS_COMBO_TYPE_EDITABLE;
		c->setEditable(editable);
		const size_t n = obs_property_list_item_count(p);
		int current = -1;
		for (size_t i = 0; i < n; i++) {
			const QString label = QString::fromUtf8(obs_property_list_item_name(p, i));
			QVariant v;
			switch (fmt) {
			case OBS_COMBO_FORMAT_INT:
				v = qlonglong(obs_property_list_item_int(p, i));
				if (obs_data_get_int(S, name) == obs_property_list_item_int(p, i))
					current = int(i);
				break;
			case OBS_COMBO_FORMAT_FLOAT:
				v = obs_property_list_item_float(p, i);
				if (obs_data_get_double(S, name) == obs_property_list_item_float(p, i))
					current = int(i);
				break;
			case OBS_COMBO_FORMAT_BOOL:
				v = obs_property_list_item_bool(p, i);
				if (obs_data_get_bool(S, name) == obs_property_list_item_bool(p, i))
					current = int(i);
				break;
			default:
				v = QString::fromUtf8(obs_property_list_item_string(p, i));
				if (v.toString() == QString::fromUtf8(obs_data_get_string(S, name)))
					current = int(i);
			}
			c->addItem(label, v);
			if (obs_property_list_item_disabled(p, i))
				if (auto *model = qobject_cast<QStandardItemModel *>(c->model()))
					if (QStandardItem *si = model->item(c->count() - 1))
						si->setEnabled(false);
		}
		if (editable && fmt == OBS_COMBO_FORMAT_STRING)
			c->setEditText(QString::fromUtf8(obs_data_get_string(S, name)));
		else if (current >= 0)
			c->setCurrentIndex(current);
		else if (n && fmt == OBS_COMBO_FORMAT_STRING && *obs_data_get_string(S, name)) {
			// A saved choice that isn't available now (an unplugged camera) stays visible.
			c->insertItem(0, tr("%1 (not found)").arg(QString::fromUtf8(obs_data_get_string(S, name))),
				      QString::fromUtf8(obs_data_get_string(S, name)));
			c->setCurrentIndex(0);
		} else
			c->setCurrentIndex(-1);
		auto apply = [this, c, S, name, prop, fmt](int idx) {
			if (idx < 0)
				return;
			const QVariant v = c->itemData(idx);
			switch (fmt) {
			case OBS_COMBO_FORMAT_INT:
				obs_data_set_int(S, name, v.toLongLong());
				break;
			case OBS_COMBO_FORMAT_FLOAT:
				obs_data_set_double(S, name, v.toDouble());
				break;
			case OBS_COMBO_FORMAT_BOOL:
				obs_data_set_bool(S, name, v.toBool());
				break;
			default:
				obs_data_set_string(S, name, v.toString().toUtf8().constData());
			}
			commit(prop());
		};
		connect(c, &QComboBox::activated, this, apply);
		if (editable)
			connect(c, &QComboBox::editTextChanged, this, [this, S, name, prop](const QString &t) {
				obs_data_set_string(S, name, t.toUtf8().constData());
				commit(prop());
			});
		w = c;
		break;
	}
	case OBS_PROPERTY_COLOR:
	case OBS_PROPERTY_COLOR_ALPHA: {
		const bool alpha = obs_property_get_type(p) == OBS_PROPERTY_COLOR_ALPHA;
		auto *b = new QPushButton;
		auto paint = [b](long long v) {
			const QColor q(int(v & 0xff), int((v >> 8) & 0xff), int((v >> 16) & 0xff));
			b->setText(colorText(v));
			b->setStyleSheet(QString("QPushButton { background: %1; color: %2; border-radius: 6px; }")
						 .arg(q.name(), q.lightness() > 140 ? "#111" : "#fff"));
		};
		paint(obs_data_get_int(S, name));
		connect(b, &QPushButton::clicked, this, [this, b, S, name, prop, alpha, paint, desc] {
			const long long v = obs_data_get_int(S, name);
			QColor cur(int(v & 0xff), int((v >> 8) & 0xff), int((v >> 16) & 0xff), alpha ? int((v >> 24) & 0xff) : 255);
			const QColor c = QColorDialog::getColor(cur, this, desc, alpha ? QColorDialog::ShowAlphaChannel : QColorDialog::ColorDialogOptions());
			if (!c.isValid())
				return;
			const long long nv = (alpha ? (long long)c.alpha() << 24 : 0xffLL << 24) | (long long)c.blue() << 16 |
					     (long long)c.green() << 8 | c.red();
			obs_data_set_int(S, name, nv);
			paint(nv);
			commit(prop());
		});
		w = b;
		break;
	}
	case OBS_PROPERTY_BUTTON: {
		auto *b = new QPushButton(desc);
		connect(b, &QPushButton::clicked, this, [this, prop] {
			obs_property_t *pp = prop();
			if (pp && obs_property_button_clicked(pp, m_object))
				scheduleRebuild();
		});
		w = b;
		fullRow = true;
		break;
	}
	case OBS_PROPERTY_FONT: {
		auto *b = new QPushButton;
		auto label = [b, S, name] {
			OBSDataAutoRelease f = obs_data_get_obj(S, name);
			b->setText(QString("%1, %2").arg(QString::fromUtf8(obs_data_get_string(f, "face"))).arg(obs_data_get_int(f, "size")));
		};
		label();
		connect(b, &QPushButton::clicked, this, [this, S, name, prop, label, desc] {
			OBSDataAutoRelease f = obs_data_get_obj(S, name);
			QFont cur(QString::fromUtf8(obs_data_get_string(f, "face")));
			cur.setPointSize(std::max(1, int(obs_data_get_int(f, "size"))));
			const int flags = int(obs_data_get_int(f, "flags"));
			cur.setBold(flags & OBS_FONT_BOLD);
			cur.setItalic(flags & OBS_FONT_ITALIC);
			cur.setUnderline(flags & OBS_FONT_UNDERLINE);
			cur.setStrikeOut(flags & OBS_FONT_STRIKEOUT);
			bool ok = false;
			const QFont nf = QFontDialog::getFont(&ok, cur, this, desc);
			if (!ok)
				return;
			OBSDataAutoRelease out = obs_data_create();
			obs_data_set_string(out, "face", nf.family().toUtf8().constData());
			obs_data_set_string(out, "style", nf.styleName().toUtf8().constData());
			obs_data_set_int(out, "size", nf.pointSize());
			obs_data_set_int(out, "flags", (nf.bold() ? OBS_FONT_BOLD : 0) | (nf.italic() ? OBS_FONT_ITALIC : 0) |
							       (nf.underline() ? OBS_FONT_UNDERLINE : 0) | (nf.strikeOut() ? OBS_FONT_STRIKEOUT : 0));
			obs_data_set_obj(S, name, out);
			label();
			commit(prop());
		});
		w = b;
		break;
	}
	case OBS_PROPERTY_EDITABLE_LIST: {
		auto *box = new QWidget;
		auto *v = new QVBoxLayout(box);
		v->setContentsMargins(0, 0, 0, 0);
		auto *list = new QListWidget;
		list->setFixedHeight(110);
		list->setStyleSheet(QString("QListWidget { border: 1px solid %1; border-radius: 5px; }").arg(Theme::current().line.name()));
		OBSDataArrayAutoRelease arr = obs_data_get_array(S, name);
		for (size_t i = 0, n = obs_data_array_count(arr); i < n; i++) {
			OBSDataAutoRelease it = obs_data_array_item(arr, i);
			list->addItem(QString::fromUtf8(obs_data_get_string(it, "value")));
		}
		auto *h = new QHBoxLayout;
		auto *add = new QPushButton(tr("Add"));
		auto *rm = new QPushButton(tr("Remove"));
		h->addWidget(add);
		h->addWidget(rm);
		h->addStretch(1);
		v->addWidget(list);
		v->addLayout(h);
		auto save = [this, list, S, name, prop] {
			OBSDataArrayAutoRelease out = obs_data_array_create();
			for (int i = 0; i < list->count(); i++) {
				OBSDataAutoRelease it = obs_data_create();
				obs_data_set_string(it, "value", list->item(i)->text().toUtf8().constData());
				obs_data_set_bool(it, "hidden", false);
				obs_data_set_bool(it, "selected", false);
				obs_data_array_push_back(out, it);
			}
			obs_data_set_array(S, name, out);
			commit(prop());
		};
		const obs_editable_list_type lt = obs_property_editable_list_type(p);
		const QString filter = QString::fromUtf8(obs_property_editable_list_filter(p));
		connect(add, &QPushButton::clicked, this, [this, list, save, lt, filter] {
			if (lt == OBS_EDITABLE_LIST_TYPE_STRINGS) {
				auto *it = new QListWidgetItem(tr("New item"), list);
				it->setFlags(it->flags() | Qt::ItemIsEditable);
				list->editItem(it);
				connect(list, &QListWidget::itemChanged, this, save, Qt::UniqueConnection);
			} else {
				const QStringList files = QFileDialog::getOpenFileNames(this, tr("Add files"), QString(), filter);
				list->addItems(files);
			}
			save();
		});
		connect(rm, &QPushButton::clicked, this, [list, save] {
			delete list->currentItem();
			save();
		});
		w = box;
		break;
	}
	case OBS_PROPERTY_GROUP: {
		obs_properties_t *content = obs_property_group_content(p);
		auto *head = new QLabel(desc.toUpper());
		head->setObjectName("ph");
		if (obs_property_group_type(p) == OBS_GROUP_CHECKABLE) {
			auto *c = new QCheckBox(desc);
			c->setChecked(obs_data_get_bool(S, name));
			connect(c, &QCheckBox::toggled, this, [this, S, name, prop](bool v) {
				obs_data_set_bool(S, name, v);
				commit(prop());
			});
			grid->addWidget(c, row++, 0, 1, 2);
			delete head;
		} else {
			grid->addWidget(head, row++, 0, 1, 2);
		}
		for (obs_property_t *q = obs_properties_first(content); q; obs_property_next(&q))
			addProperty(q, grid, row);
		return;
	}
	default:
		return;
	}

	w->setEnabled(enabled);
	if (!tip.isEmpty())
		w->setToolTip(tip);
	if (fullRow) {
		grid->addWidget(w, row++, 0, 1, 2);
	} else {
		auto *l = rowLabel(desc);
		l->setEnabled(enabled);
		if (!tip.isEmpty())
			l->setToolTip(tip);
		grid->addWidget(l, row, 0, Qt::AlignVCenter);
		grid->addWidget(w, row++, 1);
	}
}
