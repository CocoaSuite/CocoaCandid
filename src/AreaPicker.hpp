#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QVector>
#include <functional>

class QScreen;
class AreaOverlay;

/**
 * Like the Snipping Tool: every screen is frozen and dimmed, you drag a rectangle on one of them, adjust
 * it, then add it. The result is a screen capture of that display, cropped to the rectangle.
 */
class AreaPicker : public QObject {
	Q_OBJECT
public:
	using Done = std::function<void(QScreen *screen, const QRect &logicalRect)>;
	static void start(Done done);

	/** Creates the cropped screen capture and adds it to the edited scene. */
	static bool addArea(QScreen *screen, const QRect &logicalRect, QString *error);

private:
	explicit AreaPicker(Done done);
	void finish(QScreen *screen, const QRect &r);
	QVector<QPointer<AreaOverlay>> m_overlays;
	Done m_done;
	friend class AreaOverlay;
};
