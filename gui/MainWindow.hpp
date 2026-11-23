#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <vector>

#include <QMainWindow>

#include "GuiTypes.hpp"

class AnalysisSidebar;
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
class QToolButton;
class SearchRecorder;
class SearchWindow;
class ToolBarWidget;
class TutorialOverlay;
enum class TutorialTarget;
struct TutorialLesson;

//two views over the same board: Analysis (evaluation, principles, move tree) and Play (board only)
//Ctrl+Tab or the switch in the toolbar changes between them; the one BoardWidget moves to the shown view
class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(Doorman& doorman, QWidget* parent = nullptr);

	void setViewMode(ViewMode mode);
	ViewMode getViewMode() const;

	void setFocusMode(bool focus);	//play view only: fullscreen without the toolbar
	bool isFocusMode() const;

	void startTutorial();	//on the first start and from the toolbar button

	void setSearchRecorder(SearchRecorder& recorder);	//what the search window (W) shows
	int getPlayDepth() const;	//the engine's search depth in the play view, plies
	int getAnalysisDepth() const;	//and in the analysis
	bool isEngineArrowShown() const;	//analysis: the engine draws its best move

signals:
	void viewModeChanged(ViewMode mode);
	void analysisDepthChanged(int depth);
	void engineArrowShownChanged(bool shown);
	void searchLineChosen(NodeId origin, const std::vector<Move>& moves);	//clicked in the search window: play these from origin

protected:
	void changeEvent(QEvent* event) override;

private:
	void updateKeyScope();	//the analysis keys do nothing while playing and the other way round
	void updateSwitchTip();
	void crossFade(const QPixmap& from);
	QWidget* getTutorialTarget(TutorialTarget target) const;
	void enterTutorialLesson(const TutorialLesson& lesson);
	void endTutorial();

	KeyBindings* keyBindings = nullptr;
	ToolBarWidget* toolBar = nullptr;
	ModeSwitch* modeSwitch = nullptr;
	QStackedWidget* views = nullptr;
	QWidget* analysisView = nullptr;
	QBoxLayout* analysisLayout = nullptr;
	PlayView* playView = nullptr;
	BoardWidget* board = nullptr;
	AnalysisSidebar* sidebar = nullptr;
	PrincipleDrawer* principleDrawer = nullptr;
	MoveTreeOverlay* moveTree = nullptr;
	QLabel* fadeCover = nullptr;	//snapshot of the old view, fades out over the new one
	QPropertyAnimation* fade = nullptr;
	QToolButton* tutorialButton = nullptr;
	TutorialOverlay* tutorial = nullptr;
	SearchWindow* searchWindow = nullptr;

	ViewMode viewMode = ViewMode::ANALYSIS;
	bool focusMode = false;
	bool maximizedBeforeFocus = false;
	ViewMode viewBeforeTutorial = ViewMode::ANALYSIS;	//the tutorial switches around, afterwards everything goes back
	bool sidebarBeforeTutorial = true;
};

#endif
