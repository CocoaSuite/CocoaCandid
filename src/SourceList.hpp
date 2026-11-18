#pragma once

#include <QListWidget>
#include <obs.hpp>

class QMenu;

/** The edited scene's sources, top of the canvas first, with show and lock buttons. Drag to reorder. */
class SourceList : public QListWidget {
	Q_OBJECT
public:
	explicit SourceList(QWidget *parent = nullptr);
	void refresh();
	void renameSelected();

	/** The menu shared by this list and the canvas. */
	static void fillItemMenu(QMenu &m, obs_sceneitem_t *item, QWidget *parent);
	static QString iconFor(obs_source_t *source);

signals:
	void addRequested();
	void filtersRequested();

protected:
	void dropEvent(QDropEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;

private:
	obs_sceneitem_t *itemFor(QListWidgetItem *it) const;
	bool m_updating = false;
};

namespace ItemOps {
void fit(obs_sceneitem_t *item);
void stretch(obs_sceneitem_t *item);
void center(obs_sceneitem_t *item);
void reset(obs_sceneitem_t *item);
void flip(obs_sceneitem_t *item, bool horizontal);
void rotate(obs_sceneitem_t *item, float degrees);
void cropCanvas(obs_sceneitem_t *item, QWidget *parent);
bool confirmRemove(obs_sceneitem_t *item, QWidget *parent);
}
