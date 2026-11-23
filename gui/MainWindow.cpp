#include "MainWindow.hpp"

#include <QAction>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QToolButton>

#include "AnalysisSidebar.hpp"
#include "BoardWidget.hpp"
#include "Doorman.hpp"
#include "KeyBindings.hpp"
#include "ModeSwitch.hpp"
#include "MoveTreeOverlay.hpp"
#include "PlayView.hpp"
#include "PrincipleDrawer.hpp"
#include "SearchWindow.hpp"
#include "ToolBarWidget.hpp"
#include "TutorialOverlay.hpp"

namespace {

constexpr int FADE_DURATION = 170;

} // !namespace

MainWindow::MainWindow(Doorman& doorman, QWidget* parent)
	: QMainWindow(parent) {
	setWindowTitle(tr("Chess Engine GUI"));
	resize(1400, 900);

	keyBindings = new KeyBindings(*this);
	toolBar = new ToolBarWidget(doorman, *keyBindings, this);
	addToolBar(Qt::TopToolBarArea, toolBar);

	//view switch first in the toolbar
	modeSwitch = new ModeSwitch(toolBar);
	QAction* firstAction = toolBar->actions().value(0);
	toolBar->insertWidget(firstAction, modeSwitch);
	toolBar->insertSeparator(firstAction);

	//tutorial, at the far right
	auto* toolBarSpacer = new QWidget(toolBar);
	toolBarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	toolBar->addWidget(toolBarSpacer);
	tutorialButton = new QToolButton(toolBar);
	tutorialButton->setText(tr("Tutorial"));
	tutorialButton->setToolTip(tr("Show the tutorial again"));
	tutorialButton->setFocusPolicy(Qt::NoFocus);
	toolBar->addWidget(tutorialButton);

	views = new QStackedWidget(this);

	//analysis: board | evaluation, the sidebar slides away on its key and the board takes the room
	analysisView = new QWidget(views);
	analysisLayout = new QHBoxLayout(analysisView);
	analysisLayout->setContentsMargins(8, 8, 8, 8);
	analysisLayout->setSpacing(0);
	board = new BoardWidget(doorman, analysisView);
	analysisLayout->addWidget(board, 1);
	sidebar = new AnalysisSidebar(doorman, *keyBindings, analysisView);
	analysisLayout->addWidget(sidebar);
	views->addWidget(analysisView);

	//play: the same board, it moves over when the view changes
	playView = new PlayView(doorman, *keyBindings, views);
	views->addWidget(playView);
	views->setCurrentWidget(analysisView);
	setCentralWidget(views);

	fadeCover = new QLabel(views);
	fadeCover->setAttribute(Qt::WA_TransparentForMouseEvents);
	fadeCover->hide();
	auto* fadeEffect = new QGraphicsOpacityEffect(fadeCover);
	fadeCover->setGraphicsEffect(fadeEffect);
	fade = new QPropertyAnimation(fadeEffect, "opacity", this);
	fade->setDuration(FADE_DURATION);
	fade->setEasingCurve(QEasingCurve::InOutQuad);
	connect(fade, &QPropertyAnimation::finished, fadeCover, [this] {
		fadeCover->hide();
		fadeCover->clear();	//frees the snapshot
	});

	//principles, slide up from the bottom (analysis only)
	principleDrawer = new PrincipleDrawer(doorman, *keyBindings, *this);

	//move tree, shown over everything (analysis only)
	moveTree = new MoveTreeOverlay(doorman, *keyBindings, *this);

	//what the engine is thinking, its own window (both views)
	searchWindow = new SearchWindow(*keyBindings, *this);

	//tutorial, over everything else, so it is created last
	tutorial = new TutorialOverlay(*this, buildTutorialLessons(), [this](TutorialTarget target) { return getTutorialTarget(target); });
	connect(tutorial, &TutorialOverlay::lessonEntered, this, &MainWindow::enterTutorialLesson);
	connect(tutorial, &TutorialOverlay::finished, this, &MainWindow::endTutorial);
	connect(tutorialButton, &QToolButton::clicked, this, &MainWindow::startTutorial);

	connect(modeSwitch, &ModeSwitch::modeRequested, this, &MainWindow::setViewMode);
	connect(playView, &PlayView::focusModeRequested, this, &MainWindow::setFocusMode);
	connect(keyBindings, &KeyBindings::pressed, this, [this](KeyAction action) {
		switch (action) {
			case KeyAction::SWITCH_MODE:
				setViewMode(viewMode == ViewMode::ANALYSIS ? ViewMode::PLAY : ViewMode::ANALYSIS);
				break;
			case KeyAction::FOCUS_MODE:
				setFocusMode(!focusMode);
				break;
			case KeyAction::CLOSE_DRAWER:
				setFocusMode(false);
				break;
			case KeyAction::SEARCH_WINDOW:
				searchWindow->toggle();
				break;
			default:
				break;
		}
	});
	connect(keyBindings, &KeyBindings::changed, this, &MainWindow::updateSwitchTip);

	updateKeyScope();
	updateSwitchTip();
}

void MainWindow::setSearchRecorder(SearchRecorder& recorder) {
	searchWindow->setRecorder(recorder);
}

void MainWindow::setViewMode(ViewMode mode) {
	if (mode == viewMode) {
		modeSwitch->setMode(mode);
		return;
	}
	const bool animate = isVisible() && !focusMode;	//leaving focus mode resizes the window, a snapshot wouldn't fit
	const QPixmap snapshot = animate ? views->grab() : QPixmap();
	setFocusMode(false);
	principleDrawer->setOpen(false);
	moveTree->endNavigation();

	viewMode = mode;
	updateKeyScope();
	if (mode == ViewMode::PLAY) {
		analysisLayout->removeWidget(board);
		playView->placeBoard(*board);
		views->setCurrentWidget(playView);
	} else {
		playView->releaseBoard(*board);
		analysisLayout->insertWidget(0, board, 1);
		views->setCurrentWidget(analysisView);
	}
	board->show();	//reparenting hides it
	playView->setActive(mode == ViewMode::PLAY);
	modeSwitch->setMode(mode);
	if (animate) {
		crossFade(snapshot);
	}
}

ViewMode MainWindow::getViewMode() const {
	return viewMode;
}

void MainWindow::setFocusMode(bool focus) {
	if (focus == focusMode || (focus && viewMode != ViewMode::PLAY)) {
		return;
	}
	focusMode = focus;
	toolBar->setVisible(!focus);
	playView->setFocusMode(focus);
	if (focus) {
		maximizedBeforeFocus = isMaximized();
		showFullScreen();
	} else if (maximizedBeforeFocus) {
		showMaximized();
	} else {
		showNormal();
	}
}

bool MainWindow::isFocusMode() const {
	return focusMode;
}

//the system can end fullscreen on its own (e.g. a window manager shortcut): leave focus mode with it
void MainWindow::changeEvent(QEvent* event) {
	QMainWindow::changeEvent(event);
	if (event->type() == QEvent::WindowStateChange && focusMode && !isFullScreen()) {
		focusMode = false;
		toolBar->show();
		playView->setFocusMode(false);
	}
}

void MainWindow::updateKeyScope() {
	if (tutorial != nullptr && tutorial->isRunning()) {	//no key does anything behind the tutorial
		for (std::size_t index = 0; index < KEY_ACTION_COUNT; ++index) {
			keyBindings->setActionEnabled(static_cast<KeyAction>(index), false);
		}
		return;
	}
	const bool analysis = viewMode == ViewMode::ANALYSIS;
	for (KeyAction action : {KeyAction::TOGGLE_SIDEBAR, KeyAction::MOVE_TREE, KeyAction::PRINCIPLE_DRAWER, KeyAction::TREE_BACK, KeyAction::TREE_FORWARD}) {
		keyBindings->setActionEnabled(action, analysis);
	}
	keyBindings->setActionEnabled(KeyAction::FOCUS_MODE, !analysis);
}

void MainWindow::updateSwitchTip() {
	const QKeyCombination key = keyBindings->getKey(KeyAction::SWITCH_MODE);
	modeSwitch->setToolTip(KeyBindings::isBound(key) ? tr("Analysis or play against the engine (%1)").arg(KeyBindings::getKeyName(key))
		: tr("Analysis or play against the engine"));
}

void MainWindow::crossFade(const QPixmap& from) {
	fade->stop();
	fadeCover->setPixmap(from);
	fadeCover->setGeometry(views->rect());
	fadeCover->show();
	fadeCover->raise();
	fade->setStartValue(1.0);
	fade->setEndValue(0.0);
	fade->start();
}

void MainWindow::startTutorial() {
	if (tutorial->isRunning()) {
		return;
	}
	setFocusMode(false);
	moveTree->endNavigation();
	viewBeforeTutorial = viewMode;
	sidebarBeforeTutorial = sidebar->isShown();
	tutorial->start();
	updateKeyScope();
}

QWidget* MainWindow::getTutorialTarget(TutorialTarget target) const {
	switch (target) {
		case TutorialTarget::BOARD: return board;
		case TutorialTarget::TOOLBAR: return toolBar;
		case TutorialTarget::MODE_SWITCH: return modeSwitch;
		case TutorialTarget::ENGINE_SETTINGS: return playView->getEngineSettings();
		case TutorialTarget::EVAL_SIDEBAR: return sidebar;
		case TutorialTarget::PRINCIPLE_DRAWER: return principleDrawer;
		case TutorialTarget::TUTORIAL_BUTTON: return tutorialButton;
		default: return nullptr;
	}
}

//every lesson brings its view, and the drawer or the sidebar when it is about them
void MainWindow::enterTutorialLesson(const TutorialLesson& lesson) {
	setViewMode(lesson.view);	//closes the drawer when the view changes
	principleDrawer->setOpen(lesson.target == TutorialTarget::PRINCIPLE_DRAWER);
	sidebar->setShown(sidebarBeforeTutorial || lesson.target == TutorialTarget::EVAL_SIDEBAR);
	updateKeyScope();
}

void MainWindow::endTutorial() {
	principleDrawer->setOpen(false);
	sidebar->setShown(sidebarBeforeTutorial);
	setViewMode(viewBeforeTutorial);
	updateKeyScope();
	TutorialOverlay::writeMarker();
}
