#include "App.hpp"
#include "Config.hpp"
#include "Engine.hpp"
#include "Hotkeys.hpp"
#include "MainWindow.hpp"
#include "Outputs.hpp"
#include "Style.hpp"

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFontDatabase>
#include <QMessageBox>
#include <QTimer>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#if defined(__linux__) || defined(__FreeBSD__)
	// libobs draws through X11 (XWayland on Wayland desktops).
	qputenv("QT_QPA_PLATFORM", "xcb");
#endif
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
	QApplication app(argc, argv);
	QApplication::setApplicationName("CocoaCandid");
	QApplication::setOrganizationName("CocoaSuite");
	QApplication::setApplicationVersion(CANDID_VERSION);
#ifdef _WIN32
	// libobs finds its plugins and data relative to the working folder, as in OBS Studio.
	QDir::setCurrent(QCoreApplication::applicationDirPath());
#endif

	for (QDirIterator it(":/fonts", {"*.ttf"}); it.hasNext();)
		QFontDatabase::addApplicationFont(it.next());
	QFont f("Inter");
	f.setPixelSize(13);
	QApplication::setFont(f);

	CFG.load();
	setupLogging();
	Theme::apply(CFG.str("general", "theme"));

#ifdef _WIN32
	const QString pr = CFG.str("general", "priority");
	if (pr == "above")
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
	else if (pr == "high")
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif

	const QStringList args = QApplication::arguments();
	auto window = std::make_unique<MainWindow>();
	MainWindow &w = *window;
	if (args.contains("--minimize"))
		w.showMinimized();
	else
		w.show();

	// The window is on screen before the engine starts, so the app feels instant.
	QTimer::singleShot(0, &w, [&w, args] {
		QString err;
		if (!ENGINE.start(&err)) {
			QMessageBox::critical(&w, "CocoaCandid", err);
			QApplication::exit(1);
			return;
		}
		w.engineReady();
		const int sceneArg = args.indexOf("--scene");
		if (sceneArg >= 0 && sceneArg + 1 < args.size())
			for (const auto &s : ENGINE.scenes())
				if (QString::fromUtf8(obs_source_get_name(s)) == args[sceneArg + 1])
					ENGINE.showScene(s);
		QString e;
		if (args.contains("--startrecording"))
			OUTPUTS.startRecording(&e);
		if (args.contains("--startstreaming"))
			OUTPUTS.startStreaming(&e);
		if (args.contains("--startreplaybuffer"))
			OUTPUTS.startReplay(&e);
		if (args.contains("--startvirtualcam"))
			OUTPUTS.startVirtualCam(&e);
		// For scripts: stop everything cleanly and quit after a number of seconds.
		const int quitArg = args.indexOf("--quit-after");
		if (quitArg >= 0 && quitArg + 1 < args.size())
			QTimer::singleShot(args[quitArg + 1].toInt() * 1000, &w, [&w] {
				OUTPUTS.stopRecording();
				OUTPUTS.stopStreaming();
				QTimer::singleShot(1500, &w, [&w] { w.close(); });
			});
		if (!CFG.flag("general", "onboarded") && !args.contains("--no-welcome"))
			QTimer::singleShot(400, &w, [&w] { w.runWelcome(true); });
	});

	const int code = QApplication::exec();
	Hotkeys::get().unregisterAll();
	OUTPUTS.stopAll();
	// Panels hold references to sources, so they go before libobs does.
	window.reset();
	ENGINE.shutdown();
	CFG.save();
	return code;
}
