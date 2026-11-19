#include "PlayView.hpp"

#include <array>
#include <optional>

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

#include "BoardWidget.hpp"
#include "Doorman.hpp"
#include "GameClock.hpp"
#include "KeyBindings.hpp"

namespace {

constexpr int GAP_TO_BOARD = 12;
constexpr int CLOCK_COLUMN_WIDTH = 210;
constexpr int CLOCK_HEIGHT = 92;
constexpr qreal CARD_RADIUS = 12;
constexpr int CARD_PADDING = 12;
constexpr int HEADER_HEIGHT = 22;
constexpr qreal SIDE_DOT_RADIUS = 5.5;
constexpr qint64 TENTHS_BELOW = 10'000;	//ms, the last seconds are shown with tenths
constexpr int FOCUS_ICON_SIZE = 18;

const QColor WHITE_SIDE(245, 245, 245);
const QColor BLACK_SIDE(30, 30, 30);
const QColor FLAG_COLOR(225, 80, 60);

const char* const TIME_CONTROL_KEY = "play/timeControl";

constexpr std::array<TimeControl, 5> TIME_CONTROLS{{{0, 0}, {180, 2}, {300, 0}, {600, 0}, {900, 10}}};
constexpr int DEFAULT_TIME_CONTROL = 3;	//10 + 0

QString getTimeControlName(TimeControl control) {
	return control.hasClock() ? QString("%1 + %2").arg(control.baseSeconds / 60).arg(control.incrementSeconds) : PlayView::tr("No clock");
}

QString getTimeControlKey(TimeControl control) {
	return QString("%1+%2").arg(control.baseSeconds).arg(control.incrementSeconds);
}

QColor withAlpha(QColor color, int alpha) {
	color.setAlpha(alpha);
	return color;
}

QColor mix(const QColor& from, const QColor& to, float share) {
	return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * share, from.greenF() + (to.greenF() - from.greenF()) * share,
		from.blueF() + (to.blueF() - from.blueF()) * share);
}

QFont makeHeaderFont(const QFont& base) {
	QFont font = base;
	font.setBold(true);
	font.setCapitalization(QFont::AllUppercase);
	font.setLetterSpacing(QFont::PercentageSpacing, 108);
	font.setPointSizeF(font.pointSizeF() * 0.85);
	return font;
}

//"5:00", "1:02:03", "0:09.4"
QString formatTime(qint64 milliseconds) {
	if (milliseconds < TENTHS_BELOW) {
		const qint64 tenths = milliseconds / 100;
		return QString("0:%1.%2").arg(tenths / 10, 2, 10, QChar('0')).arg(tenths % 10);
	}
	const qint64 seconds = milliseconds / 1000;
	const qint64 hours = seconds / 3600;
	if (hours > 0) {
		return QString("%1:%2:%3").arg(hours).arg(seconds / 60 % 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
	}
	return QString("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QChar('0'));
}

//four corner brackets: pointing outwards to enter focus mode, inwards to leave it
QIcon makeFocusIcon(bool leave, const QColor& color) {
	constexpr qreal ratio = 2;
	constexpr qreal margin = 3;
	constexpr qreal arm = 4.5;
	QPixmap pixmap(static_cast<int>(FOCUS_ICON_SIZE * ratio), static_cast<int>(FOCUS_ICON_SIZE * ratio));
	pixmap.setDevicePixelRatio(ratio);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
	for (const qreal x : {margin, FOCUS_ICON_SIZE - margin}) {
		for (const qreal y : {margin, FOCUS_ICON_SIZE - margin}) {
			const qreal dx = x == margin ? arm : -arm;	//towards the middle
			const qreal dy = y == margin ? arm : -arm;
			const QPointF corner = leave ? QPointF(x + dx, y + dy) : QPointF(x, y);
			const QPointF points[] = {
				leave ? QPointF(corner.x(), y) : QPointF(x, y + dy),
				corner,
				leave ? QPointF(x, corner.y()) : QPointF(x + dx, y)
			};
			painter.drawPolyline(points, 3);
		}
	}
	return QIcon(pixmap);
}

} // !namespace

//one side's clock: who it is (with a dot in their piece color) and the time left; lit while it runs
class ClockFace : public QWidget {
public:
	ClockFace(const QString& title, QWidget* parent)
		: QWidget(parent)
		, title(title) {
		setFixedHeight(CLOCK_HEIGHT);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	void setSide(Color newSide) {
		side = newSide;
		update();
	}

	void setState(std::optional<qint64> newRemaining, bool newRunning, bool newFlagged) {
		remaining = newRemaining;
		running = newRunning;
		flagged = newFlagged;
		update();
	}

protected:
	void paintEvent(QPaintEvent* event) override {
		Q_UNUSED(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		const QPalette& colors = palette();
		const QColor text = colors.color(QPalette::Text);
		const QColor highlight = colors.color(QPalette::Highlight);

		const QRectF card = QRectF(rect()).adjusted(1, 1, -1, -1);
		const QColor outline = flagged ? FLAG_COLOR : running ? highlight : withAlpha(colors.color(QPalette::Mid), 90);
		painter.setPen(QPen(outline, flagged || running ? 1.5 : 1));
		painter.setBrush(running ? mix(colors.color(QPalette::Base), highlight, 0.12f) : colors.color(QPalette::Base));
		painter.drawRoundedRect(card, CARD_RADIUS, CARD_RADIUS);

		//header: piece color dot, name, and the flag on the right
		const QRectF header(CARD_PADDING, CARD_PADDING - 2, width() - 2 * CARD_PADDING, HEADER_HEIGHT);
		painter.setPen(QPen(withAlpha(colors.color(QPalette::Mid), 200), 1));
		painter.setBrush(side == WHITE ? WHITE_SIDE : BLACK_SIDE);
		painter.drawEllipse(QPointF(header.left() + SIDE_DOT_RADIUS, header.center().y()), SIDE_DOT_RADIUS, SIDE_DOT_RADIUS);
		painter.setFont(makeHeaderFont(font()));
		painter.setPen(withAlpha(text, 150));
		const QRectF titleRect = header.adjusted(2 * SIDE_DOT_RADIUS + 8, 0, 0, 0);
		painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);
		if (flagged) {
			painter.setPen(FLAG_COLOR);
			painter.drawText(titleRect, Qt::AlignRight | Qt::AlignVCenter, ClockFace::tr("Time is up"));
		}

		//the time itself
		const QRectF body(CARD_PADDING, header.bottom(), width() - 2 * CARD_PADDING, height() - header.bottom() - CARD_PADDING + 2);
		if (!remaining) {
			painter.setFont(font());
			painter.setPen(withAlpha(text, 120));
			painter.drawText(body, Qt::AlignLeft | Qt::AlignVCenter, ClockFace::tr("No clock"));
			return;
		}
		QFont timeFont = font();
		timeFont.setWeight(QFont::DemiBold);
		timeFont.setPointSizeF(timeFont.pointSizeF() * 2.2);
		painter.setFont(timeFont);
		painter.setPen(flagged ? FLAG_COLOR : running ? text : withAlpha(text, 170));
		painter.drawText(body, Qt::AlignLeft | Qt::AlignVCenter, formatTime(*remaining));
	}

private:
	QString title;
	Color side = WHITE;
	std::optional<qint64> remaining;
	bool running = false;
	bool flagged = false;
};

PlayView::PlayView(Doorman& doorman, KeyBindings& keyBindings, QWidget* parent)
	: QWidget(parent)
	, doorman(doorman)
	, keyBindings(keyBindings) {
	clock = new GameClock(this);

	//board | clocks, laid out like the analysis view so the board doesn't jump when switching
	boardRow = new QHBoxLayout(this);
	boardRow->setContentsMargins(8, 8, 8, 8);
	boardRow->setSpacing(0);

	auto* column = new QWidget(this);
	column->setFixedWidth(CLOCK_COLUMN_WIDTH + GAP_TO_BOARD);
	auto* columnLayout = new QVBoxLayout(column);
	columnLayout->setContentsMargins(GAP_TO_BOARD, 0, 0, 0);
	columnLayout->setSpacing(10);

	engineClock = new ClockFace(tr("Engine"), column);
	userClock = new ClockFace(tr("You"), column);

	timeControlBox = new QComboBox(column);
	timeControlBox->setFocusPolicy(Qt::ClickFocus);
	timeControlBox->setToolTip(tr("Time control. Choosing one, even the same again, restarts the clocks."));
	for (const TimeControl& control : TIME_CONTROLS) {
		timeControlBox->addItem(getTimeControlName(control));
	}
	focusButton = new QToolButton(column);
	focusButton->setAutoRaise(true);
	focusButton->setFocusPolicy(Qt::NoFocus);
	focusButton->setIconSize(QSize(FOCUS_ICON_SIZE, FOCUS_ICON_SIZE));
	focusButton->setCursor(Qt::PointingHandCursor);

	auto* controls = new QHBoxLayout;
	controls->setSpacing(6);
	controls->addWidget(timeControlBox, 1);
	controls->addWidget(focusButton);

	columnLayout->addWidget(engineClock);
	columnLayout->addStretch(1);
	columnLayout->addLayout(controls);
	columnLayout->addStretch(1);
	columnLayout->addWidget(userClock);
	boardRow->addWidget(column);	//the board goes in front of it, see placeBoard()

	//time control as the user left it
	const QString storedControl = QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).value(TIME_CONTROL_KEY).toString();
	int controlIndex = DEFAULT_TIME_CONTROL;
	for (int index = 0; index < static_cast<int>(TIME_CONTROLS.size()); ++index) {
		if (getTimeControlKey(TIME_CONTROLS[index]) == storedControl) {
			controlIndex = index;
		}
	}
	timeControlBox->setCurrentIndex(controlIndex);
	clock->reset(TIME_CONTROLS[controlIndex]);

	connect(timeControlBox, &QComboBox::activated, this, &PlayView::applyTimeControl);
	connect(clock, &GameClock::changed, this, &PlayView::updateClocks);
	connect(focusButton, &QToolButton::clicked, this, [this] { emit focusModeRequested(!focusMode); });
	connect(&keyBindings, &KeyBindings::changed, this, &PlayView::updateFocusButton);
	//queued: runs after the core has answered the attempt, so the doorman already has the new position
	connect(&doorman, &Doorman::moveAttempted, this, [this] {
		if (active) {
			checkPosition();
		}
	}, Qt::QueuedConnection);

	updateFocusButton();
	updateClocks();
	doorman.attach(*this);
}

PlayView::~PlayView() = default;

void PlayView::placeBoard(BoardWidget& newBoard) {
	board = &newBoard;
	boardRow->insertWidget(0, board, 1);
	updateClocks();
}

void PlayView::releaseBoard(BoardWidget& oldBoard) {
	boardRow->removeWidget(&oldBoard);
	if (board == &oldBoard) {
		board = nullptr;
	}
}

void PlayView::setActive(bool isActive) {
	if (active == isActive) {
		return;
	}
	active = isActive;
	if (!active) {
		clock->pause();
		return;
	}
	//the clock continues with the next move made here, whatever happened in the analysis meanwhile
	const Position position = doorman.getPosition();
	rememberPosition(position);
	updateClocks();
	if (position.side_to_move != getUserColor()) {
		emit engineMoveRequested(position);
	}
}

void PlayView::setFocusMode(bool focus) {
	focusMode = focus;
	timeControlBox->setVisible(!focus);
	updateFocusButton();
}

void PlayView::changeEvent(QEvent* event) {
	QWidget::changeEvent(event);
	if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
		updateFocusButton();
	}
}

void PlayView::checkPosition() {
	const Position position = doorman.getPosition();
	if (position.side_to_move == seenSide && position.total_occupancy == seenOccupancy) {
		return;	//the attempt was illegal and snapped back
	}
	rememberPosition(position);
	clock->start(position.side_to_move);	//the first move starts the clock
	if (position.side_to_move != getUserColor()) {
		emit engineMoveRequested(position);
	}
}

void PlayView::rememberPosition(const Position& position) {
	seenSide = position.side_to_move;
	seenOccupancy = position.total_occupancy;
}

void PlayView::applyTimeControl(int index) {
	if (index < 0 || index >= static_cast<int>(TIME_CONTROLS.size())) {
		return;
	}
	QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(TIME_CONTROL_KEY, getTimeControlKey(TIME_CONTROLS[index]));
	clock->reset(TIME_CONTROLS[index]);
}

void PlayView::updateClocks() {
	const Color user = getUserColor();
	const bool hasClock = clock->getTimeControl().hasClock();
	const auto show = [this, hasClock](ClockFace& face, Color side) {
		face.setSide(side);
		face.setState(hasClock ? std::optional<qint64>(clock->getRemaining(side)) : std::nullopt,
			clock->getRunningSide() == side, clock->hasFlagged(side));
	};
	show(*engineClock, !user);
	show(*userClock, user);
}

void PlayView::updateFocusButton() {
	focusButton->setIcon(makeFocusIcon(focusMode, withAlpha(palette().color(QPalette::Text), 190)));
	const KeyAction action = focusMode ? KeyAction::CLOSE_DRAWER : KeyAction::FOCUS_MODE;
	const QString name = focusMode ? tr("Leave focus mode") : tr("Focus mode");
	const QKeyCombination key = keyBindings.getKey(action);
	focusButton->setToolTip(KeyBindings::isBound(key) ? QString("%1 (%2)").arg(name, KeyBindings::getKeyName(key)) : name);
}

Color PlayView::getUserColor() const {
	return board ? board->getOrientation() : WHITE;
}
