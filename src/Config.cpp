#include "Config.hpp"

#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>

Config &Config::get()
{
	static Config c;
	return c;
}

QString Config::dir()
{
	QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#ifdef _WIN32
	base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation); // %LOCALAPPDATA%
	const QString roaming = qEnvironmentVariable("APPDATA");
	if (!roaming.isEmpty())
		base = roaming;
#endif
	QDir d(base);
	d.mkpath("CocoaCandid");
	return d.filePath("CocoaCandid");
}

QString Config::file(const QString &name)
{
	return QDir(dir()).filePath(name);
}

void Config::load()
{
	const QByteArray path = file("settings.json").toUtf8();
	m_data = obs_data_create_from_json_file_safe(path.constData(), "bak");
	if (!m_data) {
		m_data = obs_data_create();
		m_firstRun = true;
	}
	applyDefaults();
}

void Config::save()
{
	const QByteArray path = file("settings.json").toUtf8();
	obs_data_save_json_safe(m_data, path.constData(), "tmp", "bak");
}

obs_data_t *Config::section(const char *name)
{
	OBSDataAutoRelease s = obs_data_get_obj(m_data, name);
	if (!s) {
		s = obs_data_create();
		obs_data_set_obj(m_data, name, s);
	}
	return s.Get(); // still owned by m_data
}

QString Config::str(const char *sec, const char *key) const
{
	OBSDataAutoRelease s = obs_data_get_obj(m_data, sec);
	return s ? QString::fromUtf8(obs_data_get_string(s, key)) : QString();
}

long long Config::num(const char *sec, const char *key) const
{
	OBSDataAutoRelease s = obs_data_get_obj(m_data, sec);
	return s ? obs_data_get_int(s, key) : 0;
}

bool Config::flag(const char *sec, const char *key) const
{
	OBSDataAutoRelease s = obs_data_get_obj(m_data, sec);
	return s ? obs_data_get_bool(s, key) : false;
}

void Config::setStr(const char *sec, const char *key, const QString &v)
{
	obs_data_set_string(section(sec), key, v.toUtf8().constData());
}

void Config::setNum(const char *sec, const char *key, long long v)
{
	obs_data_set_int(section(sec), key, v);
}

void Config::setFlag(const char *sec, const char *key, bool v)
{
	obs_data_set_bool(section(sec), key, v);
}

void Config::applyDefaults()
{
	obs_data_t *g = section("general");
	obs_data_set_default_string(g, "theme", "dark");
	obs_data_set_default_string(g, "icon", "garnet");
	obs_data_set_default_string(g, "layout", "classic");
	obs_data_set_default_bool(g, "onboarded", false);
	obs_data_set_default_bool(g, "confirm_stop_stream", true);
	obs_data_set_default_bool(g, "snap", true);

	// The canvas starts at the size of the main screen, in real pixels.
	int w = 1920, h = 1080;
	if (QScreen *s = QGuiApplication::primaryScreen()) {
		const qreal r = s->devicePixelRatio();
		w = int(s->geometry().width() * r) & ~1;
		h = int(s->geometry().height() * r) & ~1;
	}
	obs_data_t *v = section("video");
	obs_data_set_default_int(v, "base_w", w);
	obs_data_set_default_int(v, "base_h", h);
	obs_data_set_default_int(v, "out_w", w > 1920 ? 1920 : w);
	obs_data_set_default_int(v, "out_h", w > 1920 ? (h * 1920 / w) & ~1 : h);
	obs_data_set_default_int(v, "fps_num", 60);
	obs_data_set_default_int(v, "fps_den", 1);
	obs_data_set_default_string(v, "scale", "lanczos");
	obs_data_set_default_string(v, "color_space", "709");
	obs_data_set_default_string(v, "range", "partial");

	obs_data_t *a = section("audio");
	obs_data_set_default_int(a, "sample_rate", 48000);
	obs_data_set_default_string(a, "speakers", "stereo");

	obs_data_t *st = section("stream");
	obs_data_set_default_string(st, "type", "rtmp_common");
	obs_data_set_default_string(st, "service", "Twitch");
	obs_data_set_default_string(st, "server", "auto");
	obs_data_set_default_string(st, "key", "");
	obs_data_set_default_string(st, "custom_server", "");
	obs_data_set_default_string(st, "encoder", "");
	obs_data_set_default_int(st, "bitrate", 6000);
	obs_data_set_default_int(st, "audio_bitrate", 160);
	obs_data_set_default_int(st, "keyint_sec", 2);
	obs_data_set_default_bool(st, "dyn_bitrate", true);

	obs_data_t *r = section("record");
	obs_data_set_default_string(r, "path", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation).toUtf8().constData());
	obs_data_set_default_string(r, "format", "mp4");
	obs_data_set_default_string(r, "encoder", "");
	obs_data_set_default_string(r, "quality", "high");
	obs_data_set_default_int(r, "audio_bitrate", 192);
	obs_data_set_default_bool(r, "replay", false);
	obs_data_set_default_int(r, "replay_sec", 30);
}
