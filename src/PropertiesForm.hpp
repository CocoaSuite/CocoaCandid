#pragma once

#include <QWidget>
#include <functional>
#include <obs.hpp>

class QGridLayout;

/**
 * Builds a form from a libobs property list (a source's, filter's or encoder's settings), so every
 * plugin's options can be edited without writing a dialog for each. Changes are written into the settings
 * object at once and passed to onChange.
 */
class PropertiesForm : public QWidget {
	Q_OBJECT
public:
	using MakeProps = std::function<obs_properties_t *()>;
	using OnChange = std::function<void(obs_data_t *)>;

	PropertiesForm(obs_data_t *settings, MakeProps make, OnChange onChange, void *object = nullptr, QWidget *parent = nullptr);
	~PropertiesForm() override;

	/** A form for a source or filter, updating it live. */
	static PropertiesForm *forSource(obs_source_t *source, QWidget *parent = nullptr);

	void rebuild();
	bool isEmpty() const { return m_rows == 0; }
	obs_data_t *settings() const { return m_settings; }

signals:
	void changed();

private:
	void addProperty(obs_property_t *p, QGridLayout *grid, int &row);
	void commit(obs_property_t *p);
	void scheduleRebuild();

	OBSData m_settings;
	MakeProps m_make;
	OnChange m_onChange;
	void *m_object;
	obs_properties_t *m_props = nullptr;
	QWidget *m_body = nullptr;
	int m_rows = 0;
	bool m_rebuildQueued = false;
};
