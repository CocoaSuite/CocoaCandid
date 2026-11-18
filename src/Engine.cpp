#include "Engine.hpp"
#include "Config.hpp"

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMetaObject>
#include <QScreen>
#include <cmath>
#include <functional>
#include <graphics/matrix4.h>

#if defined(__linux__) || defined(__FreeBSD__)
#include <obs-nix-platform.h>
#include <QtGui/qguiapplication_platform.h>
#endif

Engine &Engine::get()
{
	static Engine e;
	return e;
}

void Engine::post(std::function<void()> fn)
{
	QMetaObject::invokeMethod(this, [this, fn = std::move(fn)] {
		if (m_running)
			fn();
	}, Qt::QueuedConnection);
}

QString Engine::firstType(std::initializer_list<const char *> ids)
{
	// The newest version of each type: libobs lists versions in order, so the last match wins.
	for (const char *want : ids) {
		QString found;
		const char *id = nullptr, *unversioned = nullptr;
		for (size_t i = 0; obs_enum_input_types2(i, &id, &unversioned); i++) {
			if (strcmp(unversioned ? unversioned : id, want) != 0 && strcmp(id, want) != 0)
				continue;
			if (obs_get_source_output_flags(id) & OBS_SOURCE_CAP_DISABLED)
				continue;
			found = QString::fromUtf8(id);
		}
		if (!found.isEmpty())
			return found;
	}
	return {};
}

bool Engine::start(QString *error)
{
#if defined(__linux__) || defined(__FreeBSD__)
	// libobs draws into Qt's own windows, so it has to use the same X display connection (main() picks xcb).
	obs_set_nix_platform(OBS_NIX_PLATFORM_X11_EGL);
	if (auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
		obs_set_nix_platform_display(x11->display());
#endif

	const QByteArray moduleCfg = Config::file("plugin_config").toUtf8();
	if (!obs_startup("en-US", moduleCfg.constData(), nullptr)) {
		if (error)
			*error = tr("The video engine could not start.");
		return false;
	}

	obs_audio_info2 oai = {};
	oai.samples_per_sec = uint32_t(CFG.num("audio", "sample_rate"));
	oai.speakers = CFG.str("audio", "speakers") == "mono" ? SPEAKERS_MONO : SPEAKERS_STEREO;
	if (!obs_reset_audio2(&oai)) {
		if (error)
			*error = tr("Sound could not start.");
		return false;
	}

	if (!resetVideo(error))
		return false;

	// These plugins only work inside OBS Studio's own window (they call its frontend API).
	static const char *frontendOnly[] = {"frontend-tools", "decklink-output-ui", "aja-output-ui", "obs-websocket", "decklink-captions"};
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(32, 0, 0)
	for (const char *m : frontendOnly)
		obs_add_disabled_module(m);
#else
	// Older libobs has no way to skip a plugin, only to allow a fixed list.
	for (const char *m : {"image-source", "linux-alsa", "linux-capture", "linux-jack", "linux-pipewire", "linux-pulseaudio",
			      "linux-v4l2", "obs-ffmpeg", "obs-filters", "obs-libfdk", "obs-nvenc", "obs-outputs", "obs-qsv11",
			      "obs-transitions", "obs-x264", "rtmp-services", "text-freetype2", "vlc-video", "obs-webrtc", "decklink",
			      "obs-vst", "obs-browser", "win-capture", "win-dshow", "win-wasapi", "obs-text", "coreaudio-encoder",
			      "nv-filters", "mac-capture", "mac-avcapture", "mac-videotoolbox", "mac-virtualcam", "mac-syphon"})
		obs_add_safe_module(m);
	(void)frontendOnly;
#endif
	obs_module_failure_info mfi = {};
	obs_load_all_modules2(&mfi);
	obs_log_loaded_modules();
	obs_post_load_modules();
	obs_module_failure_info_free(&mfi);

	m_running = true;

	signal_handler_t *sh = obs_get_signal_handler();
	for (const char *sig : {"source_create", "source_destroy", "source_rename", "source_activate", "source_deactivate",
				"source_show", "source_hide", "source_audio_activate", "source_audio_deactivate"})
		signal_handler_connect(sh, sig, onGlobalSignal, this);

	load();
	return true;
}

void Engine::shutdown()
{
	if (!m_running)
		return;
	save();
	signal_handler_t *sh = obs_get_signal_handler();
	for (const char *sig : {"source_create", "source_destroy", "source_rename", "source_activate", "source_deactivate",
				"source_show", "source_hide", "source_audio_activate", "source_audio_deactivate"})
		signal_handler_disconnect(sh, sig, onGlobalSignal, this);
	m_running = false;

	for (auto &s : m_scenes)
		unhookScene(s);
	for (uint32_t i = 0; i < MAX_CHANNELS; i++)
		obs_set_output_source(i, nullptr);
	m_scenes.clear();
	m_program = nullptr;
	m_preview = nullptr;
	m_transition = nullptr;
	m_desktop = nullptr;
	m_mic = nullptr;

	// Removing every remaining source lets plugins stop their capture threads before libobs goes away.
	obs_enum_sources([](void *, obs_source_t *s) { obs_source_remove(s); return true; }, nullptr);
	obs_shutdown();
}

// Canvas --------------------------------------------------------------------------------------------

QSize Engine::canvasSize() const
{
	obs_video_info ovi;
	if (obs_get_video_info(&ovi))
		return QSize(int(ovi.base_width), int(ovi.base_height));
	return QSize(int(CFG.num("video", "base_w")), int(CFG.num("video", "base_h")));
}

QSize Engine::outputSize() const
{
	obs_video_info ovi;
	if (obs_get_video_info(&ovi))
		return QSize(int(ovi.output_width), int(ovi.output_height));
	return QSize(int(CFG.num("video", "out_w")), int(CFG.num("video", "out_h")));
}

double Engine::fps() const
{
	obs_video_info ovi;
	if (obs_get_video_info(&ovi) && ovi.fps_den)
		return double(ovi.fps_num) / ovi.fps_den;
	return 60;
}

static obs_scale_type scaleFromName(const QString &n)
{
	if (n == "bilinear")
		return OBS_SCALE_BILINEAR;
	if (n == "bicubic")
		return OBS_SCALE_BICUBIC;
	if (n == "area")
		return OBS_SCALE_AREA;
	return OBS_SCALE_LANCZOS;
}

bool Engine::resetVideo(QString *error)
{
	obs_video_info ovi = {};
	ovi.graphics_module = CANDID_GRAPHICS;
	ovi.fps_num = uint32_t(std::max<long long>(1, CFG.num("video", "fps_num")));
	ovi.fps_den = uint32_t(std::max<long long>(1, CFG.num("video", "fps_den")));
	ovi.base_width = uint32_t(std::clamp<long long>(CFG.num("video", "base_w"), 16, 16384)) & ~1u;
	ovi.base_height = uint32_t(std::clamp<long long>(CFG.num("video", "base_h"), 16, 16384)) & ~1u;
	ovi.output_width = uint32_t(std::clamp<long long>(CFG.num("video", "out_w"), 16, 16384)) & ~1u;
	ovi.output_height = uint32_t(std::clamp<long long>(CFG.num("video", "out_h"), 16, 16384)) & ~1u;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.colorspace = CFG.str("video", "color_space") == "601" ? VIDEO_CS_601 : VIDEO_CS_709;
	ovi.range = CFG.str("video", "range") == "full" ? VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;
	ovi.adapter = 0;
	ovi.gpu_conversion = true;
	ovi.scale_type = scaleFromName(CFG.str("video", "scale"));

	const int r = obs_reset_video(&ovi);
	if (r != OBS_VIDEO_SUCCESS) {
		if (error) {
			switch (r) {
			case OBS_VIDEO_CURRENTLY_ACTIVE:
				*error = tr("Stop recording and streaming first.");
				break;
			case OBS_VIDEO_NOT_SUPPORTED:
				*error = tr("Your graphics card or its driver does not support what CocoaCandid needs. Updating the graphics driver usually fixes this.");
				break;
			case OBS_VIDEO_INVALID_PARAM:
				*error = tr("That canvas size or frame rate can't be used.");
				break;
			default:
				*error = tr("The graphics card could not be started.");
			}
		}
		return false;
	}
	if (m_running)
		emit canvasChanged();
	return true;
}

static QRectF itemRect(obs_sceneitem_t *item)
{
	matrix4 m;
	obs_sceneitem_get_box_transform(item, &m);
	auto corner = [&](float x, float y) {
		vec3 v;
		vec3_set(&v, x, y, 0.0f);
		vec3_transform(&v, &v, &m);
		return QPointF(v.x, v.y);
	};
	const QPointF p[4] = {corner(0, 0), corner(1, 0), corner(0, 1), corner(1, 1)};
	double l = p[0].x(), r = l, t = p[0].y(), b = t;
	for (const auto &q : p) {
		l = std::min(l, q.x());
		r = std::max(r, q.x());
		t = std::min(t, q.y());
		b = std::max(b, q.y());
	}
	return QRectF(QPointF(l, t), QPointF(r, b));
}

bool Engine::cropCanvasTo(obs_sceneitem_t *item, QString *error)
{
	if (!item)
		return false;
	if (obs_video_active()) {
		if (error)
			*error = tr("Stop recording and streaming first.");
		return false;
	}
	const QRectF r = itemRect(item);
	const int w = int(std::lround(r.width())) & ~1, h = int(std::lround(r.height())) & ~1;
	if (w < 16 || h < 16) {
		if (error)
			*error = tr("This source has no picture yet.");
		return false;
	}

	const QSize oldBase = canvasSize(), oldOut = outputSize();
	CFG.setNum("video", "base_w", w);
	CFG.setNum("video", "base_h", h);
	// Keep the output no bigger than it was, at the new shape.
	if (oldOut == oldBase || (w <= oldOut.width() && h <= oldOut.height())) {
		CFG.setNum("video", "out_w", w);
		CFG.setNum("video", "out_h", h);
	} else {
		const double s = std::min(double(oldOut.width()) / w, double(oldOut.height()) / h);
		CFG.setNum("video", "out_w", int(w * s) & ~1);
		CFG.setNum("video", "out_h", int(h * s) & ~1);
	}
	if (!resetVideo(error)) {
		CFG.setNum("video", "base_w", oldBase.width());
		CFG.setNum("video", "base_h", oldBase.height());
		CFG.setNum("video", "out_w", oldOut.width());
		CFG.setNum("video", "out_h", oldOut.height());
		resetVideo();
		return false;
	}
	CFG.save();

	// Everything in the scene moves with the feed, so a webcam in its corner stays there.
	float d[2] = {float(r.left()), float(r.top())};
	obs_scene_enum_items(obs_sceneitem_get_scene(item), [](obs_scene_t *, obs_sceneitem_t *it, void *p) {
		const float *d = static_cast<const float *>(p);
		vec2 pos;
		obs_sceneitem_get_pos(it, &pos);
		pos.x -= d[0];
		pos.y -= d[1];
		obs_sceneitem_set_pos(it, &pos);
		return true;
	}, d);
	save();
	return true;
}

// Scenes ---------------------------------------------------------------------------------------------

QString Engine::uniqueName(const QString &base) const
{
	auto taken = [](const QString &n) {
		OBSSourceAutoRelease s = obs_get_source_by_name(n.toUtf8().constData());
		return s != nullptr;
	};
	if (!taken(base))
		return base;
	for (int i = 2;; i++) {
		const QString n = QString("%1 %2").arg(base).arg(i);
		if (!taken(n))
			return n;
	}
}

void Engine::hookScene(obs_source_t *scene)
{
	signal_handler_t *sh = obs_source_get_signal_handler(scene);
	for (const char *sig : {"item_add", "item_remove", "reorder", "refresh", "item_visible", "item_locked",
				"item_select", "item_deselect", "item_transform"})
		signal_handler_connect(sh, sig, onSceneSignal, this);
}

void Engine::unhookScene(obs_source_t *scene)
{
	signal_handler_t *sh = obs_source_get_signal_handler(scene);
	for (const char *sig : {"item_add", "item_remove", "reorder", "refresh", "item_visible", "item_locked",
				"item_select", "item_deselect", "item_transform"})
		signal_handler_disconnect(sh, sig, onSceneSignal, this);
}

void Engine::onSceneSignal(void *data, calldata_t *cd)
{
	auto *self = static_cast<Engine *>(data);
	(void)cd;
	// The signal's name isn't passed, so every scene change is coalesced into one refresh per event loop turn.
	if (!self->m_pendingItems) {
		self->m_pendingItems = true;
		self->post([self] {
			self->m_pendingItems = false;
			emit self->itemsChanged();
			emit self->selectionChanged();
			emit self->itemTransformed();
		});
	}
}

void Engine::onGlobalSignal(void *data, calldata_t *)
{
	auto *self = static_cast<Engine *>(data);
	if (self->m_loading || self->m_pendingAudio)
		return;
	self->m_pendingAudio = true;
	self->post([self] {
		self->m_pendingAudio = false;
		emit self->audioSourcesChanged();
		emit self->scenesChanged();
	});
}

void Engine::setTransitionSource()
{
	OBSDataAutoRelease s = obs_data_create();
	OBSSourceAutoRelease t = obs_source_create_private(m_transitionId.toUtf8().constData(), "Transition", s);
	if (!t)
		t = obs_source_create_private("fade_transition", "Transition", s);
	if (!t)
		t = obs_source_create_private("cut_transition", "Transition", s);
	if (!t)
		return;
	obs_transition_set(t, m_program);
	obs_set_output_source(0, t);
	m_transition = t.Get();
}

void Engine::setTransition(const QString &id, int ms)
{
	m_transitionId = id;
	m_transitionMs = std::clamp(ms, 0, 20000);
	setTransitionSource();
	save();
}

QVector<QPair<QString, QString>> Engine::transitionTypes() const
{
	QVector<QPair<QString, QString>> out;
	const char *id;
	for (size_t i = 0; obs_enum_transition_types(i, &id); i++) {
		const char *name = obs_source_get_display_name(id);
		if (name && strcmp(id, "obs_stinger_transition") != 0)
			out.append({QString::fromUtf8(id), QString::fromUtf8(name)});
	}
	return out;
}

void Engine::showScene(obs_source_t *scene)
{
	if (!scene)
		return;
	if (m_studio) {
		m_preview = scene;
		emit editSceneChanged();
		emit itemsChanged();
		emit selectionChanged();
		return;
	}
	if (scene == m_program)
		return;
	m_program = scene;
	if (m_transition) {
		const bool cut = m_transitionId == "cut_transition" || m_transitionMs == 0;
		if (cut || !obs_transition_start(m_transition, OBS_TRANSITION_MODE_AUTO, uint32_t(m_transitionMs), scene))
			obs_transition_set(m_transition, scene);
	}
	emit programChanged();
	emit editSceneChanged();
	emit itemsChanged();
	emit selectionChanged();
	emit audioSourcesChanged();
}

void Engine::setStudioMode(bool on)
{
	if (on == m_studio)
		return;
	m_studio = on;
	m_preview = m_program;
	emit studioModeChanged(on);
	emit editSceneChanged();
	emit itemsChanged();
	emit selectionChanged();
}

void Engine::sendPreviewLive()
{
	if (!m_studio || !m_preview)
		return;
	OBSSource next = m_preview;
	m_studio = false;
	showScene(next);
	m_studio = true;
	emit programChanged();
}

obs_source_t *Engine::addScene(const QString &name)
{
	OBSSceneAutoRelease sc = obs_scene_create(uniqueName(name).toUtf8().constData());
	obs_source_t *src = obs_scene_get_source(sc);
	m_scenes.append(OBSSource(src));
	hookScene(src);
	emit scenesChanged();
	save();
	return src;
}

obs_source_t *Engine::duplicateScene(obs_source_t *scene)
{
	if (!scene)
		return nullptr;
	const QString name = uniqueName(QString::fromUtf8(obs_source_get_name(scene)));
	OBSSceneAutoRelease dup = obs_scene_duplicate(obs_scene_from_source(scene), name.toUtf8().constData(), OBS_SCENE_DUP_REFS);
	obs_source_t *src = obs_scene_get_source(dup);
	const int at = m_scenes.indexOf(OBSSource(scene));
	m_scenes.insert(at + 1, OBSSource(src));
	hookScene(src);
	emit scenesChanged();
	save();
	return src;
}

void Engine::removeScene(obs_source_t *scene)
{
	if (m_scenes.size() <= 1)
		return;
	const int at = m_scenes.indexOf(OBSSource(scene));
	if (at < 0)
		return;
	OBSSource keep = scene;
	unhookScene(scene);
	m_scenes.removeAt(at);
	if (m_program == scene) {
		const bool st = m_studio;
		m_studio = false;
		showScene(m_scenes.value(std::max(0, at - 1)));
		m_studio = st;
	}
	if (m_preview == scene)
		m_preview = m_program;
	obs_source_remove(keep);
	emit scenesChanged();
	emit editSceneChanged();
	emit itemsChanged();
	save();
}

void Engine::moveScene(int from, int to)
{
	if (from < 0 || from >= m_scenes.size() || to < 0 || to >= m_scenes.size() || from == to)
		return;
	m_scenes.move(from, to);
	emit scenesChanged();
	save();
}

bool Engine::rename(obs_source_t *source, const QString &name)
{
	const QString n = name.trimmed();
	if (!source || n.isEmpty())
		return false;
	if (n == QString::fromUtf8(obs_source_get_name(source)))
		return true;
	OBSSourceAutoRelease other = obs_get_source_by_name(n.toUtf8().constData());
	if (other)
		return false;
	obs_source_set_name(source, n.toUtf8().constData());
	emit scenesChanged();
	emit itemsChanged();
	emit audioSourcesChanged();
	save();
	return true;
}

// Items -----------------------------------------------------------------------------------------------

obs_sceneitem_t *Engine::selectedItem() const
{
	obs_scene_t *scene = editSceneObj();
	if (!scene)
		return nullptr;
	obs_sceneitem_t *found = nullptr;
	obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *it, void *p) {
		if (obs_sceneitem_selected(it)) {
			*static_cast<obs_sceneitem_t **>(p) = it;
			return false;
		}
		return true;
	}, &found);
	return found;
}

void Engine::select(obs_sceneitem_t *item)
{
	obs_scene_t *scene = editSceneObj();
	if (!scene)
		return;
	obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *it, void *p) {
		obs_sceneitem_select(it, it == p);
		return true;
	}, item);
	emit selectionChanged();
}

obs_sceneitem_t *Engine::addToScene(obs_source_t *source, bool fitCanvas)
{
	obs_scene_t *scene = editSceneObj();
	if (!scene || !source)
		return nullptr;
	obs_sceneitem_t *item = obs_scene_add(scene, source);
	if (!item)
		return nullptr;
	if (fitCanvas) {
		// Bounds keep it fitted even before the capture knows its size.
		const QSize c = canvasSize();
		vec2 b;
		vec2_set(&b, float(c.width()), float(c.height()));
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
		obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
		obs_sceneitem_set_bounds(item, &b);
	}
	select(item);
	emit itemsChanged();
	save();
	return item;
}

void Engine::removeItem(obs_sceneitem_t *item)
{
	if (!item)
		return;
	obs_sceneitem_remove(item);
	emit itemsChanged();
	emit selectionChanged();
	save();
}

// Audio -----------------------------------------------------------------------------------------------

QVector<OBSSource> Engine::audioSources() const
{
	struct Ctx {
		const Engine *self;
		QVector<OBSSource> list;
	} ctx{this, {}};
	if (m_desktop)
		ctx.list.append(m_desktop);
	if (m_mic)
		ctx.list.append(m_mic);
	obs_enum_sources([](void *p, obs_source_t *s) {
		auto *c = static_cast<Ctx *>(p);
		if (s == c->self->m_desktop || s == c->self->m_mic || obs_source_is_scene(s) || obs_source_is_group(s))
			return true;
		if (!(obs_source_get_output_flags(s) & OBS_SOURCE_AUDIO))
			return true;
		if (obs_source_active(s) || obs_source_showing(s))
			c->list.append(OBSSource(s));
		return true;
	}, &ctx);
	return ctx.list;
}

void Engine::createAudioDevices(obs_data_t *desktop, obs_data_t *mic)
{
	auto make = [](obs_data_t *saved, std::initializer_list<const char *> types, const char *name) -> OBSSource {
		if (saved) {
			OBSSourceAutoRelease s = obs_load_source(saved);
			if (s)
				return OBSSource(s.Get());
		}
		const QString type = firstType(types);
		if (type.isEmpty())
			return nullptr;
		OBSDataAutoRelease set = obs_data_create();
		obs_data_set_string(set, "device_id", "default");
		OBSSourceAutoRelease s = obs_source_create(type.toUtf8().constData(), name, set, nullptr);
		return OBSSource(s.Get());
	};
	m_desktop = make(desktop, {"wasapi_output_capture", "pulse_output_capture", "coreaudio_output_capture"}, "Desktop audio");
	m_mic = make(mic, {"wasapi_input_capture", "pulse_input_capture", "coreaudio_input_capture"}, "Microphone");
	obs_set_output_source(1, m_desktop);
	obs_set_output_source(2, m_mic);
}

// Save and load ---------------------------------------------------------------------------------------

void Engine::save()
{
	if (!m_running || m_loading)
		return;
	OBSDataAutoRelease root = obs_data_create();
	OBSDataArrayAutoRelease all = obs_save_sources_filtered([](void *p, obs_source_t *s) {
		auto *self = static_cast<Engine *>(p);
		return s != self->m_desktop && s != self->m_mic;
	}, this);
	obs_data_set_array(root, "sources", all);

	OBSDataArrayAutoRelease order = obs_data_array_create();
	for (const auto &s : m_scenes) {
		OBSDataAutoRelease o = obs_data_create();
		obs_data_set_string(o, "name", obs_source_get_name(s));
		obs_data_array_push_back(order, o);
	}
	obs_data_set_array(root, "order", order);
	obs_data_set_string(root, "program", m_program ? obs_source_get_name(m_program) : "");
	obs_data_set_string(root, "preview", m_preview ? obs_source_get_name(m_preview) : "");
	obs_data_set_bool(root, "studio", m_studio);
	obs_data_set_string(root, "transition", m_transitionId.toUtf8().constData());
	obs_data_set_int(root, "transition_ms", m_transitionMs);
	if (m_desktop) {
		OBSDataAutoRelease d = obs_save_source(m_desktop);
		obs_data_set_obj(root, "desktop", d);
	}
	if (m_mic) {
		OBSDataAutoRelease d = obs_save_source(m_mic);
		obs_data_set_obj(root, "mic", d);
	}
	const QByteArray path = Config::file("scenes.json").toUtf8();
	obs_data_save_json_safe(root, path.constData(), "tmp", "bak");
}

void Engine::load()
{
	m_loading = true;
	const QByteArray path = Config::file("scenes.json").toUtf8();
	OBSDataAutoRelease root = obs_data_create_from_json_file_safe(path.constData(), "bak");
	if (!root) {
		createAudioDevices(nullptr, nullptr);
		createDefaults();
		m_loading = false;
		save();
		return;
	}

	OBSDataAutoRelease desk = obs_data_get_obj(root, "desktop");
	OBSDataAutoRelease mic = obs_data_get_obj(root, "mic");
	createAudioDevices(desk, mic);

	QVector<OBSSource> loaded;
	OBSDataArrayAutoRelease all = obs_data_get_array(root, "sources");
	obs_load_sources(all, [](void *p, obs_source_t *s) {
		if (obs_source_is_scene(s))
			static_cast<QVector<OBSSource> *>(p)->append(OBSSource(s));
	}, &loaded);

	// Put them in the saved order; any not listed go last.
	OBSDataArrayAutoRelease order = obs_data_get_array(root, "order");
	for (size_t i = 0, n = obs_data_array_count(order); i < n; i++) {
		OBSDataAutoRelease o = obs_data_array_item(order, i);
		const char *name = obs_data_get_string(o, "name");
		for (int k = 0; k < loaded.size(); k++)
			if (strcmp(obs_source_get_name(loaded[k]), name) == 0) {
				m_scenes.append(loaded.takeAt(k));
				break;
			}
	}
	m_scenes += loaded;
	if (m_scenes.isEmpty()) {
		createDefaults();
		m_loading = false;
		return;
	}
	for (auto &s : m_scenes)
		hookScene(s);

	auto find = [&](const char *name) -> obs_source_t * {
		for (auto &s : m_scenes)
			if (strcmp(obs_source_get_name(s), name) == 0)
				return s;
		return m_scenes.first();
	};
	m_program = find(obs_data_get_string(root, "program"));
	m_preview = find(obs_data_get_string(root, "preview"));
	m_studio = obs_data_get_bool(root, "studio");
	const char *tid = obs_data_get_string(root, "transition");
	if (*tid)
		m_transitionId = QString::fromUtf8(tid);
	obs_data_set_default_int(root, "transition_ms", 300);
	m_transitionMs = int(obs_data_get_int(root, "transition_ms"));
	setTransitionSource();
	m_loading = false;
}

void Engine::createDefaults()
{
	OBSSceneAutoRelease sc = obs_scene_create("Scene");
	obs_source_t *src = obs_scene_get_source(sc);
	m_scenes.append(OBSSource(src));
	hookScene(src);
	m_program = src;
	m_preview = src;
	setTransitionSource();

	// A first scene that already shows something: the main screen.
	const QString screen = firstType({"monitor_capture", "xshm_input", "screen_capture"});
	if (!screen.isEmpty()) {
		OBSSourceAutoRelease s = obs_source_create(screen.toUtf8().constData(), "Screen", nullptr, nullptr);
		if (s) {
			obs_sceneitem_t *item = obs_scene_add(sc, s);
			if (item) {
				const QSize c = canvasSize();
				vec2 b;
				vec2_set(&b, float(c.width()), float(c.height()));
				obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
				obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
				obs_sceneitem_set_bounds(item, &b);
			}
		}
	}
}

// Screenshot ------------------------------------------------------------------------------------------

QImage Engine::screenshot()
{
	QImage out;
	obs_enter_graphics();
	gs_texture_t *tex = obs_get_main_texture();
	if (tex) {
		const uint32_t w = gs_texture_get_width(tex), h = gs_texture_get_height(tex);
		const gs_color_format fmt = gs_texture_get_color_format(tex);
		if (fmt == GS_BGRA || fmt == GS_RGBA || fmt == GS_BGRX) {
			gs_stagesurf_t *st = gs_stagesurface_create(w, h, fmt);
			if (st) {
				gs_stage_texture(st, tex);
				uint8_t *data = nullptr;
				uint32_t ls = 0;
				if (gs_stagesurface_map(st, &data, &ls)) {
					const QImage::Format qf = fmt == GS_RGBA ? QImage::Format_RGBA8888 : QImage::Format_RGB32;
					out = QImage(data, int(w), int(h), int(ls), qf).copy();
					gs_stagesurface_unmap(st);
				}
				gs_stagesurface_destroy(st);
			}
		}
	}
	obs_leave_graphics();
	return out;
}
