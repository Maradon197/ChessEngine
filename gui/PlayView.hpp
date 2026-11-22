#ifndef PLAYVIEW_HPP
#define PLAYVIEW_HPP

#include <QWidget>

#include "DataTypes.hpp"
#include "GuiTypes.hpp"

class BoardWidget;
class Doorman;
class KeyBindings;
class QHBoxLayout;
class QLabel;
class QSlider;
class QToolButton;

//the "play the engine" page: the shared board and nothing that evaluates
//for now the user always plays white and the engine black; a new game means restarting the app
class PlayView : public QWidget {
	Q_OBJECT

public:
	PlayView(Doorman& doorman, KeyBindings& keyBindings, QWidget* parent = nullptr);
	~PlayView() override;

	//the window owns the one board and hands it over when the view changes
	void placeBoard(BoardWidget& board);
	void releaseBoard(BoardWidget& board);

	void setActive(bool active);	//shown or not: the engine is only asked to move while playing here
	void setFocusMode(bool focus);	//the window goes fullscreen, this view hides its extras

	QWidget* getEngineSettings() const;	//the tutorial points at it
	int getSearchDepth() const;

	static constexpr Color USER_COLOR = WHITE;

signals:
	void engineMoveRequested(const Position& position, int depth);	//the engine's side is to move and the game isn't over, depth in plies
	void focusModeRequested(bool focus);	//the focus button was clicked

protected:
	void changeEvent(QEvent* event) override;

private:
	void checkPosition();	//after every move: ask the engine when it's its turn
	void rememberPosition(const Position& position);
	void updateFocusButton();
	void setSearchDepth(int depth);	//slider moved: shown and kept in the settings, used from the next engine move on

	Doorman& doorman;
	KeyBindings& keyBindings;
	BoardWidget* board = nullptr;
	QHBoxLayout* boardRow = nullptr;
	QToolButton* focusButton = nullptr;
	QWidget* engineSettings = nullptr;
	QSlider* depthSlider = nullptr;
	QLabel* depthValue = nullptr;

	bool active = false;
	bool focusMode = false;
	int searchDepth = DEFAULT_SEARCH_DEPTH;
	Color seenSide = WHITE;
	Bitboard seenOccupancy = 0;
};

#endif
