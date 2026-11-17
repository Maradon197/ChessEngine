#include "KeyBindings.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSettings>
#include <QWidget>

namespace {

QString getSettingsName(KeyAction action) {
	switch (action) {
		case KeyAction::TOGGLE_SIDEBAR: return "toggleSidebar";
		case KeyAction::MOVE_TREE: return "moveTree";
		case KeyAction::PRINCIPLE_DRAWER: return "principleDrawer";
		case KeyAction::CLOSE_DRAWER: return "closeDrawer";
		case KeyAction::TREE_BACK: return "treeBack";
		case KeyAction::TREE_FORWARD: return "treeForward";
		default: return {};
	}
}

QString getKeyKey(KeyAction action) {
	return QString("keys/%1/key").arg(getSettingsName(action));
}

QString getModeKey(KeyAction action) {
	return QString("keys/%1/mode").arg(getSettingsName(action));
}

bool repeatsWhileHeld(KeyAction action) {
	return action == KeyAction::TREE_BACK || action == KeyAction::TREE_FORWARD;
}

bool isModifierKey(int key) {
	return key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Meta
		|| key == Qt::Key_AltGr || key == Qt::Key_CapsLock || key == Qt::Key_NumLock || key == Qt::Key_unknown || key == 0;
}

bool isTextInput(const QWidget& widget) {
	if (widget.inherits("QLineEdit") || widget.inherits("QAbstractSpinBox")) {
		return true;
	}
	return (widget.inherits("QTextEdit") || widget.inherits("QPlainTextEdit")) && !widget.property("readOnly").toBool();
}

//typing and widgets that move with the arrow keys keep their keys; shortcuts with Ctrl/Alt and Esc always get through
bool reachesBindings(const QWidget& receiver, QKeyCombination key) {
	const bool command = key.keyboardModifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
	if (command) {
		return true;
	}
	const Qt::Key code = key.key();
	if (code == Qt::Key_Escape || (code >= Qt::Key_F1 && code <= Qt::Key_F35)) {
		return true;
	}
	if (isTextInput(receiver)) {
		return false;
	}
	const bool navigation = code == Qt::Key_Left || code == Qt::Key_Right || code == Qt::Key_Up || code == Qt::Key_Down
		|| code == Qt::Key_PageUp || code == Qt::Key_PageDown || code == Qt::Key_Home || code == Qt::Key_End;
	return !navigation || !(receiver.inherits("QComboBox") || receiver.inherits("QAbstractScrollArea")
		|| receiver.inherits("QAbstractSlider") || receiver.property("ownsArrowKeys").toBool());
}

} // !namespace

KeyBindings::KeyBindings(QWidget& window)
	: QObject(&window)
	, window(window) {
	load();
	window.installEventFilter(this);	//focus loss releases held keys
	qApp->installEventFilter(this);
}

QKeyCombination KeyBindings::getKey(KeyAction action) const {
	return keys[static_cast<std::size_t>(action)];
}

KeyMode KeyBindings::getMode(KeyAction action) const {
	return modes[static_cast<std::size_t>(action)];
}

std::optional<KeyAction> KeyBindings::findAction(QKeyCombination key) const {
	if (!isBound(key)) {
		return std::nullopt;
	}
	for (std::size_t index = 0; index < KEY_ACTION_COUNT; ++index) {
		if (keys[index] == key) {
			return static_cast<KeyAction>(index);
		}
	}
	return std::nullopt;
}

void KeyBindings::setKey(KeyAction action, QKeyCombination key) {
	if (const std::optional<KeyAction> previous = findAction(key); previous && *previous != action) {
		keys[static_cast<std::size_t>(*previous)] = QKeyCombination();
		save(*previous);
	}
	releaseAll();
	keys[static_cast<std::size_t>(action)] = key;
	save(action);
	emit changed();
}

void KeyBindings::setMode(KeyAction action, KeyMode mode) {
	if (!hasMode(action)) {
		return;
	}
	releaseAll();
	modes[static_cast<std::size_t>(action)] = mode;
	save(action);
	emit changed();
}

void KeyBindings::resetToDefaults() {
	releaseAll();
	QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
	for (std::size_t index = 0; index < KEY_ACTION_COUNT; ++index) {
		const auto action = static_cast<KeyAction>(index);
		settings.remove(getKeyKey(action));
		settings.remove(getModeKey(action));
		keys[index] = getDefaultKey(action);
		modes[index] = getDefaultMode(action);
	}
	emit changed();
}

bool KeyBindings::hasMode(KeyAction action) {
	return action == KeyAction::TOGGLE_SIDEBAR || action == KeyAction::MOVE_TREE || action == KeyAction::PRINCIPLE_DRAWER;
}

QKeyCombination KeyBindings::getDefaultKey(KeyAction action) {
	switch (action) {
		case KeyAction::TOGGLE_SIDEBAR: return Qt::Key_A;
		case KeyAction::MOVE_TREE: return Qt::Key_M;
		case KeyAction::PRINCIPLE_DRAWER: return Qt::ControlModifier | Qt::Key_Space;
		case KeyAction::CLOSE_DRAWER: return Qt::Key_Escape;
		case KeyAction::TREE_BACK: return Qt::Key_Left;
		case KeyAction::TREE_FORWARD: return Qt::Key_Right;
		default: return {};
	}
}

KeyMode KeyBindings::getDefaultMode(KeyAction action) {
	return action == KeyAction::MOVE_TREE ? KeyMode::HOLD : KeyMode::TOGGLE;
}

QString KeyBindings::getActionName(KeyAction action) {
	switch (action) {
		case KeyAction::TOGGLE_SIDEBAR: return tr("Analysis sidebar");
		case KeyAction::MOVE_TREE: return tr("Move tree");
		case KeyAction::PRINCIPLE_DRAWER: return tr("Principle drawer");
		case KeyAction::CLOSE_DRAWER: return tr("Close drawer / details");
		case KeyAction::TREE_BACK: return tr("Previous move");
		case KeyAction::TREE_FORWARD: return tr("Next move");
		default: return {};
	}
}

QString KeyBindings::getKeyName(QKeyCombination key) {
	return isBound(key) ? QKeySequence(key).toString(QKeySequence::NativeText) : QString::fromUtf8("–");
}

bool KeyBindings::isBound(QKeyCombination key) {
	return !isModifierKey(key.key());
}

void KeyBindings::bindVisibility(KeyAction action, QObject& context, std::function<bool()> isShown, std::function<void(bool)> setShown) {
	connect(this, &KeyBindings::pressed, &context, [this, action, isShown, setShown](KeyAction pressedAction) {
		if (pressedAction == action) {
			setShown(getMode(action) == KeyMode::HOLD || !isShown());
		}
	});
	connect(this, &KeyBindings::released, &context, [this, action, setShown](KeyAction releasedAction) {
		if (releasedAction == action && getMode(action) == KeyMode::HOLD) {
			setShown(false);
		}
	});
}

bool KeyBindings::eventFilter(QObject* watched, QEvent* event) {
	if (watched == &window && event->type() == QEvent::WindowDeactivate) {
		releaseAll();
		return false;
	}
	if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) {
		return false;
	}
	auto* receiver = qobject_cast<QWidget*>(watched);
	if (receiver == nullptr || receiver->window() != &window) {
		return false;	//popups and dialogs keep their keys
	}

	const auto& keyEvent = *static_cast<QKeyEvent*>(event);
	const int code = keyEvent.key();
	if (event->type() == QEvent::KeyRelease) {
		bool wasHeld = false;
		for (std::size_t index = 0; index < KEY_ACTION_COUNT; ++index) {
			if (heldKeys[index] != 0 && heldKeys[index] == code) {
				wasHeld = true;
				if (!keyEvent.isAutoRepeat()) {	//modifiers may already be up, so only the key itself is compared
					heldKeys[index] = 0;
					emit released(static_cast<KeyAction>(index));
				}
			}
		}
		return wasHeld;
	}

	if (isModifierKey(code)) {
		return false;
	}
	const auto modifiers = keyEvent.modifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
	const QKeyCombination key(modifiers, static_cast<Qt::Key>(code));
	const std::optional<KeyAction> action = findAction(key);
	if (!action || !reachesBindings(*receiver, key)) {
		return false;
	}
	const auto index = static_cast<std::size_t>(*action);
	if (keyEvent.isAutoRepeat()) {
		if (repeatsWhileHeld(*action)) {
			emit pressed(*action);
		}
		return true;
	}
	heldKeys[index] = code;
	emit pressed(*action);
	return true;
}

void KeyBindings::load() {
	const QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
	for (std::size_t index = 0; index < KEY_ACTION_COUNT; ++index) {
		const auto action = static_cast<KeyAction>(index);
		keys[index] = getDefaultKey(action);
		modes[index] = getDefaultMode(action);

		const QVariant storedKey = settings.value(getKeyKey(action));
		if (storedKey.isValid()) {
			const QKeySequence sequence = QKeySequence::fromString(storedKey.toString(), QKeySequence::PortableText);
			keys[index] = sequence.isEmpty() ? QKeyCombination() : sequence[0];	//stored empty: unbound on purpose
		}
		const QVariant storedMode = settings.value(getModeKey(action));
		if (storedMode.isValid() && hasMode(action)) {
			modes[index] = storedMode.toInt() == static_cast<int>(KeyMode::HOLD) ? KeyMode::HOLD : KeyMode::TOGGLE;
		}
	}
}

void KeyBindings::save(KeyAction action) const {
	QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
	const QKeyCombination key = getKey(action);
	settings.setValue(getKeyKey(action), isBound(key) ? QKeySequence(key).toString(QKeySequence::PortableText) : QString());
	if (hasMode(action)) {
		settings.setValue(getModeKey(action), static_cast<int>(getMode(action)));
	}
}

void KeyBindings::releaseAll() {
	for (std::size_t index = 0; index < KEY_ACTION_COUNT; ++index) {
		if (heldKeys[index] != 0) {
			heldKeys[index] = 0;
			emit released(static_cast<KeyAction>(index));
		}
	}
}
