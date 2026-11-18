#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <atomic>

QPushButton *iconButton(const QString &icon, const QString &tip, QWidget *parent = nullptr, bool checkable = false);
QLabel *panelHeader(const QString &text, QWidget *parent = nullptr);
QFrame *hline(QWidget *parent = nullptr);
QFrame *vline(QWidget *parent = nullptr);
QString formatTime(qint64 seconds);

/** A horizontal level meter: green to -20 dB, yellow to -9, red above, with a slowly falling peak mark. */
class Meter : public QWidget {
	Q_OBJECT
public:
	explicit Meter(QWidget *parent = nullptr);
	/** Safe from any thread. Levels in dB (-60 to 0). */
	void setLevels(float magnitudeDb, float peakDb);
	void setMuted(bool m);
	QSize sizeHint() const override { return QSize(160, 8); }

protected:
	void paintEvent(QPaintEvent *) override;
	void timerEvent(QTimerEvent *) override;

private:
	std::atomic<float> m_mag{-60}, m_peak{-60};
	float m_shownPeak = -60, m_shownMag = -60, m_hold = -60;
	qint64 m_holdAt = 0;
	bool m_muted = false;
};

/** An elided label that shows the whole text as a tooltip when cut. */
class ElidedLabel : public QLabel {
public:
	using QLabel::QLabel;
	void setFullText(const QString &t);
	QSize minimumSizeHint() const override { return QSize(20, QLabel::minimumSizeHint().height()); }

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	QString m_full;
	void refit();
};
