#pragma once

#include <QMutex>
#include <QPointF>
#include <QRectF>
#include <QWidget>
#include <atomic>
#include <obs.hpp>

Q_DECLARE_OPAQUE_POINTER(obs_sceneitem_t *)

/**
 * A native child window libobs draws into. In Edit mode it shows one scene and lets you select, move and
 * resize its items with the mouse (corners keep the shape unless Shift is held; edges snap to the canvas).
 * In Program mode it shows exactly what is being recorded or streamed.
 */
class Preview : public QWidget {
	Q_OBJECT
public:
	enum Mode { Edit, Program };
	explicit Preview(Mode mode, QWidget *parent = nullptr);
	~Preview() override;

	void setScene(obs_source_t *scene);
	/** Creates the libobs display once both the engine and the native window exist. */
	void createDisplay();
	QPaintEngine *paintEngine() const override { return nullptr; }

signals:
	void itemDoubleClicked(obs_sceneitem_t *item);
	void contextMenu(obs_sceneitem_t *item, const QPoint &globalPos);
	void deletePressed();

protected:
	void showEvent(QShowEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *) override {}
	bool event(QEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void leaveEvent(QEvent *e) override;

private:
	static void draw(void *data, uint32_t cx, uint32_t cy);
	QPointF toCanvas(const QPointF &widgetPos) const;
	double canvasToWidget() const;
	obs_sceneitem_t *itemAt(const QPointF &canvasPos) const;
	int handleAt(obs_sceneitem_t *item, const QPointF &widgetPos) const;
	void beginResize(obs_sceneitem_t *item);
	void applyResize(const QPointF &canvasPos, bool freeAspect);
	QPointF snap(obs_sceneitem_t *item, const QPointF &pos) const;

	Mode m_mode;
	OBSDisplay m_display;
	mutable QMutex m_lock;
	OBSSource m_scene;

	// Written by the draw callback, read by the mouse handlers.
	std::atomic<float> m_viewX{0}, m_viewY{0}, m_scale{1}, m_dpr{1};
	std::atomic<int64_t> m_hoverId{-1};

	enum Drag { None, Move, Resize } m_drag = None;
	OBSSceneItem m_dragItem;
	QPointF m_dragStart, m_itemStart;
	QRectF m_resizeStart;
	int m_handle = -1;
	QSizeF m_baseSize;
	bool m_boundsMode = false;
};
