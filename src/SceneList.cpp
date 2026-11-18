#include "SceneList.hpp"
#include "Engine.hpp"
#include "Icons.hpp"
#include "Style.hpp"

#include <QContextMenuEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMessageBox>

SceneList::SceneList(QWidget *parent) : QListWidget(parent)
{
	setDragDropMode(QAbstractItemView::InternalMove);
	setDefaultDropAction(Qt::MoveAction);
	setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
	setIconSize(QSize(16, 16));
	setSpacing(0);
	setUniformItemSizes(true);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	connect(this, &QListWidget::currentRowChanged, this, [this](int row) {
		if (m_updating || row < 0 || row >= ENGINE.scenes().size())
			return;
		ENGINE.showScene(ENGINE.scenes()[row]);
	});
	connect(this, &QListWidget::itemChanged, this, [this](QListWidgetItem *it) {
		if (m_updating)
			return;
		const int row = this->row(it);
		if (row < 0 || row >= ENGINE.scenes().size())
			return;
		obs_source_t *s = ENGINE.scenes()[row];
		if (!ENGINE.rename(s, it->text())) {
			QMessageBox::information(this, tr("Rename scene"), tr("That name is already used, or empty."));
			refresh();
		}
	});
	connect(&ENGINE, &Engine::scenesChanged, this, &SceneList::refresh);
	connect(&ENGINE, &Engine::programChanged, this, &SceneList::refresh);
	connect(&ENGINE, &Engine::editSceneChanged, this, &SceneList::refresh);
	connect(&ENGINE, &Engine::studioModeChanged, this, &SceneList::refresh);
}

void SceneList::refresh()
{
	m_updating = true;
	const auto &scenes = ENGINE.scenes();
	while (count() > scenes.size())
		delete takeItem(count() - 1);
	for (int i = 0; i < scenes.size(); i++) {
		QListWidgetItem *it = item(i);
		if (!it) {
			it = new QListWidgetItem(this);
			it->setFlags(it->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);
		}
		const QString name = QString::fromUtf8(obs_source_get_name(scenes[i]));
		if (it->text() != name)
			it->setText(name);
		const bool live = ENGINE.studioMode() && scenes[i] == ENGINE.programScene();
		it->setIcon(live ? Icons::tinted("record", Theme::current().rec) : Icons::get("scene"));
		it->setToolTip(i < 12 ? tr("F%1 switches to this scene").arg(i + 1) : QString());
		it->setSizeHint(QSize(0, 34));
		if (scenes[i] == ENGINE.editScene())
			setCurrentRow(i);
	}
	m_updating = false;
}

void SceneList::dropEvent(QDropEvent *e)
{
	const int from = currentRow();
	QListWidget::dropEvent(e);
	const int to = currentRow();
	if (from >= 0 && to >= 0 && from != to)
		ENGINE.moveScene(from, to);
	else
		refresh();
}

void SceneList::addScene()
{
	obs_source_t *s = ENGINE.addScene(tr("Scene"));
	ENGINE.showScene(s);
	refresh();
	if (QListWidgetItem *it = item(ENGINE.scenes().indexOf(OBSSource(s))))
		editItem(it);
}

void SceneList::renameCurrent()
{
	if (QListWidgetItem *it = currentItem())
		editItem(it);
}

void SceneList::removeCurrent()
{
	const int row = currentRow();
	if (row < 0 || ENGINE.scenes().size() <= 1)
		return;
	obs_source_t *s = ENGINE.scenes()[row];
	const auto r = QMessageBox::question(this, tr("Remove scene"),
					     tr("Remove “%1”? Its sources stay in any other scene that uses them.").arg(obs_source_get_name(s)));
	if (r == QMessageBox::Yes)
		ENGINE.removeScene(s);
}

void SceneList::contextMenuEvent(QContextMenuEvent *e)
{
	QMenu m(this);
	QListWidgetItem *it = itemAt(e->pos());
	m.addAction(Icons::get("plus"), tr("Add scene"), this, &SceneList::addScene);
	if (it) {
		const int row = this->row(it);
		obs_source_t *s = ENGINE.scenes().value(row);
		m.addAction(Icons::get("edit"), tr("Rename"), this, [this, it] { editItem(it); });
		m.addAction(Icons::get("copy"), tr("Duplicate"), this, [s] { ENGINE.duplicateScene(s); });
		m.addSeparator();
		auto *up = m.addAction(Icons::get("up"), tr("Move up"), this, [row] { ENGINE.moveScene(row, row - 1); });
		up->setEnabled(row > 0);
		auto *down = m.addAction(Icons::get("down"), tr("Move down"), this, [row] { ENGINE.moveScene(row, row + 1); });
		down->setEnabled(row < ENGINE.scenes().size() - 1);
		m.addSeparator();
		auto *rm = m.addAction(Icons::get("trash"), tr("Remove"), this, [this, row] {
			setCurrentRow(row);
			removeCurrent();
		});
		rm->setEnabled(ENGINE.scenes().size() > 1);
	}
	m.exec(e->globalPos());
}
