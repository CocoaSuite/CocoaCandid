#include "MainWindow.hpp"
#include "AddSourceDialog.hpp"
#include "AreaPicker.hpp"
#include "Config.hpp"
#include "Engine.hpp"
#include "Hotkeys.hpp"
#include "Icons.hpp"
#include "Inspector.hpp"
#include "Mixer.hpp"
#include "Onboard.hpp"
#include "Outputs.hpp"
#include "Preview.hpp"
#include "SceneList.hpp"
#include "SettingsDialog.hpp"
#include "SourceList.hpp"
#include "Style.hpp"
#include "Widgets.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStorageInfo>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <graphics/matrix4.h>

static const char *kReleases = "https://github.com/CocoaSuite/CocoaCandid/releases/latest";
static const char *kGuide = "https://github.com/CocoaSuite/CocoaCandid/blob/main/docs/USER_GUIDE.md";

MainWindow::MainWindow()
{
	setWindowTitle("CocoaCandid");
	setMinimumSize(980, 620);
	resize(1440, 900);

	auto *central = new QWidget;
	auto *v = new QVBoxLayout(central);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(0);
	v->addWidget(buildTopBar());
	v->addWidget(hline());

	m_hsplit = new QSplitter(Qt::Horizontal);
	m_hsplit->setHandleWidth(4);
	m_hsplit->setChildrenCollapsible(false);
	m_left = buildLeft();
	m_hsplit->addWidget(m_left);

	m_vsplit = new QSplitter(Qt::Vertical);
	m_vsplit->setHandleWidth(4);
	m_vsplit->setChildrenCollapsible(false);
	m_vsplit->addWidget(buildDesk());
	m_mixer = new Mixer;
	m_mixer->setMinimumHeight(96);
	m_vsplit->addWidget(m_mixer);
	m_vsplit->setStretchFactor(0, 1);
	m_vsplit->setSizes({680, 170});
	m_hsplit->addWidget(m_vsplit);

	m_rightBox = new QWidget;
	auto *rv = new QHBoxLayout(m_rightBox);
	rv->setContentsMargins(0, 0, 0, 0);
	rv->setSpacing(0);
	rv->addWidget(vline());
	m_inspector = new Inspector;
	rv->addWidget(m_inspector);
	m_rightBox->setMinimumWidth(260);
	m_hsplit->addWidget(m_rightBox);
	m_hsplit->setStretchFactor(1, 1);
	m_hsplit->setSizes({240, 900, 300});
	v->addWidget(m_hsplit, 1);
	setCentralWidget(central);

	buildFooter();
	buildMenus();

	const QByteArray geo = QByteArray::fromBase64(CFG.str("general", "geometry").toLatin1());
	if (!geo.isEmpty())
		restoreGeometry(geo);
	const QByteArray hs = QByteArray::fromBase64(CFG.str("general", "hsplit").toLatin1());
	if (!hs.isEmpty())
		m_hsplit->restoreState(hs);
	const QByteArray vs = QByteArray::fromBase64(CFG.str("general", "vsplit").toLatin1());
	if (!vs.isEmpty())
		m_vsplit->restoreState(vs);
	applyLook();
}

// Top bar ---------------------------------------------------------------------------------------------

QWidget *MainWindow::buildTopBar()
{
	auto *bar = new QWidget;
	bar->setObjectName("panel");
	bar->setAttribute(Qt::WA_StyledBackground);
	bar->setFixedHeight(42);
	auto *h = new QHBoxLayout(bar);
	h->setContentsMargins(10, 0, 10, 0);
	h->setSpacing(4);

	auto *logo = new QLabel;
	logo->setPixmap(Icons::app(CFG.str("general", "icon")).pixmap(22, 22));
	logo->setObjectName("logo");
	auto *brand = new QLabel("CocoaCandid");
	brand->setStyleSheet("font-weight: 600; font-size: 14px;");
	h->addWidget(logo);
	h->addSpacing(4);
	h->addWidget(brand);
	h->addSpacing(8);

	for (const QString &name : {tr("File"), tr("Edit"), tr("View"), tr("Scene"), tr("Tools"), tr("Help")}) {
		auto *b = new QToolButton;
		b->setText(name);
		b->setObjectName("menu_" + name);
		b->setPopupMode(QToolButton::InstantPopup);
		setKind(b, "mt");
		h->addWidget(b);
	}
	h->addWidget(vline());
	m_sceneName = new QLabel;
	m_sceneName->setStyleSheet("font-weight: 500; padding: 0 6px;");
	h->addWidget(m_sceneName);
	m_status = new QLabel;
	m_status->setTextFormat(Qt::RichText);
	h->addWidget(m_status);
	h->addStretch(1);

	m_shotBtn = iconButton("snapshot", tr("Screenshot of the canvas"), bar);
	m_replayBtn = iconButton("replay", tr("Save the replay buffer"), bar);
	m_vcamBtn = iconButton("vcam", tr("Virtual camera"), bar, true);
	m_studioBtn = iconButton("studio", tr("Studio mode: edit a preview, then send it live"), bar, true);
	auto *gear = iconButton("gear", tr("Settings"), bar);
	for (auto *b : {m_shotBtn, m_replayBtn, m_vcamBtn, m_studioBtn, gear})
		h->addWidget(b);
	h->addWidget(vline());
	h->addSpacing(4);
	m_streamBtn = new QPushButton(Icons::get("live"), tr("Go live"));
	m_streamBtn->setObjectName("streamBtn");
	m_pauseBtn = iconButton("pause", tr("Pause recording"), bar, true);
	m_recBtn = new QPushButton(tr("Record"));
	m_recBtn->setObjectName("recBtn");
	setKind(m_recBtn, "pri");
	h->addWidget(m_streamBtn);
	h->addWidget(m_pauseBtn);
	h->addWidget(m_recBtn);

	connect(m_shotBtn, &QPushButton::clicked, this, &MainWindow::takeScreenshot);
	connect(m_replayBtn, &QPushButton::clicked, this, &MainWindow::saveReplay);
	connect(m_vcamBtn, &QPushButton::clicked, this, &MainWindow::toggleVirtualCam);
	connect(m_studioBtn, &QPushButton::clicked, this, [](bool on) { ENGINE.setStudioMode(on); });
	connect(gear, &QPushButton::clicked, this, [this] { openSettings(); });
	connect(m_streamBtn, &QPushButton::clicked, this, &MainWindow::toggleStream);
	connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::togglePause);
	connect(m_recBtn, &QPushButton::clicked, this, &MainWindow::toggleRecord);
	return bar;
}

// Left panel ------------------------------------------------------------------------------------------

QWidget *MainWindow::buildLeft()
{
	auto *w = new QWidget;
	w->setObjectName("panel");
	w->setAttribute(Qt::WA_StyledBackground);
	w->setMinimumWidth(200);
	auto *v = new QVBoxLayout(w);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(0);

	auto header = [](const QString &t, QPushButton *btn) {
		auto *row = new QWidget;
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(12, 6, 6, 4);
		h->addWidget(panelHeader(t));
		h->addStretch(1);
		h->addWidget(btn);
		return row;
	};
	auto *addScene = iconButton("plus", tr("Add scene"));
	v->addWidget(header(tr("Scenes"), addScene));
	m_scenes = new SceneList;
	m_scenes->setContentsMargins(6, 0, 6, 0);
	auto *sw = new QWidget;
	auto *sl = new QVBoxLayout(sw);
	sl->setContentsMargins(6, 0, 6, 0);
	sl->addWidget(m_scenes);
	v->addWidget(sw, 1);

	auto *tr_ = new QWidget;
	auto *th = new QHBoxLayout(tr_);
	th->setContentsMargins(12, 6, 10, 10);
	th->setSpacing(6);
	auto *tl = new QLabel(tr("Transition"));
	tl->setProperty("mute", true);
	tl->setStyleSheet("font-size: 12px");
	m_transition = new QComboBox;
	m_transition->setAccessibleName(tr("Transition"));
	m_transitionMs = new QSpinBox;
	m_transitionMs->setRange(0, 20000);
	m_transitionMs->setSingleStep(50);
	m_transitionMs->setSuffix(" ms");
	m_transitionMs->setButtonSymbols(QAbstractSpinBox::NoButtons);
	m_transitionMs->setFixedWidth(70);
	th->addWidget(tl);
	th->addWidget(m_transition, 1);
	th->addWidget(m_transitionMs);
	v->addWidget(tr_);
	v->addWidget(hline());

	auto *addSrc = iconButton("plus", tr("Add source"));
	v->addWidget(header(tr("Sources"), addSrc));
	m_sources = new SourceList;
	auto *srw = new QWidget;
	auto *srl = new QVBoxLayout(srw);
	srl->setContentsMargins(6, 0, 6, 10);
	srl->addWidget(m_sources);
	v->addWidget(srw, 2);

	connect(addScene, &QPushButton::clicked, m_scenes, &SceneList::addScene);
	connect(addSrc, &QPushButton::clicked, this, &MainWindow::addSource);
	connect(m_sources, &SourceList::addRequested, this, &MainWindow::addSource);
	connect(m_sources, &SourceList::filtersRequested, this, [this] {
		if (m_rightBox->isVisible())
			m_inspector->showFilters();
		else if (obs_sceneitem_t *it = ENGINE.selectedItem())
			showSourceDialog(obs_sceneitem_get_source(it), true, this);
	});
	auto changeTransition = [this] {
		ENGINE.setTransition(m_transition->currentData().toString(), m_transitionMs->value());
		m_transitionMs->setEnabled(m_transition->currentData().toString() != "cut_transition");
	};
	connect(m_transition, &QComboBox::activated, this, changeTransition);
	connect(m_transitionMs, &QSpinBox::editingFinished, this, changeTransition);
	return w;
}

// Desk: the canvas ------------------------------------------------------------------------------------

QWidget *MainWindow::buildDesk()
{
	m_desk = new QWidget;
	m_desk->setObjectName("desk");
	m_desk->setAttribute(Qt::WA_StyledBackground);
	auto *v = new QVBoxLayout(m_desk);
	v->setContentsMargins(0, 10, 0, 10);
	v->setSpacing(6);

	// The crop-canvas prompt sits above the picture, never over it.
	m_chip = new QFrame;
	m_chip->setObjectName("chip");
	auto *ch = new QHBoxLayout(m_chip);
	ch->setContentsMargins(12, 4, 5, 4);
	ch->setSpacing(8);
	m_chipText = new QLabel;
	m_chipText->setStyleSheet("font-size: 12px");
	m_chipBtn = new QPushButton(Icons::get("crop"), tr("Crop canvas to feed"));
	m_chipBtn->setStyleSheet("font-size: 12px; padding: 3px 9px;");
	ch->addWidget(m_chipText);
	ch->addWidget(m_chipBtn);
	addShadow(m_chip, 18, 4);
	auto *chipRow = new QHBoxLayout;
	chipRow->addStretch(1);
	chipRow->addWidget(m_chip);
	chipRow->addStretch(1);
	v->addLayout(chipRow);
	connect(m_chipBtn, &QPushButton::clicked, this, [this] { m_undoable ? undoCrop() : cropCanvas(); });

	auto *row = new QHBoxLayout;
	row->setSpacing(0);
	auto *pv = new QVBoxLayout;
	m_previewLabel = new QLabel(tr("Preview"));
	m_previewLabel->setAlignment(Qt::AlignCenter);
	m_previewLabel->setProperty("mute", true);
	pv->addWidget(m_previewLabel);
	m_preview = new Preview(Preview::Edit);
	pv->addWidget(m_preview, 1);
	row->addLayout(pv, 1);

	m_liveCol = new QWidget;
	auto *lc = new QVBoxLayout(m_liveCol);
	lc->addStretch(1);
	auto *send = new QPushButton(Icons::get("arrowright"), tr("Send live"));
	send->setToolTip(tr("Show the preview to viewers, with the transition"));
	lc->addWidget(send);
	lc->addStretch(1);
	row->addWidget(m_liveCol);
	connect(send, &QPushButton::clicked, this, [] { ENGINE.sendPreviewLive(); });

	m_programBox = new QWidget;
	auto *prv = new QVBoxLayout(m_programBox);
	prv->setContentsMargins(0, 0, 0, 0);
	m_programLabel = new QLabel(tr("Live"));
	m_programLabel->setAlignment(Qt::AlignCenter);
	m_programLabel->setStyleSheet(QString("color: %1; font-weight: 600").arg(Theme::current().rec.name()));
	prv->addWidget(m_programLabel);
	m_program = new Preview(Preview::Program);
	prv->addWidget(m_program, 1);
	row->addWidget(m_programBox, 1);
	v->addLayout(row, 1);

	// A small floating bar under the picture.
	auto *bar = new QFrame;
	bar->setObjectName("floatbar");
	auto *bh = new QHBoxLayout(bar);
	bh->setContentsMargins(3, 3, 3, 3);
	bh->setSpacing(2);
	auto *crop = iconButton("crop", tr("Crop canvas to the selected feed"));
	auto *fit = iconButton("fit", tr("Fit the selected feed to the canvas"));
	auto *center = iconButton("center", tr("Center the selected feed"));
	m_canvasBtn = new QPushButton;
	m_fpsBtn = new QPushButton;
	for (auto *b : {m_canvasBtn, m_fpsBtn}) {
		setKind(b, "mt");
		b->setStyleSheet("font-size: 12px");
	}
	bh->addWidget(crop);
	bh->addWidget(fit);
	bh->addWidget(center);
	bh->addWidget(vline());
	bh->addWidget(m_canvasBtn);
	bh->addWidget(m_fpsBtn);
	addShadow(bar, 22, 6);
	auto *barRow = new QHBoxLayout;
	barRow->addStretch(1);
	barRow->addWidget(bar);
	barRow->addStretch(1);
	v->addLayout(barRow);
	connect(crop, &QPushButton::clicked, this, &MainWindow::cropCanvas);
	connect(fit, &QPushButton::clicked, this, [] {
		if (obs_sceneitem_t *it = ENGINE.selectedItem())
			ItemOps::fit(it);
	});
	connect(center, &QPushButton::clicked, this, [] {
		if (obs_sceneitem_t *it = ENGINE.selectedItem())
			ItemOps::center(it);
	});
	connect(m_canvasBtn, &QPushButton::clicked, this, [this] { openSettings(tr("Video")); });
	connect(m_fpsBtn, &QPushButton::clicked, this, [this] { openSettings(tr("Video")); });

	connect(m_preview, &Preview::contextMenu, this, [this](obs_sceneitem_t *item, const QPoint &at) {
		QMenu m(this);
		if (item) {
			m.addAction(Icons::get("edit"), tr("Rename"), m_sources, &SourceList::renameSelected);
			m.addAction(Icons::get("filters"), tr("Filters"), this, [this] { m_inspector->showFilters(); });
			m.addSeparator();
			SourceList::fillItemMenu(m, item, this);
			m.addSeparator();
		}
		m.addAction(Icons::get("plus"), tr("Add source"), this, &MainWindow::addSource);
		m.addAction(Icons::get("area"), tr("Add an area of the screen"), this, &MainWindow::addArea);
		m.exec(at);
	});
	connect(m_preview, &Preview::itemDoubleClicked, this, [this](obs_sceneitem_t *item) {
		if (!m_rightBox->isVisible())
			showSourceDialog(obs_sceneitem_get_source(item), false, this);
	});
	connect(m_preview, &Preview::deletePressed, this, [this] { ItemOps::confirmRemove(ENGINE.selectedItem(), this); });
	return m_desk;
}

// Menus -----------------------------------------------------------------------------------------------

void MainWindow::buildMenus()
{
	auto menuFor = [this](const QString &name) {
		auto *b = findChild<QToolButton *>("menu_" + name);
		auto *m = new QMenu(b);
		b->setMenu(m);
		return m;
	};

	QMenu *file = menuFor(tr("File"));
	file->addAction(Icons::get("folder"), tr("Open recordings folder"), this, [] {
		QDesktopServices::openUrl(QUrl::fromLocalFile(CFG.str("record", "path")));
	});
	file->addAction(tr("Show last recording"), this, [] {
		const QString f = OUTPUTS.lastRecording();
		QDesktopServices::openUrl(QUrl::fromLocalFile(f.isEmpty() ? CFG.str("record", "path") : QFileInfo(f).absolutePath()));
	});
	file->addSeparator();
	addAction(file->addAction(Icons::get("gear"), tr("Settings"), QKeySequence("Ctrl+,"), this, [this] { openSettings(); }));
	file->addSeparator();
	file->addAction(tr("Exit"), this, &QWidget::close);

	QMenu *edit = menuFor(tr("Edit"));
	connect(edit, &QMenu::aboutToShow, this, [this, edit] {
		edit->clear();
		obs_sceneitem_t *it = ENGINE.selectedItem();
		auto *rn = edit->addAction(Icons::get("edit"), tr("Rename source"), m_sources, &SourceList::renameSelected);
		rn->setEnabled(it);
		auto *fl = edit->addAction(Icons::get("filters"), tr("Filters"), this, [this] { m_inspector->showFilters(); });
		fl->setEnabled(it);
		edit->addSeparator();
		if (it)
			SourceList::fillItemMenu(*edit, it, this);
		else
			edit->addAction(tr("Select a source first"))->setEnabled(false);
	});

	m_viewMenu = menuFor(tr("View"));
	connect(m_viewMenu, &QMenu::aboutToShow, this, [this] {
		QMenu *m = m_viewMenu;
		m->clear();
		auto *layout = m->addMenu(tr("Layout"));
		for (auto [key, label] : {std::pair{"classic", tr("Classic")}, {"compact", tr("Compact")}, {"studio", tr("Studio")}}) {
			const QString k = key;
			auto *a = layout->addAction(label, this, [this, k] {
				CFG.setStr("general", "layout", k);
				CFG.save();
				applyLayout();
			});
			a->setCheckable(true);
			a->setChecked(CFG.str("general", "layout") == k);
		}
		auto *theme = m->addMenu(tr("Theme"));
		for (auto [key, label] : {std::pair{"dark", tr("Dark")}, {"light", tr("Light")}}) {
			const QString k = key;
			auto *a = theme->addAction(label, this, [this, k] {
				CFG.setStr("general", "theme", k);
				CFG.save();
				applyLook();
			});
			a->setCheckable(true);
			a->setChecked(CFG.str("general", "theme") == k);
		}
		m->addSeparator();
		auto *proj = m->addMenu(Icons::get("screen"), tr("Show what's live in a window"));
		proj->addAction(tr("In a window"), this, [this] { openProjector(-1); });
		const auto screens = QGuiApplication::screens();
		for (int i = 0; i < screens.size(); i++)
			proj->addAction(tr("Full screen on display %1 (%2 × %3)").arg(i + 1).arg(screens[i]->size().width()).arg(screens[i]->size().height()),
					this, [this, i] { openProjector(i); });
		m->addSeparator();
		auto *panels = m->addAction(tr("Show the right panel"), this, [this](bool on) { m_rightBox->setVisible(on); });
		panels->setCheckable(true);
		panels->setChecked(m_rightBox->isVisible());
		auto *mix = m->addAction(tr("Show the audio mixer"), this, [this](bool on) { m_mixer->setVisible(on); });
		mix->setCheckable(true);
		mix->setChecked(m_mixer->isVisible());
		m->addAction(tr("Reset the panels"), this, [this] {
			m_hsplit->setSizes({240, width() - 540, 300});
			m_vsplit->setSizes({height() - 260, 170});
			m_rightBox->show();
			m_mixer->show();
		});
	});

	QMenu *scene = menuFor(tr("Scene"));
	scene->addAction(Icons::get("plus"), tr("Add scene"), m_scenes, &SceneList::addScene);
	scene->addAction(Icons::get("copy"), tr("Duplicate scene"), this, [] { ENGINE.duplicateScene(ENGINE.editScene()); });
	scene->addAction(Icons::get("edit"), tr("Rename scene"), m_scenes, &SceneList::renameCurrent);
	scene->addAction(Icons::get("trash"), tr("Remove scene"), m_scenes, &SceneList::removeCurrent);
	scene->addSeparator();
	// Menus live on buttons, so their shortcuts are added to the window too.
	addAction(scene->addAction(Icons::get("plus"), tr("Add source"), QKeySequence("Ctrl+N"), this, &MainWindow::addSource));
	addAction(scene->addAction(Icons::get("area"), tr("Add an area of the screen"), QKeySequence("Ctrl+Shift+A"), this, &MainWindow::addArea));

	QMenu *tools = menuFor(tr("Tools"));
	connect(tools, &QMenu::aboutToShow, this, [this, tools] {
		tools->clear();
		tools->addAction(Icons::get("snapshot"), tr("Screenshot of the canvas"), this, &MainWindow::takeScreenshot);
		tools->addAction(Icons::get("crop"), tr("Crop canvas to feed"), this, &MainWindow::cropCanvas)->setEnabled(ENGINE.selectedItem());
		tools->addSeparator();
		auto *rb = tools->addAction(Icons::get("replay"), OUTPUTS.replayActive() ? tr("Stop the replay buffer") : tr("Start the replay buffer"),
					    this, [this] {
						    QString err;
						    if (OUTPUTS.replayActive())
							    OUTPUTS.stopReplay();
						    else if (!OUTPUTS.startReplay(&err))
							    QMessageBox::warning(this, tr("Replay buffer"), err);
					    });
		(void)rb;
		tools->addAction(Icons::get("replay"), tr("Save the replay"), this, &MainWindow::saveReplay)->setEnabled(OUTPUTS.replayActive());
		auto *vc = tools->addAction(Icons::get("vcam"), OUTPUTS.virtualCamActive() ? tr("Stop the virtual camera") : tr("Start the virtual camera"),
					    this, &MainWindow::toggleVirtualCam);
		vc->setEnabled(OUTPUTS.virtualCamAvailable());
		auto *st = tools->addAction(Icons::get("studio"), tr("Studio mode"), this, [](bool on) { ENGINE.setStudioMode(on); });
		st->setCheckable(true);
		st->setChecked(ENGINE.studioMode());
	});

	QMenu *help = menuFor(tr("Help"));
	help->addAction(Icons::get("help"), tr("Welcome tour"), this, [this] { runWelcome(false); });
	help->addAction(tr("User guide"), this, [] { QDesktopServices::openUrl(QUrl(kGuide)); });
	help->addAction(Icons::get("folder"), tr("Open log folder"), this, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(Config::file("logs"))); });
	help->addSeparator();
	help->addAction(tr("Check for updates"), this, [] { QDesktopServices::openUrl(QUrl(kReleases)); });
	help->addAction(tr("About CocoaCandid"), this, [this] {
		QMessageBox box(this);
		box.setWindowTitle(tr("About CocoaCandid"));
		box.setIconPixmap(Icons::app(CFG.str("general", "icon")).pixmap(64, 64));
		box.setText(QString("<b>CocoaCandid %1</b>").arg(CANDID_VERSION));
		box.setInformativeText(tr("Record and stream, without the complexity. Part of CocoaSuite.<br><br>Built on libobs %1, the engine of OBS Studio. "
					  "Free software under the GNU GPL, version 2 or later. "
					  "<a href=\"https://github.com/CocoaSuite/CocoaCandid\">Source code</a>")
					       .arg(obs_get_version_string()));
		box.exec();
	});

	// Scene keys: F1 to F12 while the window has focus.
	for (int i = 0; i < 12; i++) {
		auto *sc = new QShortcut(QKeySequence(Qt::Key(Qt::Key_F1 + i)), this);
		connect(sc, &QShortcut::activated, this, [i] {
			if (i < ENGINE.scenes().size())
				ENGINE.showScene(ENGINE.scenes()[i]);
		});
	}
}

// Footer ----------------------------------------------------------------------------------------------

void MainWindow::buildFooter()
{
	auto *sb = statusBar();
	sb->setSizeGripEnabled(false);
	sb->setFixedHeight(26);
	m_cpu = new QLabel;
	m_fps = new QLabel;
	m_render = new QLabel;
	m_dropped = new QLabel;
	m_bitrate = new QLabel;
	m_message = new QLabel;
	m_message->setTextFormat(Qt::RichText);
	m_message->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_message->setOpenExternalLinks(true);
	m_encoder = new QLabel;
	m_disk = new QLabel;
	for (auto *l : {m_cpu, m_fps, m_render, m_dropped, m_bitrate})
		sb->addWidget(l);
	sb->addWidget(m_message, 1);
	sb->addPermanentWidget(m_encoder);
	sb->addPermanentWidget(m_disk);

	m_statsTimer = new QTimer(this);
	connect(m_statsTimer, &QTimer::timeout, this, &MainWindow::refreshStats);
	m_clock = new QTimer(this);
	m_clock->setInterval(500);
	connect(m_clock, &QTimer::timeout, this, &MainWindow::refreshOutputs);
	m_undoTimer = new QTimer(this);
	m_undoTimer->setSingleShot(true);
	m_undoTimer->setInterval(10000);
	connect(m_undoTimer, &QTimer::timeout, this, [this] {
		m_undoable = false;
		refreshCropChip();
	});
}

void MainWindow::notify(const QString &text, const QString &openPath)
{
	QString html = text.toHtmlEscaped();
	if (!openPath.isEmpty())
		html += QString("  <a href=\"%1\" style=\"color:%2\">%3</a>")
				.arg(QUrl::fromLocalFile(QFileInfo(openPath).absolutePath()).toString(), Theme::current().sel.name(), tr("Show in folder"));
	m_message->setText(html);
	QTimer::singleShot(12000, m_message, [this, html] {
		if (m_message->text() == html)
			m_message->clear();
	});
}

// Startup ---------------------------------------------------------------------------------------------

void MainWindow::engineReady()
{
	m_preview->setScene(ENGINE.editScene());
	m_program->setScene(ENGINE.programScene());
	m_preview->createDisplay();
	m_program->createDisplay();
	connect(&ENGINE, &Engine::editSceneChanged, this, [this] {
		m_preview->setScene(ENGINE.editScene());
		m_sceneName->setText(QString::fromUtf8(obs_source_get_name(ENGINE.programScene())));
		refreshCropChip();
	});
	connect(&ENGINE, &Engine::programChanged, this, [this] {
		m_sceneName->setText(QString::fromUtf8(obs_source_get_name(ENGINE.programScene())));
	});
	connect(&ENGINE, &Engine::scenesChanged, this, [this] {
		m_sceneName->setText(QString::fromUtf8(obs_source_get_name(ENGINE.programScene())));
	});
	connect(&ENGINE, &Engine::studioModeChanged, this, [this](bool on) {
		m_studioBtn->setChecked(on);
		m_programBox->setVisible(on);
		m_liveCol->setVisible(on);
		m_previewLabel->setVisible(on);
		CFG.setStr("general", "layout", on ? "studio" : (CFG.str("general", "layout") == "studio" ? "classic" : CFG.str("general", "layout")));
		CFG.save();
	});
	connect(&ENGINE, &Engine::selectionChanged, this, &MainWindow::refreshCropChip);
	connect(&ENGINE, &Engine::itemTransformed, this, &MainWindow::refreshCropChip);
	connect(&ENGINE, &Engine::canvasChanged, this, [this] {
		const QSize c = ENGINE.canvasSize();
		m_canvasBtn->setText(QString("%1 × %2").arg(c.width()).arg(c.height()));
		m_fpsBtn->setText(QString("%1 fps").arg(ENGINE.fps(), 0, 'g', 4));
		refreshCropChip();
	});
	emit ENGINE.canvasChanged();
	connect(&OUTPUTS, &Outputs::stateChanged, this, &MainWindow::refreshOutputs);
	connect(&OUTPUTS, &Outputs::error, this, [this](const QString &m) { QMessageBox::warning(this, "CocoaCandid", m); });
	connect(&OUTPUTS, &Outputs::recordingSaved, this, [this](const QString &p) { notify(tr("Recording saved."), p); });
	connect(&OUTPUTS, &Outputs::replaySaved, this, [this](const QString &p) { notify(tr("Replay saved."), p); });

	m_scenes->refresh();
	m_sources->refresh();
	m_mixer->refresh();
	refreshTransitions();
	m_sceneName->setText(QString::fromUtf8(obs_source_get_name(ENGINE.programScene())));
	registerHotkeys();
	applyLayout();
	refreshOutputs();
	m_statsTimer->start(1000);
	m_clock->start();
	if (CFG.flag("record", "replay")) {
		QString err;
		OUTPUTS.startReplay(&err);
	}
	m_encoder->setText(OUTPUTS.videoEncoders().isEmpty() ? QString() : tr("Encoder: %1").arg([] {
		QString id = CFG.str("record", "encoder");
		if (id.isEmpty())
			id = Outputs::bestEncoder();
		return QString::fromUtf8(obs_encoder_get_display_name(id.toUtf8().constData()));
	}()));
}

void MainWindow::refreshTransitions()
{
	const QSignalBlocker b(m_transition);
	m_transition->clear();
	for (const auto &[id, name] : ENGINE.transitionTypes())
		m_transition->addItem(name, id);
	const int i = m_transition->findData(ENGINE.transitionId());
	m_transition->setCurrentIndex(std::max(0, i));
	m_transitionMs->setValue(ENGINE.transitionMs());
	m_transitionMs->setEnabled(ENGINE.transitionId() != "cut_transition");
}

void MainWindow::registerHotkeys()
{
	QVector<Hotkeys::Action> a = {
		{"record", tr("Start or stop recording"), [this] { toggleRecord(); }},
		{"pause", tr("Pause or resume recording"), [this] { togglePause(); }},
		{"stream", tr("Go live or end the stream"), [this] { toggleStream(); }},
		{"replay", tr("Save the replay"), [this] { saveReplay(); }},
		{"screenshot", tr("Screenshot of the canvas"), [this] { takeScreenshot(); }},
		{"mute_mic", tr("Mute or unmute the microphone"), [] {
			 if (obs_source_t *m = ENGINE.micAudio())
				 obs_source_set_muted(m, !obs_source_muted(m));
		 }},
		{"send_live", tr("Studio mode: send the preview live"), [] { ENGINE.sendPreviewLive(); }},
	};
	for (int i = 0; i < 9; i++)
		a.append({QString("scene_%1").arg(i + 1), tr("Switch to scene %1").arg(i + 1), [i] {
				  if (i < ENGINE.scenes().size())
					  ENGINE.showScene(ENGINE.scenes()[i]);
			  }});
	Hotkeys::get().registerAll(a);
}

void MainWindow::runWelcome(bool hello)
{
	auto ring = [](QWidget *w) { return [w] { return QList<QWidget *>{w}; }; };
	const QVector<Onboard::Step> steps = {
		{tr("Scenes and sources"),
		 tr("A scene is a layout you switch to with one click or F1 to F12. Add a screen, an app window or an area you draw as a source, then drag to reorder."),
		 ring(m_left)},
		{tr("Your canvas"),
		 tr("This is exactly what gets recorded or streamed. Drag and resize feeds, and Crop canvas to feed makes the canvas match a feed in one click."),
		 ring(m_desk)},
		{tr("Everything the source can do"),
		 tr("The selected source’s settings, position and crop appear here. Filters, next to Properties, add green screen, blur, sharpen and more."),
		 ring(m_rightBox)},
		{tr("Sound"), tr("Every sound gets a meter. Click the speaker to mute it, and use the menu for listening, sync delay and filters like noise suppression."),
		 ring(m_mixer)},
		{tr("Record and go live"),
		 tr("Record saves to this computer. Go live streams to Twitch, YouTube or any RTMP server. Both can run at once, on your graphics card."),
		 [this] { return QList<QWidget *>{m_streamBtn, m_recBtn}; }},
		{tr("Make it yours"), tr("This is the Classic layout. Prefer something else? Pick Compact or Studio now, or any time in Settings, General."), nullptr},
	};
	// The tour points at the Classic layout; the one picked at the end applies afterwards.
	const QString before = CFG.str("general", "layout");
	CFG.setStr("general", "layout", "classic");
	applyLayout();
	Onboard::run(this, steps, [this, before](const QString &layout) {
		CFG.setStr("general", "layout", layout.isEmpty() ? before : layout);
		CFG.save();
		applyLayout();
	}, hello);
}

// Layout and look -------------------------------------------------------------------------------------

void MainWindow::applyLayout()
{
	const QString l = CFG.str("general", "layout");
	const bool compact = l == "compact", studio = l == "studio";
	m_rightBox->setVisible(!compact);
	m_mixer->setCompact(compact);
	if (ENGINE.running() && ENGINE.studioMode() != studio)
		ENGINE.setStudioMode(studio);
	m_programBox->setVisible(studio);
	m_liveCol->setVisible(studio);
	m_previewLabel->setVisible(studio);
	m_studioBtn->setChecked(studio);
}

void MainWindow::applyLook()
{
	Theme::apply(CFG.str("general", "theme"));
	const QIcon icon = Icons::app(CFG.str("general", "icon"));
	qApp->setWindowIcon(icon);
	setWindowIcon(icon);
	if (auto *logo = findChild<QLabel *>("logo"))
		logo->setPixmap(icon.pixmap(22, 22));
	const Theme &t = Theme::current();
	m_chip->setStyleSheet(QString("QFrame#chip { background: %1; border: 1px solid %2; border-radius: 8px; } QFrame#chip QLabel { color: %3; }")
				      .arg(t.panel.name(), t.line.name(), t.dark ? "#c9c9c9" : "#444"));
	if (auto *bar = findChild<QFrame *>("floatbar"))
		bar->setStyleSheet(QString("QFrame#floatbar { background: %1; border: 1px solid %2; border-radius: 8px; }").arg(t.panel.name(), t.line.name()));
	// Every icon was drawn in the old colours; rebuilding the lists repaints them.
	if (ENGINE.running()) {
		m_scenes->refresh();
		m_sources->refresh();
	}
	refreshOutputs();
}

// Outputs ---------------------------------------------------------------------------------------------

void MainWindow::refreshOutputs()
{
	if (!m_recBtn)
		return;
	const Theme &t = Theme::current();
	const bool rec = OUTPUTS.recording(), live = OUTPUTS.streaming(), starting = OUTPUTS.streamStarting();

	m_recBtn->setText(rec ? tr("Stop") : tr("Record"));
	m_recBtn->setIcon(Icons::tinted(rec ? "stop" : "record", rec ? QColor(Qt::white) : (OUTPUTS.recording() ? t.rec : t.rec)));
	setKind(m_recBtn, rec ? "rec" : "pri");
	m_pauseBtn->setVisible(rec);
	m_pauseBtn->setChecked(OUTPUTS.paused());
	m_pauseBtn->setIcon(Icons::get(OUTPUTS.paused() ? "play" : "pause"));
	m_pauseBtn->setToolTip(OUTPUTS.paused() ? tr("Resume recording") : tr("Pause recording"));

	m_streamBtn->setText(starting ? tr("Connecting") : live ? tr("End stream") : tr("Go live"));
	m_streamBtn->setIcon(live ? Icons::tinted("live", Qt::white) : Icons::get("live"));
	setKind(m_streamBtn, live ? "rec" : "");
	m_streamBtn->setEnabled(!starting);

	m_replayBtn->setEnabled(OUTPUTS.replayActive());
	m_replayBtn->setToolTip(OUTPUTS.replayActive() ? tr("Save the last %1 seconds").arg(CFG.num("record", "replay_sec"))
						     : tr("The replay buffer is off (Settings, Record)"));
	m_vcamBtn->setChecked(OUTPUTS.virtualCamActive());
	m_vcamBtn->setEnabled(OUTPUTS.virtualCamAvailable() || OUTPUTS.virtualCamActive());

	QStringList parts;
	const QString pill = "<span style=\"color:#ff8b8f; font-weight:600\">&#9679; %1</span>";
	if (live)
		parts << pill.arg(OUTPUTS.reconnecting() ? tr("RECONNECTING") : tr("LIVE %1").arg(formatTime(OUTPUTS.streamSeconds())));
	if (rec)
		parts << pill.arg((OUTPUTS.paused() ? tr("PAUSED %1") : tr("REC %1")).arg(formatTime(OUTPUTS.recordSeconds())));
	if (parts.isEmpty())
		parts << QString("<span style=\"color:%1\"><span style=\"color:%2\">&#9679;</span> %3</span>")
				 .arg(t.mute.name(), t.ok.name(), starting ? tr("Connecting") : tr("Ready"));
	m_status->setText(parts.join("&nbsp;&nbsp;&nbsp;"));
}

void MainWindow::toggleRecord()
{
	if (OUTPUTS.recording()) {
		OUTPUTS.stopRecording();
		return;
	}
	QString err;
	if (!OUTPUTS.startRecording(&err))
		QMessageBox::warning(this, tr("Record"), err);
}

void MainWindow::togglePause()
{
	if (!OUTPUTS.recording())
		return;
	if (!OUTPUTS.setPaused(!OUTPUTS.paused()))
		m_pauseBtn->setChecked(OUTPUTS.paused());
}

void MainWindow::toggleStream()
{
	if (OUTPUTS.streaming()) {
		if (CFG.flag("general", "confirm_stop_stream") &&
		    QMessageBox::question(this, tr("End stream"), tr("End the stream now?")) != QMessageBox::Yes)
			return;
		OUTPUTS.stopStreaming();
		return;
	}
	QString err;
	if (!OUTPUTS.startStreaming(&err)) {
		if (QMessageBox::warning(this, tr("Go live"), err, QMessageBox::Ok | QMessageBox::Open) == QMessageBox::Open)
			openSettings(tr("Stream"));
	}
}

void MainWindow::toggleVirtualCam()
{
	if (OUTPUTS.virtualCamActive()) {
		OUTPUTS.stopVirtualCam();
		return;
	}
	QString err;
	if (!OUTPUTS.startVirtualCam(&err)) {
		m_vcamBtn->setChecked(false);
		QMessageBox::warning(this, tr("Virtual camera"), err);
	}
}

void MainWindow::saveReplay()
{
	if (OUTPUTS.replayActive())
		OUTPUTS.saveReplay();
}

void MainWindow::takeScreenshot()
{
	const QImage img = ENGINE.screenshot();
	if (img.isNull()) {
		notify(tr("The screenshot could not be taken."));
		return;
	}
	QString dir = CFG.str("record", "path");
	QDir().mkpath(dir);
	const QString path = QDir(dir).filePath(QDateTime::currentDateTime().toString("'Screenshot' yyyy-MM-dd HH-mm-ss'.png'"));
	if (img.save(path))
		notify(tr("Screenshot saved."), path);
	else
		notify(tr("The screenshot could not be saved to the recordings folder."));
}

// Sources and the canvas ------------------------------------------------------------------------------

void MainWindow::addSource()
{
	AddSourceDialog d(this);
	d.exec();
	if (d.wantsArea())
		QTimer::singleShot(150, this, &MainWindow::addArea);
}

void MainWindow::addArea()
{
	// The window steps aside so it isn't in the way of the screen being picked.
	const bool wasMin = isMinimized();
	showMinimized();
	QTimer::singleShot(350, this, [this, wasMin] {
		AreaPicker::start([this, wasMin](QScreen *screen, const QRect &r) {
			if (!wasMin)
				showNormal();
			raise();
			activateWindow();
			if (!screen)
				return;
			QString err;
			if (!AreaPicker::addArea(screen, r, &err) && !err.isEmpty())
				QMessageBox::warning(this, tr("Area"), err);
		});
	});
}

static QRectF visibleBox(obs_sceneitem_t *item)
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

void MainWindow::refreshCropChip()
{
	if (m_undoable) {
		m_chip->show();
		return;
	}
	obs_sceneitem_t *it = ENGINE.selectedItem();
	bool show = false;
	if (it && OUTPUTS.anyActive() == false) {
		obs_source_t *s = obs_sceneitem_get_source(it);
		const QString icon = SourceList::iconFor(s);
		const bool feed = icon == "screen" || icon == "window" || icon == "area" || icon == "camera" || icon == "game";
		const QRectF b = visibleBox(it);
		const QSize c = ENGINE.canvasSize();
		if (feed && obs_source_get_width(s) > 0 && b.width() >= 16 && b.height() >= 16 &&
		    (std::abs(b.width() - c.width()) > 2 || std::abs(b.height() - c.height()) > 2)) {
			show = true;
			const QString name = QString::fromUtf8(obs_source_get_name(s));
			m_chipText->setText(b.width() < c.width() || b.height() < c.height()
						    ? tr("The canvas is bigger than “%1”").arg(name)
						    : tr("“%1” is bigger than the canvas").arg(name));
			m_chipBtn->setText(tr("Crop canvas to feed"));
			m_chipBtn->setIcon(Icons::get("crop"));
		}
	}
	m_chip->setVisible(show);
}

void MainWindow::cropCanvas()
{
	obs_sceneitem_t *it = ENGINE.selectedItem();
	if (!it) {
		notify(tr("Select a feed first."));
		return;
	}
	const QSize base = ENGINE.canvasSize(), out = ENGINE.outputSize();
	const QRectF b = visibleBox(it);
	QString err;
	if (!ENGINE.cropCanvasTo(it, &err)) {
		QMessageBox::information(this, tr("Crop canvas to feed"), err);
		return;
	}
	m_undoable = true;
	m_undoBase = base;
	m_undoOut = out;
	m_undoShift = b.topLeft();
	m_chipText->setText(tr("Canvas now fits “%1” exactly").arg(QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(it)))));
	m_chipBtn->setText(tr("Undo"));
	m_chipBtn->setIcon(QIcon());
	m_chip->show();
	m_undoTimer->start();
}

void MainWindow::undoCrop()
{
	if (!m_undoable)
		return;
	m_undoable = false;
	m_undoTimer->stop();
	CFG.setNum("video", "base_w", m_undoBase.width());
	CFG.setNum("video", "base_h", m_undoBase.height());
	CFG.setNum("video", "out_w", m_undoOut.width());
	CFG.setNum("video", "out_h", m_undoOut.height());
	QString err;
	if (!ENGINE.resetVideo(&err)) {
		QMessageBox::information(this, tr("Undo"), err);
		return;
	}
	CFG.save();
	float d[2] = {float(m_undoShift.x()), float(m_undoShift.y())};
	obs_scene_enum_items(ENGINE.editSceneObj(), [](obs_scene_t *, obs_sceneitem_t *it, void *p) {
		const float *d = static_cast<const float *>(p);
		vec2 pos;
		obs_sceneitem_get_pos(it, &pos);
		pos.x += d[0];
		pos.y += d[1];
		obs_sceneitem_set_pos(it, &pos);
		return true;
	}, d);
	ENGINE.save();
	refreshCropChip();
}

// Stats -----------------------------------------------------------------------------------------------

void MainWindow::refreshStats()
{
	const OutputStats s = OUTPUTS.stats();
	m_cpu->setText(tr("CPU %1%").arg(s.cpu, 0, 'f', 1));
	m_fps->setText(tr("%1 fps").arg(s.fps, 0, 'f', 2));
	m_render->setText(tr("Render %1 ms").arg(s.renderMs, 0, 'f', 1));
	if (OUTPUTS.streaming()) {
		const double pct = s.totalStream ? 100.0 * s.droppedStream / s.totalStream : 0;
		m_dropped->setText(tr("Dropped %1 (%2%)").arg(s.droppedStream).arg(pct, 0, 'f', 1));
		m_bitrate->setText(tr("%1 kb/s").arg(int(s.streamKbps)));
		m_bitrate->show();
	} else {
		const double pct = s.totalFrames ? 100.0 * s.laggedFrames / s.totalFrames : 0;
		m_dropped->setText(tr("Lagged %1 (%2%)").arg(s.laggedFrames).arg(pct, 0, 'f', 1));
		m_bitrate->setVisible(OUTPUTS.recording());
		m_bitrate->setText(tr("%1 MB").arg(s.recordMb, 0, 'f', 1));
	}
	QStorageInfo disk(CFG.str("record", "path"));
	if (disk.isValid())
		m_disk->setText(tr("%1 GB free").arg(disk.bytesAvailable() / (1024.0 * 1024 * 1024), 0, 'f', 0));
}

// Settings, projector, closing ------------------------------------------------------------------------

void MainWindow::openSettings(const QString &page)
{
	SettingsDialog d(this, page);
	connect(&d, &SettingsDialog::lookChanged, this, [this] {
		applyLook();
		applyLayout();
	});
	connect(&d, &SettingsDialog::showTourRequested, this, [this] { QTimer::singleShot(200, this, [this] { runWelcome(false); }); });
	d.exec();
}

void MainWindow::openProjector(int screen)
{
	auto *w = new QWidget(nullptr, Qt::Window);
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->setWindowTitle(tr("CocoaCandid, live"));
	w->setWindowIcon(windowIcon());
	auto *v = new QVBoxLayout(w);
	v->setContentsMargins(0, 0, 0, 0);
	v->addWidget(new Preview(Preview::Program, w));
	auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), w);
	connect(esc, &QShortcut::activated, w, &QWidget::close);
	const auto screens = QGuiApplication::screens();
	if (screen >= 0 && screen < screens.size()) {
		w->setGeometry(screens[screen]->geometry());
		w->showFullScreen();
	} else {
		w->resize(960, 560);
		w->show();
	}
}

void MainWindow::closeEvent(QCloseEvent *e)
{
	if (OUTPUTS.recording() || OUTPUTS.streaming()) {
		const auto r = QMessageBox::question(this, tr("Quit"), tr("You're still recording or live. Stop and quit?"));
		if (r != QMessageBox::Yes) {
			e->ignore();
			return;
		}
	}
	CFG.setStr("general", "geometry", QString::fromLatin1(saveGeometry().toBase64()));
	CFG.setStr("general", "hsplit", QString::fromLatin1(m_hsplit->saveState().toBase64()));
	CFG.setStr("general", "vsplit", QString::fromLatin1(m_vsplit->saveState().toBase64()));
	CFG.save();
	for (QWidget *w : QApplication::topLevelWidgets())
		if (w != this)
			w->close();
	e->accept();
}
