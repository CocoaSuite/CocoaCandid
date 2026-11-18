#pragma once

#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

/**
 * Global hotkeys through libobs, so they work while another app (a game) has focus. Bindings are kept in
 * settings.json as Qt key sequences, one per action.
 */
class Hotkeys : public QObject {
	Q_OBJECT
public:
	struct Action {
		QString id;
		QString label;
		std::function<void()> run;
	};

	static Hotkeys &get();
	void registerAll(const QVector<Action> &actions);
	const QVector<Action> &actions() const { return m_actions; }
	QKeySequence binding(const QString &id) const;
	void setBinding(const QString &id, const QKeySequence &seq);
	void applyAll();
	void unregisterAll();

private:
	Hotkeys() = default;
	QVector<Action> m_actions;
	QVector<size_t> m_ids;
};
