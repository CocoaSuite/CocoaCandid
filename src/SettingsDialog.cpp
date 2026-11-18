#include "SettingsDialog.hpp"
#include "Config.hpp"
#include "Engine.hpp"
#include "Hotkeys.hpp"
#include "Icons.hpp"
#include "Mixer.hpp"
#include "Outputs.hpp"
#include "PropertiesForm.hpp"
#include "Style.hpp"
#include "Widgets.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QDir>
#include <QUrl>
#include <QVBoxLayout>

#ifdef _WIN32
#include <windows.h>
#endif

static QFormLayout *form(QWidget *page)
{
	auto *f = new QFormLayout(page);
	f->setContentsMargins(26, 22, 26, 22);
	f->setHorizontalSpacing(14);
	f->setVerticalSpacing(11);
	f->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	return f;
}

static QLabel *note(const QString &t)
{
	auto *l = new QLabel(t);
	l->setWordWrap(true);
	l->setProperty("mute", true);
	l->setStyleSheet("font-size: 12px");
	return l;
}

static void selectData(QComboBox *c, const QVariant &v)
{
	const int i = c->findData(v);
	if (i >= 0)
		c->setCurrentIndex(i);
}

SettingsDialog::SettingsDialog(QWidget *parent, const QString &page) : QDialog(parent)
{
	setWindowTitle(tr("Settings"));
	resize(900, 620);
	auto *root = new QHBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	auto *navBox = new QWidget;
	navBox->setObjectName("panel");
	navBox->setAttribute(Qt::WA_StyledBackground);
	navBox->setFixedWidth(190);
	auto *nv = new QVBoxLayout(navBox);
	nv->setContentsMargins(8, 14, 8, 14);
	auto *title = new QLabel(tr("Settings"));
	title->setObjectName("title");
	title->setContentsMargins(10, 0, 0, 10);
	nv->addWidget(title);
	m_nav = new QListWidget;
	nv->addWidget(m_nav, 1);
	root->addWidget(navBox);
	root->addWidget(vline());

	auto *right = new QVBoxLayout;
	right->setContentsMargins(0, 0, 0, 0);
	m_pages = new QStackedWidget;
	right->addWidget(m_pages, 1);
	right->addWidget(hline());
	auto *bb = new QDialogButtonBox;
	auto *done = bb->addButton(tr("Done"), QDialogButtonBox::AcceptRole);
	setKind(done, "pri");
	bb->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
	bb->setContentsMargins(18, 10, 18, 12);
	right->addWidget(bb);
	root->addLayout(right, 1);
	connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

	const QList<QPair<QString, QWidget *>> pages = {
		{tr("General"), general()}, {tr("Stream"), stream()},   {tr("Record"), record()},     {tr("Video"), video()},
		{tr("Audio"), audio()},     {tr("Hotkeys"), hotkeys()}, {tr("Advanced"), advanced()},
	};
	for (const auto &[name, w] : pages) {
		auto *scroll = new QScrollArea;
		scroll->setWidgetResizable(true);
		scroll->setWidget(w);
		m_pages->addWidget(scroll);
		auto *it = new QListWidgetItem(name, m_nav);
		it->setSizeHint(QSize(0, 34));
	}
	connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
	int start = 0;
	for (int i = 0; i < pages.size(); i++)
		if (pages[i].first.compare(page, Qt::CaseInsensitive) == 0)
			start = i;
	m_nav->setCurrentRow(start);
}

// General ---------------------------------------------------------------------------------------------

QWidget *SettingsDialog::general()
{
	auto *w = new QWidget;
	auto *f = form(w);
	m_theme = new QComboBox;
	m_theme->addItem(tr("Dark"), "dark");
	m_theme->addItem(tr("Light"), "light");
	selectData(m_theme, CFG.str("general", "theme"));
	m_layout = new QComboBox;
	m_layout->addItem(tr("Classic"), "classic");
	m_layout->addItem(tr("Compact"), "compact");
	m_layout->addItem(tr("Studio (preview, then send live)"), "studio");
	selectData(m_layout, CFG.str("general", "layout"));
	m_icon = new QComboBox;
	m_icon->addItem(Icons::app("garnet"), tr("Garnet"), "garnet");
	m_icon->addItem(Icons::app("midnight"), tr("Midnight"), "midnight");
	selectData(m_icon, CFG.str("general", "icon"));
	m_snap = new QCheckBox(tr("Snap sources to the canvas edges and centre (hold Ctrl to turn off)"));
	m_snap->setChecked(CFG.flag("general", "snap"));
	m_confirmStop = new QCheckBox(tr("Ask before ending a stream"));
	m_confirmStop->setChecked(CFG.flag("general", "confirm_stop_stream"));
	auto *tour = new QPushButton(tr("Show the welcome tour"));
	connect(tour, &QPushButton::clicked, this, [this] {
		emit showTourRequested();
		reject();
	});
	f->addRow(tr("Theme"), m_theme);
	f->addRow(tr("Layout"), m_layout);
	f->addRow(tr("App icon"), m_icon);
	f->addRow(QString(), m_snap);
	f->addRow(QString(), m_confirmStop);
	f->addRow(QString(), tour);
	f->addRow(QString(), note(tr("Compact hides the right panel and the volume sliders; double-click a source for its settings. Studio shows the preview and what's live side by side.")));
	return w;
}

// Stream ----------------------------------------------------------------------------------------------

static const QList<QPair<QString, QString>> kKeyLinks = {
	{"Twitch", "https://dashboard.twitch.tv/settings/stream"},
	{"YouTube - RTMPS", "https://studio.youtube.com/channel/UC/livestreaming"},
	{"YouTube - HLS", "https://studio.youtube.com/channel/UC/livestreaming"},
	{"Kick", "https://kick.com/dashboard/settings/stream"},
	{"Facebook Live", "https://www.facebook.com/live/producer"},
	{"Trovo", "https://studio.trovo.live/stream"},
};

QWidget *SettingsDialog::stream()
{
	auto *w = new QWidget;
	auto *f = form(w);
	m_service = new QComboBox;
	m_service->setEditable(false);
	m_service->setMaxVisibleItems(18);
	// Every service rtmp-services knows, plus a custom server.
	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	if (props) {
		OBSDataAutoRelease s = obs_data_create();
		obs_data_set_bool(s, "show_all", true);
		if (obs_property_t *all = obs_properties_get(props, "show_all"))
			obs_property_modified(all, s);
		if (obs_property_t *p = obs_properties_get(props, "service"))
			for (size_t i = 0, n = obs_property_list_item_count(p); i < n; i++) {
				const QString name = QString::fromUtf8(obs_property_list_item_string(p, i));
				if (!name.isEmpty())
					m_service->addItem(name, name);
			}
		obs_properties_destroy(props);
	}
	m_service->insertItem(0, tr("Custom server (RTMP or SRT)"), "__custom");
	if (obs_service_get_display_name("whip_custom"))
		m_service->insertItem(1, tr("WHIP (WebRTC)"), "__whip");
	m_service->insertSeparator(obs_service_get_display_name("whip_custom") ? 2 : 1);
	const QString type = CFG.str("stream", "type");
	selectData(m_service, type == "rtmp_custom" ? QString("__custom") : type == "whip_custom" ? QString("__whip") : CFG.str("stream", "service"));

	m_server = new QComboBox;
	m_customServer = new QLineEdit(CFG.str("stream", "custom_server"));
	m_customServer->setPlaceholderText("rtmp://example.com/live");
	m_key = new QLineEdit(CFG.str("stream", "key"));
	m_key->setEchoMode(QLineEdit::Password);
	auto *keyRow = new QWidget;
	auto *kh = new QHBoxLayout(keyRow);
	kh->setContentsMargins(0, 0, 0, 0);
	auto *show = new QPushButton(tr("Show"));
	show->setCheckable(true);
	auto *get = new QPushButton(tr("Get key"));
	kh->addWidget(m_key, 1);
	kh->addWidget(show);
	kh->addWidget(get);
	connect(show, &QPushButton::toggled, this, [this, show](bool on) {
		m_key->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
		show->setText(on ? tr("Hide") : tr("Show"));
	});
	connect(get, &QPushButton::clicked, this, [this] {
		for (const auto &[svc, url] : kKeyLinks)
			if (m_service->currentData().toString() == svc)
				QDesktopServices::openUrl(QUrl(url));
	});

	auto refreshService = [this, get] {
		const QString d = m_service->currentData().toString();
		const bool custom = d.startsWith("__");
		m_server->setVisible(!custom);
		m_customServer->setVisible(custom);
		bool link = false;
		for (const auto &kv : kKeyLinks)
			link |= kv.first == d;
		get->setVisible(link);
		fillServers();
	};
	connect(m_service, &QComboBox::currentIndexChanged, this, refreshService);

	m_streamEnc = new QComboBox;
	for (const auto &e : Outputs::videoEncoders())
		m_streamEnc->addItem(e.name + (e.hardware ? tr("  (graphics card)") : QString()), e.id);
	m_streamEnc->insertItem(0, tr("Best available"), QString());
	selectData(m_streamEnc, CFG.str("stream", "encoder"));
	m_bitrate = new QSpinBox;
	m_bitrate->setRange(200, 100000);
	m_bitrate->setSingleStep(500);
	m_bitrate->setSuffix(" kb/s");
	m_bitrate->setValue(int(CFG.num("stream", "bitrate")));
	m_audioKbps = new QComboBox;
	for (int k : {96, 128, 160, 192, 256, 320})
		m_audioKbps->addItem(QString("%1 kb/s").arg(k), k);
	selectData(m_audioKbps, int(CFG.num("stream", "audio_bitrate")));
	m_keyint = new QSpinBox;
	m_keyint->setRange(0, 20);
	m_keyint->setSuffix(" s");
	m_keyint->setSpecialValueText(tr("Automatic"));
	m_keyint->setValue(int(CFG.num("stream", "keyint_sec")));
	m_dyn = new QCheckBox(tr("Lower the bitrate when the connection struggles"));
	m_dyn->setChecked(CFG.flag("stream", "dyn_bitrate"));
	auto *adv = new QPushButton(tr("Encoder settings"));
	connect(adv, &QPushButton::clicked, this, [this] { editEncoder(true); });

	f->addRow(tr("Service"), m_service);
	f->addRow(tr("Server"), m_server);
	f->addRow(tr("Server address"), m_customServer);
	f->addRow(tr("Stream key"), keyRow);
	f->addRow(QString(), note(tr("The key stays on this computer, in your settings file.")));
	f->addRow(panelHeader(tr("Quality")));
	f->addRow(tr("Encoder"), m_streamEnc);
	f->addRow(tr("Video bitrate"), m_bitrate);
	f->addRow(tr("Audio bitrate"), m_audioKbps);
	f->addRow(tr("Keyframes every"), m_keyint);
	f->addRow(QString(), m_dyn);
	f->addRow(QString(), adv);
	f->addRow(QString(), note(tr("6000 kb/s suits 1080p at 60 fps on Twitch; YouTube takes more. Settings apply the next time you go live.")));

	auto labelFor = [f](QWidget *field) { return f->labelForField(field); };
	auto sync = [this, labelFor] {
		const bool custom = m_service->currentData().toString().startsWith("__");
		if (auto *l = labelFor(m_server))
			l->setVisible(!custom);
		if (auto *l = labelFor(m_customServer))
			l->setVisible(custom);
	};
	connect(m_service, &QComboBox::currentIndexChanged, this, sync);
	refreshService();
	sync();
	return w;
}

void SettingsDialog::fillServers()
{
	m_server->clear();
	const QString svc = m_service->currentData().toString();
	if (svc.startsWith("__"))
		return;
	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	if (!props)
		return;
	OBSDataAutoRelease s = obs_data_create();
	obs_data_set_bool(s, "show_all", true);
	obs_data_set_string(s, "service", svc.toUtf8().constData());
	if (obs_property_t *p = obs_properties_get(props, "service"))
		obs_property_modified(p, s);
	if (obs_property_t *p = obs_properties_get(props, "server"))
		for (size_t i = 0, n = obs_property_list_item_count(p); i < n; i++)
			m_server->addItem(QString::fromUtf8(obs_property_list_item_name(p, i)),
					  QString::fromUtf8(obs_property_list_item_string(p, i)));
	obs_properties_destroy(props);
	selectData(m_server, CFG.str("stream", "server"));
}

void SettingsDialog::editEncoder(bool forStream)
{
	const char *sec = forStream ? "stream" : "record";
	QString id = (forStream ? m_streamEnc : m_recEnc)->currentData().toString();
	if (id.isEmpty())
		id = Outputs::bestEncoder();
	if (id.isEmpty())
		return;
	obs_data_t *section = CFG.section(sec);
	OBSDataAutoRelease base = Outputs::videoSettings(id, forStream);
	OBSDataAutoRelease custom = obs_data_get_obj(section, "encoder_settings");
	if (custom && strcmp(obs_data_get_string(section, "encoder_settings_for"), id.toUtf8().constData()) == 0)
		obs_data_apply(base, custom);

	QDialog d(this);
	d.setWindowTitle(tr("Encoder settings"));
	d.resize(480, 520);
	auto *v = new QVBoxLayout(&d);
	v->setContentsMargins(16, 16, 16, 12);
	auto *head = new QLabel(QString::fromUtf8(obs_encoder_get_display_name(id.toUtf8().constData())));
	head->setObjectName("title");
	v->addWidget(head);
	auto *scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	const QByteArray idb = id.toUtf8();
	auto *pf = new PropertiesForm(base, [idb] { return obs_get_encoder_properties(idb.constData()); }, nullptr);
	scroll->setWidget(pf);
	v->addWidget(scroll, 1);
	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
	v->addWidget(bb);
	connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
	connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
	connect(bb->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &d, [&d, section] {
		obs_data_erase(section, "encoder_settings");
		obs_data_erase(section, "encoder_settings_for");
		d.reject();
	});
	if (d.exec() == QDialog::Accepted) {
		obs_data_set_obj(section, "encoder_settings", base);
		obs_data_set_string(section, "encoder_settings_for", idb.constData());
		CFG.save();
	}
}

// Record ----------------------------------------------------------------------------------------------

QWidget *SettingsDialog::record()
{
	auto *w = new QWidget;
	auto *f = form(w);
	m_path = new QLineEdit(CFG.str("record", "path"));
	auto *row = new QWidget;
	auto *h = new QHBoxLayout(row);
	h->setContentsMargins(0, 0, 0, 0);
	auto *browse = new QPushButton(tr("Browse"));
	auto *open = new QPushButton(tr("Open"));
	h->addWidget(m_path, 1);
	h->addWidget(browse);
	h->addWidget(open);
	connect(browse, &QPushButton::clicked, this, [this] {
		const QString d = QFileDialog::getExistingDirectory(this, tr("Save recordings in"), m_path->text());
		if (!d.isEmpty())
			m_path->setText(QDir::toNativeSeparators(d));
	});
	connect(open, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_path->text())); });

	m_format = new QComboBox;
	m_format->addItem(tr("MP4, safe if the computer crashes"), "mp4");
	m_format->addItem(tr("MKV"), "mkv");
	m_format->addItem(tr("MOV"), "mov");
	m_format->addItem(tr("MPEG-TS"), "ts");
	m_format->addItem(tr("FLV"), "flv");
	selectData(m_format, CFG.str("record", "format"));
	m_quality = new QComboBox;
	m_quality->addItem(tr("Same as the stream"), "same");
	m_quality->addItem(tr("High, medium files"), "high");
	m_quality->addItem(tr("Indistinguishable, large files"), "indistinguishable");
	m_quality->addItem(tr("Lossless, very large files"), "lossless");
	selectData(m_quality, CFG.str("record", "quality"));
	m_recEnc = new QComboBox;
	for (const auto &e : Outputs::videoEncoders())
		m_recEnc->addItem(e.name + (e.hardware ? tr("  (graphics card)") : QString()), e.id);
	m_recEnc->insertItem(0, tr("Best available"), QString());
	selectData(m_recEnc, CFG.str("record", "encoder"));
	auto *adv = new QPushButton(tr("Encoder settings"));
	connect(adv, &QPushButton::clicked, this, [this] { editEncoder(false); });

	m_replay = new QCheckBox(tr("Keep a replay buffer while CocoaCandid is open"));
	m_replay->setChecked(CFG.flag("record", "replay"));
	m_replaySec = new QSpinBox;
	m_replaySec->setRange(5, 21600);
	m_replaySec->setSuffix(tr(" seconds"));
	m_replaySec->setValue(int(CFG.num("record", "replay_sec")));

	f->addRow(tr("Save to"), row);
	f->addRow(tr("Format"), m_format);
	f->addRow(tr("Quality"), m_quality);
	f->addRow(tr("Encoder"), m_recEnc);
	f->addRow(QString(), adv);
	f->addRow(panelHeader(tr("Replay buffer")));
	f->addRow(QString(), m_replay);
	f->addRow(tr("Keep the last"), m_replaySec);
	f->addRow(QString(), note(tr("The replay button in the top bar (or its hotkey) saves the last stretch of the canvas as a file, without recording all the time.")));
	return w;
}

// Video -----------------------------------------------------------------------------------------------

QWidget *SettingsDialog::video()
{
	auto *w = new QWidget;
	auto *f = form(w);
	auto spin = [](int v) {
		auto *s = new QSpinBox;
		s->setRange(16, 16384);
		s->setSingleStep(2);
		s->setValue(v);
		return s;
	};
	m_baseW = spin(int(CFG.num("video", "base_w")));
	m_baseH = spin(int(CFG.num("video", "base_h")));
	m_outW = spin(int(CFG.num("video", "out_w")));
	m_outH = spin(int(CFG.num("video", "out_h")));
	auto pair = [](QSpinBox *a, QSpinBox *b) {
		auto *r = new QWidget;
		auto *h = new QHBoxLayout(r);
		h->setContentsMargins(0, 0, 0, 0);
		h->addWidget(a, 1);
		h->addWidget(new QLabel("×"));
		h->addWidget(b, 1);
		return r;
	};
	auto *presets = new QComboBox;
	presets->addItem(tr("Presets"));
	const QList<QPair<QString, QSize>> sizes = {
		{"3840 × 2160 (4K)", {3840, 2160}}, {"2560 × 1440", {2560, 1440}},          {"1920 × 1080", {1920, 1080}},
		{"1280 × 720", {1280, 720}},        {tr("1080 × 1920 (vertical)"), {1080, 1920}}, {tr("1080 × 1080 (square)"), {1080, 1080}},
	};
	for (const auto &s : sizes)
		presets->addItem(s.first, s.second);
	connect(presets, &QComboBox::activated, this, [this, presets](int i) {
		const QSize s = presets->itemData(i).toSize();
		if (s.isEmpty())
			return;
		m_baseW->setValue(s.width());
		m_baseH->setValue(s.height());
		m_outW->setValue(s.width());
		m_outH->setValue(s.height());
		presets->setCurrentIndex(0);
	});
	m_fps = new QComboBox;
	for (auto [label, num, den] : {std::tuple{"24", 24, 1}, {"25", 25, 1}, {"29.97", 30000, 1001}, {"30", 30, 1},
				       {"48", 48, 1}, {"50", 50, 1}, {"59.94", 60000, 1001}, {"60", 60, 1}, {"120", 120, 1}})
		m_fps->addItem(QString("%1 fps").arg(label), QString("%1/%2").arg(num).arg(den));
	selectData(m_fps, QString("%1/%2").arg(CFG.num("video", "fps_num")).arg(CFG.num("video", "fps_den")));
	m_scale = new QComboBox;
	m_scale->addItem(tr("Lanczos (sharpest)"), "lanczos");
	m_scale->addItem(tr("Bicubic"), "bicubic");
	m_scale->addItem(tr("Area"), "area");
	m_scale->addItem(tr("Bilinear (fastest)"), "bilinear");
	selectData(m_scale, CFG.str("video", "scale"));
	m_space = new QComboBox;
	m_space->addItem("Rec. 709", "709");
	m_space->addItem("Rec. 601", "601");
	selectData(m_space, CFG.str("video", "color_space"));
	m_range = new QComboBox;
	m_range->addItem(tr("Limited (recommended)"), "partial");
	m_range->addItem(tr("Full"), "full");
	selectData(m_range, CFG.str("video", "range"));

	f->addRow(tr("Canvas"), pair(m_baseW, m_baseH));
	f->addRow(QString(), presets);
	f->addRow(tr("Output (scaled)"), pair(m_outW, m_outH));
	f->addRow(tr("Frame rate"), m_fps);
	f->addRow(tr("Downscale filter"), m_scale);
	f->addRow(panelHeader(tr("Colour")));
	f->addRow(tr("Colour space"), m_space);
	f->addRow(tr("Range"), m_range);
	f->addRow(QString(), note(tr("The canvas is the space you arrange sources on. The output is the size recordings and streams are made at. Crop canvas to feed, under the canvas, sets the canvas from a source in one click.")));
	return w;
}

// Audio -----------------------------------------------------------------------------------------------

QWidget *SettingsDialog::audio()
{
	auto *w = new QWidget;
	auto *f = form(w);
	m_rate = new QComboBox;
	m_rate->addItem("48 kHz", 48000);
	m_rate->addItem("44.1 kHz", 44100);
	selectData(m_rate, int(CFG.num("audio", "sample_rate")));
	m_channels = new QComboBox;
	m_channels->addItem(tr("Stereo"), "stereo");
	m_channels->addItem(tr("Mono"), "mono");
	selectData(m_channels, CFG.str("audio", "speakers"));
	f->addRow(tr("Sample rate"), m_rate);
	f->addRow(tr("Channels"), m_channels);
	f->addRow(QString(), note(tr("These two apply after restarting CocoaCandid.")));
	f->addRow(panelHeader(tr("Devices")));
	for (obs_source_t *s : {ENGINE.desktopAudio(), ENGINE.micAudio()}) {
		if (!s)
			continue;
		auto *b = new QPushButton(tr("Choose device"));
		OBSSource keep(s);
		connect(b, &QPushButton::clicked, this, [this, keep] { showSourceDialog(keep, false, this); });
		f->addRow(QString::fromUtf8(obs_source_get_name(s)), b);
	}
	return w;
}

// Hotkeys ---------------------------------------------------------------------------------------------

QWidget *SettingsDialog::hotkeys()
{
	auto *w = new QWidget;
	auto *f = form(w);
	f->addRow(note(tr("Hotkeys work even while another app, like a game, is in front. Click a box and press the keys; Backspace clears it.")));
	for (const auto &a : Hotkeys::get().actions()) {
		auto *e = new QKeySequenceEdit(Hotkeys::get().binding(a.id));
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
		e->setMaximumSequenceLength(1);
#endif
		auto *row = new QWidget;
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		auto *clear = iconButton("x", tr("Clear"));
		connect(clear, &QPushButton::clicked, e, &QKeySequenceEdit::clear);
		h->addWidget(e, 1);
		h->addWidget(clear);
		f->addRow(a.label, row);
		m_keys.append({a.id, e});
	}
	return w;
}

// Advanced --------------------------------------------------------------------------------------------

QWidget *SettingsDialog::advanced()
{
	auto *w = new QWidget;
	auto *f = form(w);
	m_priority = new QComboBox;
	m_priority->addItem(tr("Normal"), "normal");
	m_priority->addItem(tr("Above normal (fewer dropped frames in games)"), "above");
	m_priority->addItem(tr("High"), "high");
	selectData(m_priority, CFG.str("general", "priority").isEmpty() ? QString("normal") : CFG.str("general", "priority"));
	auto *cfg = new QPushButton(tr("Open settings folder"));
	connect(cfg, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(Config::dir())); });
	auto *logs = new QPushButton(tr("Open log folder"));
	connect(logs, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(Config::file("logs"))); });
#ifdef _WIN32
	f->addRow(tr("Process priority"), m_priority);
#else
	m_priority->hide();
#endif
	f->addRow(QString(), cfg);
	f->addRow(QString(), logs);
	f->addRow(QString(), note(tr("CocoaCandid is built on libobs, the engine of OBS Studio, and is free software under the GNU GPL version 2 or later.")));
	return w;
}

void SettingsDialog::accept()
{
	// General
	const bool look = CFG.str("general", "theme") != m_theme->currentData().toString() ||
			  CFG.str("general", "layout") != m_layout->currentData().toString() ||
			  CFG.str("general", "icon") != m_icon->currentData().toString();
	CFG.setStr("general", "theme", m_theme->currentData().toString());
	CFG.setStr("general", "layout", m_layout->currentData().toString());
	CFG.setStr("general", "icon", m_icon->currentData().toString());
	CFG.setFlag("general", "snap", m_snap->isChecked());
	CFG.setFlag("general", "confirm_stop_stream", m_confirmStop->isChecked());
	CFG.setStr("general", "priority", m_priority->currentData().toString());
#ifdef _WIN32
	const QString pr = m_priority->currentData().toString();
	SetPriorityClass(GetCurrentProcess(), pr == "high" ? HIGH_PRIORITY_CLASS : pr == "above" ? ABOVE_NORMAL_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
#endif

	// Stream
	const QString svc = m_service->currentData().toString();
	CFG.setStr("stream", "type", svc == "__custom" ? "rtmp_custom" : svc == "__whip" ? "whip_custom" : "rtmp_common");
	if (!svc.startsWith("__"))
		CFG.setStr("stream", "service", svc);
	if (m_server->currentIndex() >= 0)
		CFG.setStr("stream", "server", m_server->currentData().toString());
	CFG.setStr("stream", "custom_server", m_customServer->text().trimmed());
	CFG.setStr("stream", "key", m_key->text().trimmed());
	CFG.setStr("stream", "encoder", m_streamEnc->currentData().toString());
	CFG.setNum("stream", "bitrate", m_bitrate->value());
	CFG.setNum("stream", "audio_bitrate", m_audioKbps->currentData().toInt());
	CFG.setNum("stream", "keyint_sec", m_keyint->value());
	CFG.setFlag("stream", "dyn_bitrate", m_dyn->isChecked());

	// Record
	CFG.setStr("record", "path", m_path->text().trimmed());
	CFG.setStr("record", "format", m_format->currentData().toString());
	CFG.setStr("record", "quality", m_quality->currentData().toString());
	CFG.setStr("record", "encoder", m_recEnc->currentData().toString());
	const bool replayWas = CFG.flag("record", "replay");
	CFG.setFlag("record", "replay", m_replay->isChecked());
	CFG.setNum("record", "replay_sec", m_replaySec->value());

	// Audio (on restart)
	CFG.setNum("audio", "sample_rate", m_rate->currentData().toInt());
	CFG.setStr("audio", "speakers", m_channels->currentData().toString());

	// Hotkeys
	for (const auto &[id, e] : m_keys)
		Hotkeys::get().setBinding(id, e->keySequence());
	Hotkeys::get().applyAll();

	// Video
	const QStringList fps = m_fps->currentData().toString().split('/');
	const bool videoChanged = m_baseW->value() != CFG.num("video", "base_w") || m_baseH->value() != CFG.num("video", "base_h") ||
				  m_outW->value() != CFG.num("video", "out_w") || m_outH->value() != CFG.num("video", "out_h") ||
				  fps.value(0).toLongLong() != CFG.num("video", "fps_num") || fps.value(1).toLongLong() != CFG.num("video", "fps_den") ||
				  m_scale->currentData().toString() != CFG.str("video", "scale") ||
				  m_space->currentData().toString() != CFG.str("video", "color_space") ||
				  m_range->currentData().toString() != CFG.str("video", "range");
	if (videoChanged) {
		if (OUTPUTS.anyActive()) {
			QMessageBox::information(this, tr("Video"), tr("Stop recording, streaming, the replay buffer and the virtual camera to change video settings."));
			m_nav->setCurrentRow(3);
			return;
		}
		CFG.setNum("video", "base_w", m_baseW->value() & ~1);
		CFG.setNum("video", "base_h", m_baseH->value() & ~1);
		CFG.setNum("video", "out_w", m_outW->value() & ~1);
		CFG.setNum("video", "out_h", m_outH->value() & ~1);
		CFG.setNum("video", "fps_num", fps.value(0).toLongLong());
		CFG.setNum("video", "fps_den", fps.value(1).toLongLong());
		CFG.setStr("video", "scale", m_scale->currentData().toString());
		CFG.setStr("video", "color_space", m_space->currentData().toString());
		CFG.setStr("video", "range", m_range->currentData().toString());
		QString err;
		if (!ENGINE.resetVideo(&err))
			QMessageBox::warning(this, tr("Video"), err);
	}
	CFG.save();

	if (m_replay->isChecked() != replayWas) {
		QString err;
		if (m_replay->isChecked())
			OUTPUTS.startReplay(&err);
		else
			OUTPUTS.stopReplay();
	} else if (m_replay->isChecked() && OUTPUTS.replayActive()) {
		// Restart so a new length takes effect.
		OUTPUTS.stopReplay();
		QTimer::singleShot(800, &OUTPUTS, [] {
			QString err;
			OUTPUTS.startReplay(&err);
		});
	}
	if (look)
		emit lookChanged();
	QDialog::accept();
}
