#pragma once

#include <QColor>
#include <QString>

/** The CocoaShot palette (dark and light), with CocoaCandid's garnet accent. */
struct Theme {
	QColor bg, panel, panel2, hov, line, fg, mute, pri, prifg, sel, selbg, selbd, desk, rec, bad, ok, warn;
	bool dark = true;

	static const Theme &current();
	static void apply(const QString &name); // "dark" or "light"
	static QString styleSheet();
};

/** Marks a button's look: "pri", "ic", "rec", "mt", "tab", "seg" or "danger". */
class QWidget;
void setKind(QWidget *w, const char *kind);
/** A soft shadow under a floating element inside a window (bars, prompts, cards). */
void addShadow(QWidget *w, int blur = 24, int dy = 6);
