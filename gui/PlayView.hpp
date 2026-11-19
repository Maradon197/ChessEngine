#ifndef PLAYVIEW_HPP
#define PLAYVIEW_HPP

#include <QWidget>

#include "DataTypes.hpp"
#include "GuiTypes.hpp"

class BoardWidget;
class ClockFace;
class Doorman;
class GameClock;
class KeyBindings;
class QComboBox;
class QHBoxLayout;
class QToolButton;

//the "play the engine" page: the shared board, two clocks and nothing that evaluates
//the user plays the color at the bottom of the board, the engine the other one
class PlayView : public QWidget {
	Q_OBJECT

public:
	PlayView(Doorman& doorman, KeyBindings& keyBindings, QWidget* parent = nullptr);
	~PlayView() override;

	//the window owns the one board and hands it over when the view changes
	void placeBoard(BoardWidget& board);
	void releaseBoard(BoardWidget& board);

	void setActive(bool active);	//shown or not: the clock only runs while playing here
	void setFocusMode(bool focus);	//the window goes fullscreen, this view hides its extras

	//placeholder: a game setup (side, hints, time control, ...) will configure the game here; not built yet

signals:
	void engineMoveRequested(const Position& position);	//the engine's side is to move; nothing answers yet
	void focusModeRequested(bool focus);	//the focus button was clicked

protected:
	void changeEvent(QEvent* event) override;

private:
	void checkPosition();	//after every move: switch the clock, ask the engine when it's its turn
	void rememberPosition(const Position& position);
	void applyTimeControl(int index);
	void updateClocks();
	void updateFocusButton();
	Color getUserColor() const;

	Doorman& doorman;
	KeyBindings& keyBindings;
	BoardWidget* board = nullptr;
	QHBoxLayout* boardRow = nullptr;
	GameClock* clock = nullptr;
	ClockFace* engineClock = nullptr;
	ClockFace* userClock = nullptr;
	QComboBox* timeControlBox = nullptr;
	QToolButton* focusButton = nullptr;

	bool active = false;
	bool focusMode = false;
	Color seenSide = WHITE;
	Bitboard seenOccupancy = 0;
};

#endif
