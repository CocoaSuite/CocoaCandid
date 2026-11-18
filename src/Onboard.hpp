#pragma once

#include <QPointer>
#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;
class QHBoxLayout;
class QFrame;
class TourArt;

/**
 * First launch, the same as CocoaShot's: "hello" is written in one stroke over the blurred window with
 * the CocoaSuite hello sound, then a short tour rings each part of the real window, ending with the choice
 * of layout. It is a separate see-through window laid over the app, so it can sit above the video preview.
 */
class Onboard : public QWidget {
	Q_OBJECT
public:
	struct Step {
		QString title, text;
		std::function<QList<QWidget *>()> targets; // empty for a centred card
	};

	static void run(QWidget *mainWindow, const QVector<Step> &steps, std::function<void(const QString &layout)> done,
			bool hello = true);

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	bool eventFilter(QObject *o, QEvent *e) override;
	void timerEvent(QTimerEvent *) override;

private:
	Onboard(QWidget *main, const QVector<Step> &steps, std::function<void(const QString &)> done, bool hello);
	void begin(bool fromInput);
	void toCard();
	void show(int index);
	void place();
	void finish();
	void playSound();
	QRect targetRect() const;

	QPointer<QWidget> m_main;
	QVector<Step> m_steps;
	std::function<void(const QString &)> m_done;
	QImage m_blur;
	int m_phase = -1; // -1 waiting, 0 writing hello, 1 tour, 2 closing
	qint64 m_t0 = 0;
	double m_write = 0, m_fade = 0, m_tap = 0, m_helloOut = 0;
	int m_index = 0;
	QString m_layout = "classic";
	QFrame *m_card = nullptr;
	TourArt *m_art = nullptr;
	QLabel *m_title = nullptr, *m_text = nullptr;
	QWidget *m_layouts = nullptr, *m_dots = nullptr;
	QPushButton *m_back = nullptr, *m_next = nullptr;
	QRect m_ring, m_ringTarget;
};
