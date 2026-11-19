#ifndef GAMECLOCK_HPP
#define GAMECLOCK_HPP

#include <array>
#include <optional>

#include <QElapsedTimer>
#include <QObject>

#include "DataTypes.hpp"

class QTimer;

struct TimeControl {
	int baseSeconds = 0;	//0: no clock
	int incrementSeconds = 0;

	bool hasClock() const { return baseSeconds > 0; }
};

//two chess clocks for the play view, pure display time keeping: a flag only shows up, the game result stays with the core
class GameClock : public QObject {
	Q_OBJECT

public:
	explicit GameClock(QObject* parent = nullptr);

	void reset(TimeControl control);	//both sides back to the base time, stopped
	TimeControl getTimeControl() const;

	void start(Color side);	//runs this side's time; the side that was running moved and gets the increment
	void pause();	//keeps both times, start() continues
	bool isRunning() const;
	std::optional<Color> getRunningSide() const;

	qint64 getRemaining(Color side) const;	//milliseconds
	bool hasFlagged(Color side) const;

signals:
	void changed();	//every tick and state change, for repainting
	void flagged(Color side);

private:
	void commitElapsed();
	void tick();

	TimeControl control;
	std::array<qint64, 2> remaining{};	//as of the last start/pause; the running side also loses elapsed time
	std::array<bool, 2> flags{};
	std::optional<Color> running;
	QElapsedTimer elapsed;
	QTimer* ticker = nullptr;
};

#endif
