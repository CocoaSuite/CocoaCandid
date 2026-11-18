#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <QVector>
#include <obs.hpp>
#include <obs-config.h>

// libobs 30.1 renamed the transform calls; Windows builds use a newer libobs than Linux distributions ship.
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 1, 0)
#define candid_get_info obs_sceneitem_get_info2
#define candid_set_info obs_sceneitem_set_info2
#else
#define candid_get_info obs_sceneitem_get_info
#define candid_set_info obs_sceneitem_set_info
#endif

/**
 * Owns libobs: start-up, the canvas, scenes, the transition, the two global audio devices, the
 * selected scene item and saving everything to scenes.json. libobs calls back on its own threads; every
 * callback here is posted to the Qt thread before anything is emitted.
 */
class Engine : public QObject {
	Q_OBJECT
public:
	static Engine &get();

	/** Starts libobs and loads every plugin. Returns false with a reason if the graphics card can't be used. */
	bool start(QString *error);
	void shutdown();
	bool running() const { return m_running; }

	// Canvas
	QSize canvasSize() const;
	QSize outputSize() const;
	double fps() const;
	/** Changes the canvas, output size and frame rate. Fails while recording or streaming. */
	bool resetVideo(QString *error = nullptr);
	/** Makes the canvas the size of an item as it appears now, and moves the item to the top left. */
	bool cropCanvasTo(obs_sceneitem_t *item, QString *error = nullptr);

	// Scenes, in the order the list shows them
	const QVector<OBSSource> &scenes() const { return m_scenes; }
	obs_source_t *programScene() const { return m_program; }
	/** The scene being edited: the preview in studio mode, else what's live. */
	obs_source_t *editScene() const { return m_studio ? m_preview.Get() : m_program.Get(); }
	obs_scene_t *editSceneObj() const { return obs_scene_from_source(editScene()); }
	void showScene(obs_source_t *scene);
	obs_source_t *addScene(const QString &name);
	obs_source_t *duplicateScene(obs_source_t *scene);
	void removeScene(obs_source_t *scene);
	void moveScene(int from, int to);
	bool rename(obs_source_t *source, const QString &name);
	QString uniqueName(const QString &base) const;

	// Studio mode: edit a preview, then send it live with the transition
	bool studioMode() const { return m_studio; }
	void setStudioMode(bool on);
	void sendPreviewLive();

	// Transition
	QString transitionId() const { return m_transitionId; }
	int transitionMs() const { return m_transitionMs; }
	void setTransition(const QString &id, int ms);
	QVector<QPair<QString, QString>> transitionTypes() const;

	// Selection in the edited scene
	obs_sceneitem_t *selectedItem() const;
	void select(obs_sceneitem_t *item);

	// The two always-on devices
	obs_source_t *desktopAudio() const { return m_desktop; }
	obs_source_t *micAudio() const { return m_mic; }
	/** Every source that has sound and is in use, the two devices first. */
	QVector<OBSSource> audioSources() const;

	/** Adds a source to the edited scene and selects it. */
	obs_sceneitem_t *addToScene(obs_source_t *source, bool fitCanvas = false);
	void removeItem(obs_sceneitem_t *item);

	/** A copy of the canvas as it is now, or a null image. */
	QImage screenshot();

	void save();

	/** The first of these source type ids that a loaded plugin provides (versioned ids resolved). */
	static QString firstType(std::initializer_list<const char *> ids);

signals:
	void scenesChanged();
	void programChanged();
	void editSceneChanged();
	void itemsChanged();
	void selectionChanged();
	void itemTransformed();
	void audioSourcesChanged();
	void canvasChanged();
	void studioModeChanged(bool on);

private:
	Engine() = default;
	void load();
	void createDefaults();
	void createAudioDevices(obs_data_t *desktop, obs_data_t *mic);
	void hookScene(obs_source_t *scene);
	void unhookScene(obs_source_t *scene);
	void setTransitionSource();
	void post(std::function<void()> fn);

	static void onSceneSignal(void *data, calldata_t *cd);
	static void onGlobalSignal(void *data, calldata_t *cd);

	bool m_running = false;
	QVector<OBSSource> m_scenes;
	OBSSource m_program, m_preview;
	OBSSource m_transition;
	OBSSource m_desktop, m_mic;
	QString m_transitionId = "fade_transition";
	int m_transitionMs = 300;
	bool m_studio = false;
	bool m_loading = false;
	bool m_pendingItems = false, m_pendingAudio = false, m_pendingTransform = false;
};

#define ENGINE Engine::get()
