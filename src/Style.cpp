#include "Style.hpp"
#include "Icons.hpp"

#include <QApplication>
#include <QAbstractItemView>
#include <QComboBox>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QLayout>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyle>
#include <QWidget>

/**
 * Menus and dropdown lists get a soft drop shadow so they stand out from the panels behind them. The popup
 * window is made see-through with a margin, and the shadow is painted into that margin just before Qt
 * paints the menu itself.
 */
class PopupShadow : public QObject {
public:
	static constexpr int M = 14; // shadow margin, px

	static bool wants(QWidget *w)
	{
		return qobject_cast<QMenu *>(w) || w->inherits("QComboBoxPrivateContainer");
	}

	bool eventFilter(QObject *o, QEvent *e) override
	{
		auto *w = qobject_cast<QWidget *>(o);
		if (!w)
			return false;
		// A dropdown's list window exists before it's first shown; prepare it while its combo box is polished.
		if (e->type() == QEvent::Polish)
			if (auto *cb = qobject_cast<QComboBox *>(w)) {
				QWidget *c = cb->view() ? cb->view()->window() : nullptr;
				if (c && c != cb->window() && !c->testAttribute(Qt::WA_WState_Created)) {
					c->setAttribute(Qt::WA_TranslucentBackground);
					c->setWindowFlag(Qt::NoDropShadowWindowHint);
					c->installEventFilter(this);
				}
				return false;
			}
		if (!w->isWindow() || !wants(w))
			return false;
		if (e->type() == QEvent::Polish && !w->testAttribute(Qt::WA_WState_Created)) {
			// Must happen before the native window exists.
			w->setAttribute(Qt::WA_TranslucentBackground);
			w->setWindowFlag(Qt::FramelessWindowHint);
			w->setWindowFlag(Qt::NoDropShadowWindowHint);
		} else if (e->type() == QEvent::Show && qobject_cast<QMenu *>(w) && w->testAttribute(Qt::WA_TranslucentBackground)) {
			// The see-through margin would push the menu away from where it was opened.
			w->move(w->pos() - QPoint(M, M - 4));
		} else if (e->type() == QEvent::Show && w->inherits("QComboBoxPrivateContainer")) {
			if (QLayout *l = w->layout())
				l->setContentsMargins(M, M - 4, M, M + 2);
			// Grow the window by the shadow margin so the list keeps the combo box's width and position.
			if (w->testAttribute(Qt::WA_TranslucentBackground) && !w->property("candidGrown").toBool()) {
				w->setProperty("candidGrown", true);
				QRect g = w->geometry().adjusted(-M, -(M - 4), M, M + 2);
				// Wide enough for the longest item, so names aren't cut.
				if (auto *v = w->findChild<QAbstractItemView *>()) {
					const int need = v->sizeHintForColumn(0) + 2 * M + 34;
					if (g.width() < need)
						g.setWidth(need);
				}
				w->setGeometry(g);
			}
			if (auto *f = qobject_cast<QFrame *>(w))
				f->setFrameShape(QFrame::NoFrame);
		} else if (e->type() == QEvent::Hide && w->inherits("QComboBoxPrivateContainer")) {
			w->setProperty("candidGrown", false);
		} else if (e->type() == QEvent::Paint && w->testAttribute(Qt::WA_TranslucentBackground)) {
			QPainter p(w);
			p.setRenderHint(QPainter::Antialiasing);
			p.setCompositionMode(QPainter::CompositionMode_Source);
			p.fillRect(w->rect(), Qt::transparent);
			p.setCompositionMode(QPainter::CompositionMode_SourceOver);
			const QRectF inner = QRectF(w->rect()).adjusted(M, M - 4, -M, -M - 2);
			const bool dark = Theme::current().dark;
			// Layered rounded rectangles approximate a blur, slightly lower than the panel.
			for (int i = M; i >= 1; i--) {
				const double k = 1.0 - double(i) / (M + 1);
				QColor c(0, 0, 0, int((dark ? 70 : 34) * k * k / 3.2));
				QPainterPath path;
				path.addRoundedRect(inner.adjusted(-i, -i + 3, i, i + 3), 8 + i, 8 + i);
				p.fillPath(path, c);
			}
		}
		return false;
	}
};

static Theme g_theme;

static Theme make(bool dark)
{
	Theme t;
	t.dark = dark;
	if (dark) {
		t.bg = "#121212";
		t.panel = "#1a1a1a";
		t.panel2 = "#252525";
		t.hov = "#363636";
		t.line = "#303030";
		t.fg = "#ececec";
		t.mute = "#9a9a9a";
		t.pri = "#f2f2f2";
		t.prifg = "#111111";
		t.sel = "#E0A3B3";
		// Selections are plain gray like the other Cocoa apps; garnet is kept for accents only.
		t.selbg = "#383838";
		t.selbd = "#4d4d4d";
		t.desk = "#1f1f1f";
	} else {
		t.bg = "#f4f4f2";
		t.panel = "#ffffff";
		t.panel2 = "#f0f0ee";
		t.hov = "#e6e6e3";
		t.line = "#dddcd8";
		t.fg = "#1c1c1c";
		t.mute = "#6b6b6b";
		t.pri = "#1c1c1c";
		t.prifg = "#ffffff";
		t.sel = "#a8546a";
		t.selbg = "#e2e2df";
		t.selbd = "#c8c8c4";
		t.desk = "#e8e8e5";
	}
	t.rec = "#ff5a5f";
	t.bad = "#ff8a73";
	t.ok = "#5b9f74";
	t.warn = "#c9a24a";
	return t;
}

const Theme &Theme::current()
{
	if (!g_theme.bg.isValid())
		g_theme = make(true);
	return g_theme;
}

void Theme::apply(const QString &name)
{
	g_theme = make(name != "light");
	const Theme &t = g_theme;
	Icons::setColors(t.mute, t.fg);

	QPalette p;
	p.setColor(QPalette::Window, t.bg);
	p.setColor(QPalette::WindowText, t.fg);
	p.setColor(QPalette::Base, t.bg);
	p.setColor(QPalette::AlternateBase, t.panel2);
	p.setColor(QPalette::Text, t.fg);
	p.setColor(QPalette::Button, t.panel2);
	p.setColor(QPalette::ButtonText, t.fg);
	p.setColor(QPalette::Highlight, t.selbg);
	p.setColor(QPalette::HighlightedText, t.fg);
	p.setColor(QPalette::ToolTipBase, t.panel2);
	p.setColor(QPalette::ToolTipText, t.fg);
	p.setColor(QPalette::PlaceholderText, t.mute);
	p.setColor(QPalette::Link, t.sel);
	p.setColor(QPalette::Disabled, QPalette::Text, t.mute);
	p.setColor(QPalette::Disabled, QPalette::ButtonText, t.mute);
	p.setColor(QPalette::Disabled, QPalette::WindowText, t.mute);
	qApp->setPalette(p);
	static PopupShadow *shadow = nullptr;
	if (!shadow) {
		shadow = new PopupShadow;
		qApp->installEventFilter(shadow);
	}
	qApp->setStyleSheet(styleSheet());
}

void addShadow(QWidget *w, int blur, int dy)
{
	auto *fx = new QGraphicsDropShadowEffect(w);
	fx->setBlurRadius(blur);
	fx->setOffset(0, dy);
	fx->setColor(QColor(0, 0, 0, Theme::current().dark ? 150 : 60));
	w->setGraphicsEffect(fx);
}

void setKind(QWidget *w, const char *kind)
{
	w->setProperty("kind", kind);
	w->style()->unpolish(w);
	w->style()->polish(w);
}

QString Theme::styleSheet()
{
	const Theme &t = current();
	QString s = R"(
* { font-family: Inter, "Segoe UI", system-ui, sans-serif; font-size: 13px; color: @fg; }
QMainWindow, QDialog { background: @bg; }
QWidget#panel, QFrame#panel { background: @panel; }
QWidget#desk { background: @desk; }
QLabel#ph { color: @mute; font-size: 11px; font-weight: 500; letter-spacing: 1px; padding: 0; }
QLabel#mute, QLabel[mute="true"] { color: @mute; }
QLabel#title { font-size: 15px; font-weight: 600; }
QLabel#big { font-size: 20px; font-weight: 600; }
QFrame#line { background: @line; max-height: 1px; min-height: 1px; border: none; }
QFrame#vline { background: @line; max-width: 1px; min-width: 1px; border: none; }

QPushButton, QToolButton { background: @panel2; border: 1px solid @line; border-radius: 6px; padding: 5px 10px; }
QPushButton:hover, QToolButton:hover { border-color: @mute; }
QPushButton:pressed, QToolButton:pressed { background: @hov; }
QPushButton:disabled, QToolButton:disabled { color: @mute; }
QPushButton:checked, QToolButton:checked { background: @selbg; border-color: @selbd; }
QPushButton[kind="pri"] { background: @pri; border-color: @pri; color: @prifg; font-weight: 600; }
QPushButton[kind="pri"]:hover { background: @pri; border-color: @pri; }
QPushButton[kind="rec"] { background: @rec; border-color: @rec; color: #ffffff; font-weight: 600; }
QPushButton[kind="danger"] { color: @bad; }
QPushButton[kind="ic"], QToolButton[kind="ic"] { background: transparent; border-color: transparent; padding: 5px; border-radius: 5px; }
QPushButton[kind="ic"]:hover, QToolButton[kind="ic"]:hover { background: @panel2; }
QPushButton[kind="ic"]:checked, QToolButton[kind="ic"]:checked { background: @selbg; border-color: transparent; }
QPushButton[kind="mt"], QToolButton[kind="mt"] { background: transparent; border: none; padding: 4px 9px; border-radius: 4px; }
QPushButton[kind="mt"]:hover, QToolButton[kind="mt"]:hover, QToolButton[kind="mt"]:pressed { background: @panel2; }
QToolButton[kind="mt"]::menu-indicator { image: none; width: 0; }
QPushButton[kind="tab"] { background: transparent; border: none; border-bottom: 2px solid transparent; border-radius: 0; padding: 7px 8px; color: @mute; font-size: 12px; }
QPushButton[kind="tab"]:checked { color: @fg; border-bottom-color: @sel; background: transparent; }
QPushButton[kind="seg"] { background: transparent; border: none; border-radius: 6px; padding: 6px 10px; color: @mute; }
QPushButton[kind="seg"]:checked { background: @selbg; color: @fg; }
QPushButton[kind="tile"] { text-align: left; padding: 10px 12px; border-radius: 10px; min-height: 58px; }
QPushButton[kind="tile"]:checked { background: @selbg; border-color: @selbd; }
QPushButton[kind="row"] { background: transparent; border: 1px solid transparent; text-align: left; padding: 6px 10px; border-radius: 5px; }
QPushButton[kind="row"]:hover { background: @panel2; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit, QKeySequenceEdit QLineEdit {
  background: @bg; border: 1px solid @line; border-radius: 5px; padding: 4px 6px; selection-background-color: @selbd; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus { border-color: @sel; }
QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0; border: none; }
QComboBox { padding-right: 22px; combobox-popup: 0; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox::down-arrow { image: url(:/icons/chevron.svg); width: 12px; height: 12px; }
QComboBox QAbstractItemView { background: @panel2; border: 1px solid @menubd; border-radius: 8px; selection-background-color: @hov; outline: none; padding: 4px; }

QListWidget, QTreeWidget, QListView { background: transparent; border: none; outline: none; }
QListWidget::item, QTreeWidget::item { border-radius: 5px; padding: 0 4px; min-height: 32px; }
QListWidget::item:hover, QTreeWidget::item:hover { background: @panel2; }
QListWidget::item:selected, QTreeWidget::item:selected { background: @selbg; color: @fg; }

QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }
QScrollBar::handle { background: rgba(128,128,128,90); border-radius: 3px; min-height: 24px; min-width: 24px; margin: 2px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QMenu { background: @panel2; border: 1px solid @menubd; border-radius: 8px; padding: 4px; margin: 10px 14px 16px 14px; }
QMenu::item { padding: 6px 22px 6px 10px; border-radius: 4px; }
QMenu::item:selected { background: @hov; }
QMenu::item:disabled { color: @mute; }
QMenu::separator { height: 1px; background: @line; margin: 4px 2px; }
QMenu::icon { padding-left: 6px; }
QToolTip { background: @panel2; color: @fg; border: 1px solid @menubd; border-radius: 5px; padding: 4px 7px; }

QSlider::groove:horizontal { height: 4px; background: @line; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @mute; border-radius: 2px; }
QSlider::handle:horizontal { background: @fg; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }
QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 15px; height: 15px; border-radius: 4px; border: 1px solid @mute; background: @bg; }
QCheckBox::indicator:checked { background: @sel; border-color: @sel; image: url(:/icons/check.svg); }
QRadioButton::indicator { width: 14px; height: 14px; border-radius: 7px; border: 1px solid @mute; background: @bg; }
QRadioButton::indicator:checked { background: @sel; border-color: @sel; }
QGroupBox { border: none; margin-top: 18px; font-weight: 600; }
QGroupBox::title { subcontrol-origin: margin; left: 0; color: @mute; font-size: 11px; }
QTabWidget::pane { border: none; }
QSplitter::handle { background: @panel; }
QSplitter::handle:hover { background: @selbd; }
QStatusBar { background: @panel; border-top: 1px solid @line; color: @mute; }
QStatusBar QLabel { color: @mute; font-size: 11.5px; padding: 0 6px; }
QMessageBox { background: @panel; }
)";
	const QList<QPair<QString, QColor>> vars = {
		{"@menubd", t.dark ? QColor("#3d3d3d") : QColor("#d0cfcb")},
		{"@panel2", t.panel2}, {"@panel", t.panel}, {"@bg", t.bg},       {"@hov", t.hov},     {"@line", t.line},
		{"@fg", t.fg},         {"@mute", t.mute},   {"@prifg", t.prifg}, {"@pri", t.pri},     {"@selbg", t.selbg},
		{"@selbd", t.selbd},   {"@sel", t.sel},     {"@desk", t.desk},   {"@rec", t.rec},     {"@bad", t.bad},
	};
	for (const auto &v : vars)
		s.replace(v.first, v.second.name());
	return s;
}
