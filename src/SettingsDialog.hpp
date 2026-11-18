#pragma once

#include <QDialog>
#include <obs.hpp>

class QStackedWidget;
class QListWidget;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QKeySequenceEdit;

/** Settings: General, Stream, Record, Video, Audio, Hotkeys and Advanced. Nothing applies until Done. */
class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	explicit SettingsDialog(QWidget *parent = nullptr, const QString &page = {});

signals:
	void lookChanged();
	void showTourRequested();

protected:
	void accept() override;

private:
	QWidget *general();
	QWidget *stream();
	QWidget *record();
	QWidget *video();
	QWidget *audio();
	QWidget *hotkeys();
	QWidget *advanced();
	void fillServers();
	void editEncoder(bool forStream);

	QListWidget *m_nav;
	QStackedWidget *m_pages;
	OBSDataAutoRelease m_draft; // a copy of the settings, written back on Done

	// General
	QComboBox *m_theme, *m_layout, *m_icon;
	QCheckBox *m_snap, *m_confirmStop;
	// Stream
	QComboBox *m_service, *m_server, *m_streamEnc, *m_audioKbps;
	QLineEdit *m_customServer, *m_key;
	QSpinBox *m_bitrate, *m_keyint;
	QCheckBox *m_dyn;
	// Record
	QLineEdit *m_path;
	QComboBox *m_format, *m_quality, *m_recEnc;
	QCheckBox *m_replay;
	QSpinBox *m_replaySec;
	// Video
	QSpinBox *m_baseW, *m_baseH, *m_outW, *m_outH;
	QComboBox *m_fps, *m_scale, *m_space, *m_range;
	// Audio
	QComboBox *m_rate, *m_channels;
	// Hotkeys
	QVector<QPair<QString, QKeySequenceEdit *>> m_keys;
	// Advanced
	QComboBox *m_priority;
};
