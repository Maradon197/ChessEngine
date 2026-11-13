#ifndef TOOLBARWIDGET_HPP
#define TOOLBARWIDGET_HPP

#include <array>

#include <QToolBar>

#include "GuiTypes.hpp"

class Doorman;
class QAction;
class QComboBox;

class ToolBarWidget : public QToolBar {
	Q_OBJECT

public:
	explicit ToolBarWidget(Doorman& doorman, QWidget* parent = nullptr);

private:
	void loadSettings();
	void chooseColor(ColorRole role);
	void resetColors();
	void updateSwatches();

	Doorman& doorman;
	QComboBox* themeBox = nullptr;
	std::array<QAction*, COLOR_ROLE_COUNT> colorActions{};
};

#endif
