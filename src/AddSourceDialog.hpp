#pragma once

#include <QDialog>
#include <obs.hpp>

class QGridLayout;
class QVBoxLayout;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QPushButton;
class QScrollArea;
class QLabel;

/**
 * Pick what to add: a tile per kind of source, then its settings right below with a live picture on the
 * canvas. Area closes the dialog and hands over to the area picker.
 */
class AddSourceDialog : public QDialog {
	Q_OBJECT
public:
	explicit AddSourceDialog(QWidget *parent = nullptr);
	~AddSourceDialog() override;
	bool wantsArea() const { return m_area; }

	struct Kind {
		QString key, icon, title, hint;
		QStringList types;
		bool fit = false;
	};
	static QVector<Kind> kinds();

protected:
	void accept() override;
	void reject() override;

private:
	void pick(int index);
	void discardDraft();

	QVector<Kind> m_kinds;
	QVector<QPushButton *> m_tiles;
	int m_current = -1;
	OBSSource m_draft;
	bool m_area = false;

	QLabel *m_detailTitle;
	QLineEdit *m_name;
	QComboBox *m_existing;
	QScrollArea *m_formArea;
	QCheckBox *m_fit, *m_cropCanvas;
	QPushButton *m_add;
};
