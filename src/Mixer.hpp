#pragma once

#include <QVector>
#include <QWidget>
#include <obs.hpp>

class QVBoxLayout;
class AudioStrip;

/** A row per sound source: level meter, volume, mute and a menu. */
class Mixer : public QWidget {
	Q_OBJECT
public:
	explicit Mixer(QWidget *parent = nullptr);
	void refresh();
	void setCompact(bool compact);

private:
	QVBoxLayout *m_rows;
	QVector<AudioStrip *> m_strips;
	bool m_compact = false;
};

/** Opens a small window with a source's settings (used for sources not on the canvas, like the microphone). */
void showSourceDialog(obs_source_t *source, bool filters, QWidget *parent);
