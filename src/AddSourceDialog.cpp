#include "AddSourceDialog.hpp"
#include "Engine.hpp"
#include "Icons.hpp"
#include "PropertiesForm.hpp"
#include "Style.hpp"
#include "Widgets.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

static int64_t g_draftItem = -1;

QVector<AddSourceDialog::Kind> AddSourceDialog::kinds()
{
	QVector<Kind> all = {
		{"screen", "screen", tr("Screen"), tr("A whole display"), {"monitor_capture", "xshm_input", "pipewire-desktop-capture-source", "screen_capture"}, true},
		{"window", "window", tr("App window"), tr("Follows the window"), {"window_capture", "xcomposite_input", "pipewire-window-capture-source"}, true},
		{"area", "area", tr("Area"), tr("Draw a rectangle"), {"monitor_capture", "xshm_input"}, false},
		{"game", "game", tr("Game"), tr("Full-screen games"), {"game_capture"}, true},
		{"camera", "camera", tr("Camera"), tr("Webcam or capture card"), {"dshow_input", "v4l2_input", "av_capture_input"}, false},
		{"mic", "mic", tr("Microphone"), tr("Any input device"), {"wasapi_input_capture", "pulse_input_capture", "coreaudio_input_capture"}, false},
		{"desktop", "speaker", tr("Computer sound"), tr("An output device"), {"wasapi_output_capture", "pulse_output_capture", "coreaudio_output_capture"}, false},
		{"appaudio", "appaudio", tr("App audio"), tr("Sound from one app"), {"wasapi_process_output_capture"}, false},
		{"image", "image", tr("Image"), tr("PNG, JPG, GIF and more"), {"image_source"}, false},
		{"slideshow", "image", tr("Slideshow"), tr("Several images in turn"), {"slideshow"}, false},
		{"media", "media", tr("Video or sound file"), tr("Plays a file"), {"ffmpeg_source"}, false},
		{"text", "text", tr("Text"), tr("Titles and captions"), {"text_gdiplus", "text_ft2_source"}, false},
		{"web", "web", tr("Web page"), tr("Alerts and chat widgets"), {"browser_source"}, false},
		{"color", "color", tr("Colour"), tr("A solid block"), {"color_source"}, false},
		{"scene", "scene", tr("Scene"), tr("Nest another scene"), {}, false},
		{"existing", "copy", tr("Copy existing"), tr("Reuse a source"), {}, false},
	};
	QVector<Kind> out;
	for (auto &k : all) {
		if (k.types.isEmpty()) {
			out.append(k);
			continue;
		}
		QStringList avail;
		for (const auto &t : k.types) {
			const QString id = Engine::firstType({t.toUtf8().constData()});
			if (!id.isEmpty())
				avail.append(id);
		}
		if (avail.isEmpty())
			continue;
		k.types = avail;
		out.append(k);
	}
	return out;
}

AddSourceDialog::AddSourceDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(tr("Add source"));
	resize(720, 680);
	m_kinds = kinds();

	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(18, 16, 18, 14);
	v->setSpacing(12);
	auto *head = new QHBoxLayout;
	auto *title = new QLabel(tr("Add source"));
	title->setObjectName("title");
	auto *to = new QLabel(tr("to %1").arg(QString::fromUtf8(obs_source_get_name(ENGINE.editScene()))));
	to->setProperty("mute", true);
	head->addWidget(title);
	head->addStretch(1);
	head->addWidget(to);
	v->addLayout(head);

	auto *grid = new QGridLayout;
	grid->setSpacing(8);
	for (int i = 0; i < m_kinds.size(); i++) {
		const Kind &k = m_kinds[i];
		auto *b = new QPushButton;
		setKind(b, "tile");
		b->setCheckable(true);
		b->setIcon(Icons::get(k.icon));
		b->setIconSize(QSize(20, 20));
		b->setText(QString("%1\n%2").arg(k.title, k.hint));
		b->setToolTip(k.hint);
		b->setMinimumHeight(60);
		b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		connect(b, &QPushButton::clicked, this, [this, i] { pick(i); });
		grid->addWidget(b, i / 4, i % 4);
		m_tiles.append(b);
	}
	for (int c = 0; c < 4; c++)
		grid->setColumnStretch(c, 1);
	v->addLayout(grid);

	v->addWidget(hline());
	m_detailTitle = panelHeader(tr("Pick what to add"));
	v->addWidget(m_detailTitle);

	auto *nameRow = new QHBoxLayout;
	auto *nl = new QLabel(tr("Name"));
	nl->setProperty("mute", true);
	nl->setFixedWidth(92);
	m_name = new QLineEdit;
	m_existing = new QComboBox;
	m_existing->hide();
	nameRow->addWidget(nl);
	nameRow->addWidget(m_name, 1);
	nameRow->addWidget(m_existing, 1);
	v->addLayout(nameRow);

	m_formArea = new QScrollArea;
	m_formArea->setWidgetResizable(true);
	m_formArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	v->addWidget(m_formArea, 1);

	auto *foot = new QHBoxLayout;
	m_fit = new QCheckBox(tr("Fit to the canvas"));
	m_fit->setChecked(true);
	m_cropCanvas = new QCheckBox(tr("Resize the canvas to this source"));
	foot->addWidget(m_fit);
	foot->addSpacing(12);
	foot->addWidget(m_cropCanvas);
	foot->addStretch(1);
	auto *cancel = new QPushButton(tr("Cancel"));
	m_add = new QPushButton(tr("Add"));
	setKind(m_add, "pri");
	m_add->setDefault(true);
	m_add->setEnabled(false);
	foot->addWidget(cancel);
	foot->addWidget(m_add);
	v->addLayout(foot);

	connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
	connect(m_add, &QPushButton::clicked, this, &AddSourceDialog::accept);
	connect(m_fit, &QCheckBox::toggled, this, [](bool on) {
		obs_scene_t *sc = ENGINE.editSceneObj();
		obs_sceneitem_t *it = sc && g_draftItem >= 0 ? obs_scene_find_sceneitem_by_id(sc, g_draftItem) : nullptr;
		if (!it)
			return;
		obs_transform_info info;
		candid_get_info(it, &info);
		info.bounds_type = on ? OBS_BOUNDS_SCALE_INNER : OBS_BOUNDS_NONE;
		info.bounds_alignment = OBS_ALIGN_CENTER;
		const QSize c = ENGINE.canvasSize();
		vec2_set(&info.bounds, float(c.width()), float(c.height()));
		vec2_set(&info.pos, 0, 0);
		candid_set_info(it, &info);
	});
	m_formArea->setWidget(new QWidget);
}

AddSourceDialog::~AddSourceDialog()
{
	discardDraft();
}

void AddSourceDialog::discardDraft()
{
	if (m_draft) {
		obs_scene_t *sc = ENGINE.editSceneObj();
		if (obs_sceneitem_t *it = sc && g_draftItem >= 0 ? obs_scene_find_sceneitem_by_id(sc, g_draftItem) : nullptr)
			obs_sceneitem_remove(it);
		obs_source_remove(m_draft);
		m_draft = nullptr;
	}
	g_draftItem = -1;
}

void AddSourceDialog::pick(int index)
{
	for (int i = 0; i < m_tiles.size(); i++)
		m_tiles[i]->setChecked(i == index);
	discardDraft();
	m_current = index;
	const Kind &k = m_kinds[index];

	if (k.key == "area") {
		m_area = true;
		QDialog::accept();
		return;
	}

	m_detailTitle->setText(k.title.toUpper());
	const bool existing = k.key == "existing" || k.key == "scene";
	m_name->setVisible(!existing);
	m_existing->setVisible(existing);
	m_fit->setVisible(!existing && k.fit);
	m_fit->setChecked(k.fit);
	m_cropCanvas->setVisible(k.key == "screen" || k.key == "window" || k.key == "game" || k.key == "camera");
	m_cropCanvas->setChecked(false);
	m_add->setEnabled(true);

	if (existing) {
		m_existing->clear();
		struct Ctx {
			QComboBox *c;
			bool scenes;
		} ctx{m_existing, k.key == "scene"};
		auto add = [](void *p, obs_source_t *s) {
			auto *c = static_cast<Ctx *>(p);
			if (s == ENGINE.editScene() || obs_source_is_group(s))
				return true;
			if (obs_source_is_scene(s) != c->scenes)
				return true;
			if (!c->scenes && (s == ENGINE.desktopAudio() || s == ENGINE.micAudio()))
				return true;
			c->c->addItem(QString::fromUtf8(obs_source_get_name(s)), QString::fromUtf8(obs_source_get_uuid(s)));
			return true;
		};
		if (k.key == "scene")
			obs_enum_scenes(add, &ctx);
		else
			obs_enum_sources(add, &ctx);
		m_add->setEnabled(m_existing->count() > 0);
		auto *note = new QLabel(k.key == "scene" ? tr("The scene shows inside this one, with all its sources. Changes to it appear in both.")
							 : tr("The same source appears in both scenes. Its settings and filters are shared."));
		note->setWordWrap(true);
		note->setProperty("mute", true);
		m_formArea->setWidget(note);
		return;
	}

	const QString type = k.types.value(0);
	const QString name = ENGINE.uniqueName(k.title);
	m_name->setText(name);
	OBSSourceAutoRelease src = obs_source_create(type.toUtf8().constData(), name.toUtf8().constData(), nullptr, nullptr);
	if (!src) {
		m_formArea->setWidget(new QLabel(tr("This kind of source isn't available.")));
		m_add->setEnabled(false);
		return;
	}
	m_draft = src.Get();
	// It goes on the canvas straight away, so its settings can be judged by the real picture.
	if (obs_source_get_output_flags(src) & OBS_SOURCE_VIDEO) {
		if (obs_sceneitem_t *it = obs_scene_add(ENGINE.editSceneObj(), src)) {
			g_draftItem = obs_sceneitem_get_id(it);
			if (k.fit) {
				const QSize c = ENGINE.canvasSize();
				vec2 b;
				vec2_set(&b, float(c.width()), float(c.height()));
				obs_sceneitem_set_bounds_type(it, OBS_BOUNDS_SCALE_INNER);
				obs_sceneitem_set_bounds_alignment(it, OBS_ALIGN_CENTER);
				obs_sceneitem_set_bounds(it, &b);
			}
			ENGINE.select(it);
		}
	}
	auto *form = PropertiesForm::forSource(src);
	m_formArea->setWidget(form);
}

void AddSourceDialog::accept()
{
	if (m_current < 0)
		return;
	const Kind &k = m_kinds[m_current];
	obs_scene_t *scene = ENGINE.editSceneObj();
	if (k.key == "existing" || k.key == "scene") {
		OBSSourceAutoRelease s = obs_get_source_by_uuid(m_existing->currentData().toString().toUtf8().constData());
		if (s)
			ENGINE.addToScene(s, false);
		QDialog::accept();
		return;
	}
	if (!m_draft)
		return;
	const QString want = m_name->text().trimmed();
	if (!want.isEmpty() && want != QString::fromUtf8(obs_source_get_name(m_draft)) && !ENGINE.rename(m_draft, want)) {
		QMessageBox::information(this, tr("Add source"), tr("That name is already used. Pick another."));
		return;
	}
	obs_sceneitem_t *item = g_draftItem >= 0 ? obs_scene_find_sceneitem_by_id(scene, g_draftItem) : nullptr;
	if (!item)
		item = obs_scene_add(scene, m_draft); // sound-only sources
	m_draft = nullptr;
	g_draftItem = -1;
	ENGINE.select(item);
	emit ENGINE.itemsChanged();
	emit ENGINE.audioSourcesChanged();
	ENGINE.save();

	if (m_cropCanvas->isChecked() && item) {
		// The capture may need a moment to learn its size.
		const int64_t id = obs_sceneitem_get_id(item);
		auto tries = std::make_shared<int>(0);
		auto attempt = std::make_shared<std::function<void()>>();
		QWidget *owner = parentWidget();
		*attempt = [id, tries, attempt, owner] {
			obs_sceneitem_t *it = obs_scene_find_sceneitem_by_id(ENGINE.editSceneObj(), id);
			if (!it)
				return;
			obs_source_t *s = obs_sceneitem_get_source(it);
			if (obs_source_get_width(s) == 0 && ++*tries < 12) {
				QTimer::singleShot(250, &ENGINE, *attempt);
				return;
			}
			obs_sceneitem_set_bounds_type(it, OBS_BOUNDS_NONE);
			vec2 one, zero;
			vec2_set(&one, 1, 1);
			vec2_set(&zero, 0, 0);
			obs_sceneitem_set_scale(it, &one);
			obs_sceneitem_set_pos(it, &zero);
			QString err;
			if (!ENGINE.cropCanvasTo(it, &err))
				QMessageBox::information(owner, tr("Resize the canvas"), err);
		};
		(*attempt)();
	}
	QDialog::accept();
}

void AddSourceDialog::reject()
{
	discardDraft();
	QDialog::reject();
}
