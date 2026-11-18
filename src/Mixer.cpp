#include "Mixer.hpp"
#include "Engine.hpp"
#include "Icons.hpp"
#include "Inspector.hpp"
#include "PropertiesForm.hpp"
#include "Style.hpp"
#include "Widgets.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

void showSourceDialog(obs_source_t *source, bool filters, QWidget *parent)
{
	auto *d = new QDialog(parent);
	d->setAttribute(Qt::WA_DeleteOnClose);
	d->setWindowTitle(QString("%1 %2").arg(QString::fromUtf8(obs_source_get_name(source)), filters ? QObject::tr("filters") : QObject::tr("settings")));
	d->resize(460, filters ? 560 : 420);
	auto *v = new QVBoxLayout(d);
	v->setContentsMargins(16, 16, 16, 12);
	auto *scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	QWidget *body = filters ? Inspector::filtersEditor(source, nullptr) : PropertiesForm::forSource(source);
	scroll->setWidget(body);
	v->addWidget(scroll, 1);
	auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
	QObject::connect(bb, &QDialogButtonBox::rejected, d, &QDialog::close);
	QObject::connect(d, &QDialog::finished, [] { ENGINE.save(); });
	v->addWidget(bb);
	d->show();
}

class AudioStrip : public QWidget {
public:
	AudioStrip(obs_source_t *src, QWidget *parent) : QWidget(parent), m_source(src)
	{
		auto *g = new QGridLayout(this);
		g->setContentsMargins(0, 0, 0, 0);
		g->setHorizontalSpacing(12);
		g->setVerticalSpacing(4);
		m_name = new ElidedLabel;
		m_name->setFixedWidth(128);
		m_meter = new Meter;
		m_fader = new QSlider(Qt::Horizontal);
		m_fader->setRange(0, 1000);
		m_fader->setAccessibleName(tr("Volume"));
		m_db = new QLabel;
		m_db->setProperty("mute", true);
		m_db->setFixedWidth(62);
		m_db->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_db->setStyleSheet("font-size: 11.5px");
		m_mute = iconButton("speaker", tr("Mute"), this);
		auto *more = iconButton("more", tr("Options"), this);

		auto *col = new QVBoxLayout;
		col->setSpacing(5);
		col->addWidget(m_meter);
		col->addWidget(m_fader);
		g->addWidget(m_name, 0, 0);
		g->addLayout(col, 0, 1);
		g->addWidget(m_db, 0, 2);
		g->addWidget(m_mute, 0, 3);
		g->addWidget(more, 0, 4);
		g->setColumnStretch(1, 1);

		m_vol = obs_volmeter_create(OBS_FADER_LOG);
		obs_volmeter_attach_source(m_vol, src);
		obs_volmeter_add_callback(m_vol, onLevels, this);
		m_fad = obs_fader_create(OBS_FADER_LOG);
		obs_fader_attach_source(m_fad, src);
		obs_fader_add_callback(m_fad, onFader, this);
		signal_handler_connect(obs_source_get_signal_handler(src), "mute", onMute, this);
		signal_handler_connect(obs_source_get_signal_handler(src), "rename", onRename, this);

		m_fader->setValue(int(std::lround(obs_fader_get_deflection(m_fad) * 1000)));
		connect(m_fader, &QSlider::valueChanged, this, [this](int v) {
			obs_fader_set_deflection(m_fad, float(v) / 1000.0f);
			showDb();
		});
		connect(m_fader, &QSlider::sliderReleased, this, [] { ENGINE.save(); });
		connect(m_mute, &QPushButton::clicked, this, [this] {
			obs_source_set_muted(m_source, !obs_source_muted(m_source));
			ENGINE.save();
		});
		connect(more, &QPushButton::clicked, this, [this, more] { menu(more); });
		refreshName();
		refreshMute();
		showDb();
	}
	~AudioStrip() override
	{
		signal_handler_disconnect(obs_source_get_signal_handler(m_source), "mute", onMute, this);
		signal_handler_disconnect(obs_source_get_signal_handler(m_source), "rename", onRename, this);
		obs_volmeter_remove_callback(m_vol, onLevels, this);
		obs_fader_remove_callback(m_fad, onFader, this);
		obs_volmeter_destroy(m_vol);
		obs_fader_destroy(m_fad);
	}
	obs_source_t *source() const { return m_source; }
	void setCompact(bool c) { m_fader->setVisible(!c); }

private:
	static void onLevels(void *p, const float mag[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS], const float[MAX_AUDIO_CHANNELS])
	{
		auto *self = static_cast<AudioStrip *>(p);
		float m = -60, k = -60;
		const int ch = std::max(1, int(obs_volmeter_get_nr_channels(self->m_vol)));
		for (int i = 0; i < ch && i < MAX_AUDIO_CHANNELS; i++) {
			m = std::max(m, mag[i]);
			k = std::max(k, peak[i]);
		}
		self->m_meter->setLevels(m, k);
	}
	static void onFader(void *p, float)
	{
		auto *self = static_cast<AudioStrip *>(p);
		QMetaObject::invokeMethod(self, [self] {
			const QSignalBlocker b(self->m_fader);
			self->m_fader->setValue(int(std::lround(obs_fader_get_deflection(self->m_fad) * 1000)));
			self->showDb();
		}, Qt::QueuedConnection);
	}
	static void onMute(void *p, calldata_t *)
	{
		auto *self = static_cast<AudioStrip *>(p);
		QMetaObject::invokeMethod(self, [self] { self->refreshMute(); }, Qt::QueuedConnection);
	}
	static void onRename(void *p, calldata_t *)
	{
		auto *self = static_cast<AudioStrip *>(p);
		QMetaObject::invokeMethod(self, [self] { self->refreshName(); }, Qt::QueuedConnection);
	}
	void refreshName() { m_name->setFullText(QString::fromUtf8(obs_source_get_name(m_source))); }
	void refreshMute()
	{
		const bool muted = obs_source_muted(m_source);
		m_mute->setIcon(muted ? Icons::tinted("muted", Theme::current().bad) : Icons::get("speaker"));
		m_mute->setToolTip(muted ? tr("Unmute") : tr("Mute"));
		m_meter->setMuted(muted);
		showDb();
	}
	void showDb()
	{
		if (obs_source_muted(m_source)) {
			m_db->setText(tr("Muted"));
			return;
		}
		const float db = obs_fader_get_db(m_fad);
		m_db->setText(std::isinf(db) || db < -95 ? QString("-inf dB") : QString("%1 dB").arg(db, 0, 'f', 1));
	}
	void menu(QWidget *anchor)
	{
		QMenu m(this);
		OBSSource s = m_source;
		m.addAction(Icons::get("gear"), tr("Settings"), this, [this, s] { showSourceDialog(s, false, window()); });
		m.addAction(Icons::get("filters"), tr("Filters"), this, [this, s] { showSourceDialog(s, true, window()); });
		auto *listen = m.addMenu(tr("Listen"));
		const obs_monitoring_type cur = obs_source_get_monitoring_type(s);
		for (auto [label, type] : {std::pair{tr("Off"), OBS_MONITORING_TYPE_NONE},
					   {tr("Monitor only (mute output)"), OBS_MONITORING_TYPE_MONITOR_ONLY},
					   {tr("Monitor and output"), OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT}}) {
			auto *a = listen->addAction(label, this, [s, type] {
				obs_source_set_monitoring_type(s, type);
				ENGINE.save();
			});
			a->setCheckable(true);
			a->setChecked(cur == type);
		}
		m.addAction(tr("Sync delay"), this, [this, s] {
			bool ok = false;
			const int ms = QInputDialog::getInt(this, tr("Sync delay"), tr("Delay this sound by (ms)"),
							    int(obs_source_get_sync_offset(s) / 1000000), -950, 20000, 10, &ok);
			if (ok) {
				obs_source_set_sync_offset(s, int64_t(ms) * 1000000);
				ENGINE.save();
			}
		});
		auto *mono = m.addAction(tr("Downmix to mono"), this, [s](bool on) {
			uint32_t f = obs_source_get_flags(s);
			obs_source_set_flags(s, on ? (f | OBS_SOURCE_FLAG_FORCE_MONO) : (f & ~OBS_SOURCE_FLAG_FORCE_MONO));
			ENGINE.save();
		});
		mono->setCheckable(true);
		mono->setChecked(obs_source_get_flags(s) & OBS_SOURCE_FLAG_FORCE_MONO);
		m.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
	}

	OBSSource m_source;
	ElidedLabel *m_name;
	Meter *m_meter;
	QSlider *m_fader;
	QLabel *m_db;
	QPushButton *m_mute;
	obs_volmeter_t *m_vol;
	obs_fader_t *m_fad;
};

Mixer::Mixer(QWidget *parent) : QWidget(parent)
{
	setObjectName("panel");
	setAttribute(Qt::WA_StyledBackground);
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(12, 6, 12, 10);
	v->setSpacing(6);
	auto *head = new QHBoxLayout;
	head->addWidget(panelHeader(tr("Audio")));
	head->addStretch(1);
	auto *opts = iconButton("sliders", tr("Sound devices"), this);
	connect(opts, &QPushButton::clicked, this, [this, opts] {
		QMenu m(this);
		for (obs_source_t *s : {ENGINE.desktopAudio(), ENGINE.micAudio()})
			if (s)
				m.addAction(tr("%1 device").arg(QString::fromUtf8(obs_source_get_name(s))), this,
					    [this, s] { showSourceDialog(s, false, window()); });
		m.exec(opts->mapToGlobal(QPoint(0, opts->height())));
	});
	head->addWidget(opts);
	v->addLayout(head);
	auto *scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	auto *body = new QWidget;
	m_rows = new QVBoxLayout(body);
	m_rows->setContentsMargins(0, 0, 4, 0);
	m_rows->setSpacing(10);
	m_rows->addStretch(1);
	scroll->setWidget(body);
	v->addWidget(scroll, 1);
	connect(&ENGINE, &Engine::audioSourcesChanged, this, &Mixer::refresh);
}

void Mixer::refresh()
{
	const auto list = ENGINE.audioSources();
	// Keep strips that are still wanted, so meters don't flicker.
	QVector<AudioStrip *> keep;
	for (AudioStrip *s : m_strips) {
		bool still = false;
		for (const auto &src : list)
			if (src == s->source())
				still = true;
		if (still)
			keep.append(s);
		else
			s->deleteLater();
	}
	QVector<AudioStrip *> ordered;
	for (const auto &src : list) {
		AudioStrip *found = nullptr;
		for (AudioStrip *s : keep)
			if (s->source() == src)
				found = s;
		if (!found)
			found = new AudioStrip(src, this);
		found->setCompact(m_compact);
		ordered.append(found);
	}
	for (int i = 0; i < ordered.size(); i++) {
		m_rows->removeWidget(ordered[i]);
		m_rows->insertWidget(i, ordered[i]);
		ordered[i]->show();
	}
	m_strips = ordered;
}

void Mixer::setCompact(bool c)
{
	m_compact = c;
	for (auto *s : m_strips)
		s->setCompact(c);
}
