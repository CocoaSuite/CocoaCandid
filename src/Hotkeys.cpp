#include "Hotkeys.hpp"
#include "Config.hpp"

#include <QMetaObject>
#include <obs-hotkey.h>
#include <obs.h>

Hotkeys &Hotkeys::get()
{
	static Hotkeys h;
	return h;
}

static void pressed(void *data, obs_hotkey_id, obs_hotkey_t *, bool down)
{
	if (!down)
		return;
	auto *fn = static_cast<std::function<void()> *>(data);
	QMetaObject::invokeMethod(&Hotkeys::get(), [fn] { (*fn)(); }, Qt::QueuedConnection);
}

void Hotkeys::registerAll(const QVector<Action> &actions)
{
	unregisterAll();
	m_actions = actions;
	m_ids.clear();
	for (auto &a : m_actions) {
		const QByteArray id = ("candid." + a.id).toUtf8(), label = a.label.toUtf8();
		m_ids.append(obs_hotkey_register_frontend(id.constData(), label.constData(), pressed, &a.run));
	}
	applyAll();
}

void Hotkeys::unregisterAll()
{
	for (size_t id : m_ids)
		obs_hotkey_unregister(id);
	m_ids.clear();
}

QKeySequence Hotkeys::binding(const QString &id) const
{
	OBSDataAutoRelease h = obs_data_get_obj(CFG.data(), "hotkeys");
	return h ? QKeySequence(QString::fromUtf8(obs_data_get_string(h, id.toUtf8().constData()))) : QKeySequence();
}

void Hotkeys::setBinding(const QString &id, const QKeySequence &seq)
{
	CFG.setStr("hotkeys", id.toUtf8().constData(), seq.toString(QKeySequence::PortableText));
}

static obs_key_t toObsKey(int key)
{
	QString name;
	if (key >= Qt::Key_A && key <= Qt::Key_Z)
		name = QString("OBS_KEY_%1").arg(QChar('A' + (key - Qt::Key_A)));
	else if (key >= Qt::Key_0 && key <= Qt::Key_9)
		name = QString("OBS_KEY_%1").arg(key - Qt::Key_0);
	else if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
		name = QString("OBS_KEY_F%1").arg(key - Qt::Key_F1 + 1);
	else {
		switch (key) {
		case Qt::Key_Space:
			name = "OBS_KEY_SPACE";
			break;
		case Qt::Key_Home:
			name = "OBS_KEY_HOME";
			break;
		case Qt::Key_End:
			name = "OBS_KEY_END";
			break;
		case Qt::Key_PageUp:
			name = "OBS_KEY_PAGEUP";
			break;
		case Qt::Key_PageDown:
			name = "OBS_KEY_PAGEDOWN";
			break;
		case Qt::Key_Insert:
			name = "OBS_KEY_INSERT";
			break;
		case Qt::Key_Delete:
			name = "OBS_KEY_DELETE";
			break;
		case Qt::Key_Left:
			name = "OBS_KEY_LEFT";
			break;
		case Qt::Key_Right:
			name = "OBS_KEY_RIGHT";
			break;
		case Qt::Key_Up:
			name = "OBS_KEY_UP";
			break;
		case Qt::Key_Down:
			name = "OBS_KEY_DOWN";
			break;
		case Qt::Key_Pause:
			name = "OBS_KEY_PAUSE";
			break;
		case Qt::Key_ScrollLock:
			name = "OBS_KEY_SCROLLLOCK";
			break;
		case Qt::Key_Print:
			name = "OBS_KEY_PRINT";
			break;
		case Qt::Key_Minus:
			name = "OBS_KEY_MINUS";
			break;
		case Qt::Key_Equal:
			name = "OBS_KEY_EQUAL";
			break;
		default:
			return OBS_KEY_NONE;
		}
	}
	return obs_key_from_name(name.toUtf8().constData());
}

void Hotkeys::applyAll()
{
	for (int i = 0; i < m_actions.size() && i < m_ids.size(); i++) {
		const QKeySequence seq = binding(m_actions[i].id);
		if (seq.isEmpty()) {
			obs_hotkey_load_bindings(m_ids[i], nullptr, 0);
			continue;
		}
		const QKeyCombination kc = seq[0];
		obs_key_combination_t combo = {};
		const Qt::KeyboardModifiers mods = kc.keyboardModifiers();
		if (mods & Qt::ShiftModifier)
			combo.modifiers |= INTERACT_SHIFT_KEY;
		if (mods & Qt::ControlModifier)
			combo.modifiers |= INTERACT_CONTROL_KEY;
		if (mods & Qt::AltModifier)
			combo.modifiers |= INTERACT_ALT_KEY;
		if (mods & Qt::MetaModifier)
			combo.modifiers |= INTERACT_COMMAND_KEY;
		combo.key = toObsKey(kc.key());
		if (combo.key == OBS_KEY_NONE)
			obs_hotkey_load_bindings(m_ids[i], nullptr, 0);
		else
			obs_hotkey_load_bindings(m_ids[i], &combo, 1);
	}
}
