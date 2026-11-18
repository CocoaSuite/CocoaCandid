#pragma once

#include <QString>
#include <obs.hpp>

/**
 * App settings, kept as one obs_data JSON file (settings.json) in the user's config folder, so encoder
 * settings can be edited by the same generic property forms that libobs sources use.
 */
class Config {
public:
	static Config &get();

	void load();
	void save();

	/** %APPDATA%/CocoaCandid on Windows, ~/.config/CocoaCandid elsewhere. Created if missing. */
	static QString dir();
	static QString file(const QString &name);

	obs_data_t *data() const { return m_data; }
	/** A sub-object, created with defaults if missing. The returned pointer is borrowed. */
	obs_data_t *section(const char *name);

	QString str(const char *sec, const char *key) const;
	long long num(const char *sec, const char *key) const;
	bool flag(const char *sec, const char *key) const;
	void setStr(const char *sec, const char *key, const QString &v);
	void setNum(const char *sec, const char *key, long long v);
	void setFlag(const char *sec, const char *key, bool v);

	bool firstRun() const { return m_firstRun; }

private:
	Config() = default;
	void applyDefaults();
	OBSDataAutoRelease m_data;
	bool m_firstRun = false;
};

#define CFG Config::get()
