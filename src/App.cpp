#include "App.hpp"
#include "Config.hpp"

#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <cstdarg>
#include <cstdio>
#include <util/base.h>

static FILE *g_log = nullptr;
static QString g_logPath;
static QMutex g_logLock;

static void logHandler(int level, const char *fmt, va_list args, void *)
{
	char buf[8192];
	vsnprintf(buf, sizeof(buf), fmt, args);
	const QByteArray stamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz").toUtf8();
	QMutexLocker l(&g_logLock);
	if (g_log) {
		fprintf(g_log, "%s: %s\n", stamp.constData(), buf);
		fflush(g_log);
	}
#ifndef NDEBUG
	fprintf(stderr, "%s\n", buf);
#else
	if (level <= LOG_WARNING)
		fprintf(stderr, "%s\n", buf);
#endif
}

void setupLogging()
{
	QDir dir(Config::dir());
	dir.mkpath("logs");
	dir.cd("logs");
	// Keep the ten newest logs.
	const QStringList old = dir.entryList({"*.txt"}, QDir::Files, QDir::Time);
	for (int i = 9; i < old.size(); i++)
		dir.remove(old[i]);
	g_logPath = dir.filePath(QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss") + ".txt");
	g_log = fopen(QFile::encodeName(g_logPath).constData(), "w");
	base_set_log_handler(logHandler, nullptr);
	blog(LOG_INFO, "CocoaCandid %s", CANDID_VERSION);
}

QString currentLogFile()
{
	return g_logPath;
}
