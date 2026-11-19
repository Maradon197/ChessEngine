#include "GameClock.hpp"

#include <algorithm>

#include <QTimer>

namespace {

constexpr int TICK_INTERVAL = 50;	//ms, fine enough for tenths of a second

} // !namespace

GameClock::GameClock(QObject* parent)
	: QObject(parent) {
	ticker = new QTimer(this);
	ticker->setInterval(TICK_INTERVAL);
	ticker->setTimerType(Qt::PreciseTimer);
	connect(ticker, &QTimer::timeout, this, &GameClock::tick);
}

void GameClock::reset(TimeControl newControl) {
	ticker->stop();
	running.reset();
	control = newControl;
	remaining.fill(static_cast<qint64>(std::max(0, control.baseSeconds)) * 1000);
	flags.fill(false);
	emit changed();
}

TimeControl GameClock::getTimeControl() const {
	return control;
}

void GameClock::start(Color side) {
	if (!control.hasClock() || flags[WHITE] || flags[BLACK] || running == side) {
		return;
	}
	if (running) {
		commitElapsed();
		remaining[*running] += static_cast<qint64>(std::max(0, control.incrementSeconds)) * 1000;
	}
	running = side;
	elapsed.start();
	ticker->start();
	emit changed();
}

void GameClock::pause() {
	if (!running) {
		return;
	}
	commitElapsed();
	running.reset();
	ticker->stop();
	emit changed();
}

bool GameClock::isRunning() const {
	return running.has_value();
}

std::optional<Color> GameClock::getRunningSide() const {
	return running;
}

qint64 GameClock::getRemaining(Color side) const {
	if (running == side) {
		return std::max<qint64>(0, remaining[side] - elapsed.elapsed());
	}
	return remaining[side];
}

bool GameClock::hasFlagged(Color side) const {
	return flags[side];
}

void GameClock::commitElapsed() {
	if (running) {
		remaining[*running] = std::max<qint64>(0, remaining[*running] - elapsed.restart());
	}
}

void GameClock::tick() {
	if (!running) {
		ticker->stop();
		return;
	}
	if (getRemaining(*running) > 0) {
		emit changed();
		return;
	}
	const Color side = *running;
	remaining[side] = 0;
	flags[side] = true;
	running.reset();
	ticker->stop();
	emit changed();
	emit flagged(side);
}
