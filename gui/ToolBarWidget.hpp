#ifndef TOOLBARWIDGET_HPP
#define TOOLBARWIDGET_HPP

#include <array>

#include <QToolBar>

#include "GuiTypes.hpp"

class Doorman;
class KeyBindings;
class QAction;
class QComboBox;

class ToolBarWidget : public QToolBar {
	Q_OBJECT

public:
	ToolBarWidget(Doorman& doorman, KeyBindings& keyBindings, QWidget* parent = nullptr);

private:
	void loadSettings();
	void chooseColor(ColorRole role);
	void resetColors();
	void updateSwatches();
	void editKey(KeyAction action);
	void updateKeyTexts();

	Doorman& doorman;
	KeyBindings& keyBindings;
	QComboBox* themeBox = nullptr;
	std::array<QAction*, COLOR_ROLE_COUNT> colorActions{};
	std::array<QAction*, KEY_ACTION_COUNT> keyActions{};
};

#endif
