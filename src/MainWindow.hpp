#pragma once

#include <QMainWindow>
#include <QPointer>
#include <obs.hpp>

class Preview;
class SceneList;
class SourceList;
class Inspector;
class Mixer;
class QLabel;
class QPushButton;
class QToolButton;
class QComboBox;
class QSpinBox;
class QSplitter;
class QFrame;
class QMenu;
class QTimer;

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	MainWindow();
	/** Called once libobs is running: builds the canvas and fills every panel. */
	void engineReady();
	void runWelcome(bool hello);

protected:
	void closeEvent(QCloseEvent *e) override;

private:
	QWidget *buildTopBar();
	QWidget *buildLeft();
	QWidget *buildDesk();
	void buildMenus();
	void buildFooter();
	void registerHotkeys();

	void applyLayout();
	void applyLook();
	void refreshOutputs();
	void refreshCropChip();
	void refreshStats();
	void refreshTransitions();

	void toggleRecord();
	void togglePause();
	void toggleStream();
	void toggleVirtualCam();
	void saveReplay();
	void takeScreenshot();
	void addSource();
	void addArea();
	void cropCanvas();
	void undoCrop();
	void openSettings(const QString &page = {});
	void openProjector(int screen);
	void notify(const QString &text, const QString &openPath = {});

	// Top bar
	QLabel *m_sceneName, *m_status;
	QPushButton *m_streamBtn, *m_recBtn, *m_pauseBtn, *m_shotBtn, *m_replayBtn, *m_vcamBtn, *m_studioBtn;
	QMenu *m_viewMenu = nullptr;

	// Panels
	QSplitter *m_hsplit, *m_vsplit;
	QWidget *m_left, *m_rightBox;
	SceneList *m_scenes;
	SourceList *m_sources;
	QComboBox *m_transition;
	QSpinBox *m_transitionMs;
	Inspector *m_inspector;
	Mixer *m_mixer;

	// Desk
	QWidget *m_desk;
	Preview *m_preview = nullptr, *m_program = nullptr;
	QWidget *m_programBox, *m_liveCol;
	QFrame *m_chip;
	QLabel *m_chipText;
	QPushButton *m_chipBtn;
	QPushButton *m_canvasBtn, *m_fpsBtn;
	QLabel *m_previewLabel, *m_programLabel;

	// Footer
	QLabel *m_cpu, *m_fps, *m_render, *m_dropped, *m_bitrate, *m_encoder, *m_disk, *m_message;
	QTimer *m_statsTimer, *m_clock;

	// Crop undo
	bool m_undoable = false;
	QSize m_undoBase, m_undoOut;
	QPointF m_undoShift;
	QTimer *m_undoTimer;
};
