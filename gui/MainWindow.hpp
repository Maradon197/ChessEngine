#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>

#include "GuiTypes.hpp"

class BoardWidget;
class Doorman;
class KeyBindings;
class ModeSwitch;
class MoveTreeOverlay;
class PlayView;
class PrincipleDrawer;
class QBoxLayout;
class QLabel;
class QPropertyAnimation;
class QStackedWidget;
class ToolBarWidget;

//two views over the same board: Analysis (evaluation, principles, move tree) and Play (board and clocks only)
//Ctrl+Tab or the switch in the toolbar changes between them; the one BoardWidget moves to the shown view
class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(Doorman& doorman, QWidget* parent = nullptr);

	void setViewMode(ViewMode mode);
	ViewMode getViewMode() const;

	void setFocusMode(bool focus);	//play view only: fullscreen without the toolbar
	bool isFocusMode() const;

protected:
	void changeEvent(QEvent* event) override;

private:
	void updateKeyScope();	//the analysis keys do nothing while playing and the other way round
	void updateSwitchTip();
	void crossFade(const QPixmap& from);

	KeyBindings* keyBindings = nullptr;
	ToolBarWidget* toolBar = nullptr;
	ModeSwitch* modeSwitch = nullptr;
	QStackedWidget* views = nullptr;
	QWidget* analysisView = nullptr;
	QBoxLayout* analysisLayout = nullptr;
	PlayView* playView = nullptr;
	BoardWidget* board = nullptr;
	PrincipleDrawer* principleDrawer = nullptr;
	MoveTreeOverlay* moveTree = nullptr;
	QLabel* fadeCover = nullptr;	//snapshot of the old view, fades out over the new one
	QPropertyAnimation* fade = nullptr;

	ViewMode viewMode = ViewMode::ANALYSIS;
	bool focusMode = false;
	bool maximizedBeforeFocus = false;
};

#endif
