#pragma once

#include <QListWidget>

/** Scenes, in order. Click to switch (or to preview in studio mode), drag to reorder, double-click to rename. */
class SceneList : public QListWidget {
	Q_OBJECT
public:
	explicit SceneList(QWidget *parent = nullptr);
	void refresh();
	void addScene();
	void renameCurrent();
	void removeCurrent();

protected:
	void dropEvent(QDropEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;

private:
	bool m_updating = false;
};
