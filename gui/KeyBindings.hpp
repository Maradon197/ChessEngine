#ifndef KEYBINDINGS_HPP
#define KEYBINDINGS_HPP

#include <array>
#include <functional>
#include <optional>

#include <QKeyCombination>
#include <QObject>

#include "GuiTypes.hpp"

class QWidget;

//every keyboard shortcut of the main window, one key combination per action; rebindable and kept in the settings
//keys are caught before the focused widget sees them, except while typing or when a widget needs its arrow keys
class KeyBindings : public QObject {
	Q_OBJECT

public:
	explicit KeyBindings(QWidget& window);

	QKeyCombination getKey(KeyAction action) const;
	KeyMode getMode(KeyAction action) const;
	std::optional<KeyAction> findAction(QKeyCombination key) const;

	void setKey(KeyAction action, QKeyCombination key);	//an action that had this key loses it
	void setMode(KeyAction action, KeyMode mode);
	void resetToDefaults();

	static bool hasMode(KeyAction action);	//only actions that show something can be held
	static QKeyCombination getDefaultKey(KeyAction action);
	static KeyMode getDefaultMode(KeyAction action);
	static QString getActionName(KeyAction action);
	static QString getKeyName(QKeyCombination key);	//"–" when unbound
	static bool isBound(QKeyCombination key);

	//held: shown while the key is down; toggle: every press flips it
	void bindVisibility(KeyAction action, QObject& context, std::function<bool()> isShown, std::function<void(bool)> setShown);

signals:
	void pressed(KeyAction action);	//repeats while held for the move tree arrows
	void released(KeyAction action);
	void changed();

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	void load();
	void save(KeyAction action) const;
	void releaseAll();

	QWidget& window;
	std::array<QKeyCombination, KEY_ACTION_COUNT> keys{};
	std::array<KeyMode, KEY_ACTION_COUNT> modes{};
	std::array<int, KEY_ACTION_COUNT> heldKeys{};	//Qt::Key that is down for the action, 0 when up
};

#endif
