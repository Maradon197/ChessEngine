#ifndef TUTORIALOVERLAY_HPP
#define TUTORIALOVERLAY_HPP

#include <functional>
#include <vector>

#include <QMargins>
#include <QPointer>
#include <QRectF>
#include <QString>
#include <QWidget>

#include "GuiTypes.hpp"

class QGraphicsOpacityEffect;
class QLabel;
class QPushButton;
class QVariantAnimation;

//the parts of the window a lesson can leave undimmed; the window looks up the widget behind each one
enum class TutorialTarget {
	NONE,				//everything dimmed, only the text box
	CUSTOM,				//TutorialLesson::customArea
	BOARD,
	TOOLBAR,
	MODE_SWITCH,		//"Analysis | Play"
	ENGINE_SETTINGS,	//play view: the depth slider
	EVAL_SIDEBAR,		//analysis view: the evaluation column, shown for the lesson
	PRINCIPLE_DRAWER,	//analysis view: opened for the lesson
	TUTORIAL_BUTTON
};

//one step of the tutorial, the list itself is in TutorialLessons.cpp
struct TutorialLesson {
	QString text;									//markdown
	ViewMode view = ViewMode::ANALYSIS;				//the window switches to it first
	TutorialTarget target = TutorialTarget::NONE;
	QRectF customArea;								//CUSTOM only: fractions of the window, (0, 0, 1, 1) is all of it
	QMargins padding{6, 6, 6, 6};					//grows the clear area around the target (negative: shrinks it)
};

std::vector<TutorialLesson> buildTutorialLessons();

//covers the whole window: dims everything except the current lesson's target, the text box sits next to it
//all clicks outside the text box are swallowed; the window disables its keys while the tutorial runs
class TutorialOverlay : public QWidget {
	Q_OBJECT

public:
	using TargetLookup = std::function<QWidget*(TutorialTarget)>;

	TutorialOverlay(QWidget& window, std::vector<TutorialLesson> lessons, TargetLookup lookup);

	void start();	//from the first lesson; does nothing while running
	bool isRunning() const;

	//marker file next to the saved games: the tutorial starts on its own when it is missing or not exactly as written
	static QString getMarkerPath();
	static bool isMarkerCurrent();
	static void writeMarker();

signals:
	void lessonEntered(const TutorialLesson& lesson);	//the window brings the lesson's view and target on screen
	void finished();	//last lesson clicked through or skipped, the overlay is fading out

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void contextMenuEvent(QContextMenuEvent* event) override;

private:
	void showLesson(int index);
	void next();
	void finish();
	void refresh();	//target moved or the hole animation advanced: new hole, box placement, repaint
	QRectF getTargetRect() const;	//in overlay coordinates, empty when nothing is highlighted
	void placeBox(const QRectF& target);
	void watch(QWidget* widget);

	std::vector<TutorialLesson> lessons;
	TargetLookup lookup;
	int current = -1;
	bool running = false;

	QPointer<QWidget> targetWidget;	//its moves and resizes move the hole
	QRectF hole;				//as painted
	QRectF holeFrom;			//hole when the lesson changed
	qreal holeProgress = 1;
	QVariantAnimation* holeAnimation = nullptr;
	QVariantAnimation* fade = nullptr;
	QGraphicsOpacityEffect* fadeEffect = nullptr;

	QWidget* box = nullptr;	//its background is painted by the overlay, so it follows theme changes
	QLabel* textLabel = nullptr;
	QLabel* stepLabel = nullptr;
	QPushButton* skipButton = nullptr;
	QPushButton* nextButton = nullptr;
};

#endif
