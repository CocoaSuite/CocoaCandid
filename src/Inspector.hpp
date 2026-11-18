#pragma once

#include <QWidget>
#include <obs.hpp>

class QStackedWidget;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QLabel;
class QListWidget;
class QSpinBox;
class QDoubleSpinBox;
class PropertiesForm;

/** The right panel: the selected source's settings, position, crop and blending, and its filters. */
class Inspector : public QWidget {
	Q_OBJECT
public:
	explicit Inspector(QWidget *parent = nullptr);
	void showFilters();
	void showProperties();
	/** Builds a filter list and editor for any source (used by the mixer too). */
	static QWidget *filtersEditor(obs_source_t *source, QWidget *parent);

private:
	void rebuild();
	void refreshTransform();
	QWidget *buildTransform(obs_sceneitem_t *item);

	QPushButton *m_tabProps, *m_tabFilters;
	QScrollArea *m_scroll;
	QWidget *m_content = nullptr;
	OBSSource m_shownSource;
	int64_t m_shownItem = -1;
	bool m_filters = false;

	QDoubleSpinBox *m_x = nullptr, *m_y = nullptr, *m_w = nullptr, *m_h = nullptr, *m_rot = nullptr;
	QSpinBox *m_crop[4] = {};
};
