#include "PlayView.hpp"

#include <algorithm>

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>
#include <QVBoxLayout>

#include "BoardWidget.hpp"
#include "Doorman.hpp"
#include "KeyBindings.hpp"
#include "SearchSettingsWidget.hpp"
#include "SearchStatusLine.hpp"

namespace {

constexpr int GAP_TO_BOARD = 12;
constexpr int SIDE_COLUMN_WIDTH = 210;	//keeps the board where it sits in the analysis view
constexpr int FOCUS_ICON_SIZE = 18;

QColor withAlpha(QColor color, int alpha) {
	color.setAlpha(alpha);
	return color;
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

PlayView::PlayView(Doorman& doorman, KeyBindings& keyBindings, QWidget* parent)
	: QWidget(parent)
	, doorman(doorman)
	, keyBindings(keyBindings) {
	//board | side column, laid out like the analysis view so the board doesn't jump when switching
	boardRow = new QHBoxLayout(this);
	boardRow->setContentsMargins(8, 8, 8, 8);
	boardRow->setSpacing(0);

	auto* column = new QWidget(this);
	column->setFixedWidth(SIDE_COLUMN_WIDTH + GAP_TO_BOARD);
	auto* columnLayout = new QVBoxLayout(column);
	columnLayout->setContentsMargins(GAP_TO_BOARD, 0, 0, 0);
	columnLayout->setSpacing(10);

	//engine settings: how far it looks ahead, and the search's numbers while it thinks
	engineSettings = new QWidget(column);
	auto* settingsLayout = new QVBoxLayout(engineSettings);
	settingsLayout->setContentsMargins(0, 0, 0, 0);
	settingsLayout->setSpacing(4);
	searchSettings = new SearchSettingsWidget(tr("Engine depth"),
		tr("How many half-moves the engine looks ahead. Deeper is stronger, but slower."),
		QStringLiteral("engine/play"), DEFAULT_SEARCH_DEPTH, engineSettings);
	settingsLayout->addWidget(searchSettings);
	searchStatus = new SearchStatusLine(engineSettings);
	settingsLayout->addSpacing(6);
	settingsLayout->addWidget(searchStatus);

	focusButton = new QToolButton(column);
	focusButton->setAutoRaise(true);
	focusButton->setFocusPolicy(Qt::NoFocus);
	focusButton->setIconSize(QSize(FOCUS_ICON_SIZE, FOCUS_ICON_SIZE));
	focusButton->setCursor(Qt::PointingHandCursor);

	columnLayout->addStretch(1);
	columnLayout->addWidget(engineSettings);
	columnLayout->addSpacing(16);
	columnLayout->addWidget(focusButton, 0, Qt::AlignLeft);
	columnLayout->addStretch(1);
	boardRow->addWidget(column);	//the board goes in front of it, see placeBoard()

	connect(focusButton, &QToolButton::clicked, this, [this] { emit focusModeRequested(!focusMode); });
	connect(&keyBindings, &KeyBindings::changed, this, &PlayView::updateFocusButton);
	//queued: runs after the core has answered the attempt, so the doorman already has the new position
	connect(&doorman, &Doorman::moveAttempted, this, [this] {
		if (active) {
			checkPosition();
		}
	}, Qt::QueuedConnection);

	updateFocusButton();
	doorman.attach(*this);
}

PlayView::~PlayView() = default;

void PlayView::placeBoard(BoardWidget& newBoard) {
	board = &newBoard;
	boardRow->insertWidget(0, board, 1);
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
		return;
	}
	//the game continues from whatever position the board shows, also after moves made in the analysis
	const Position position = doorman.getPosition();
	rememberPosition(position);
	if (position.side_to_move != USER_COLOR && doorman.getGameStatus().result == GameResult::NONE) {
		emit engineMoveRequested(position, getSearchDepth());
	}
}

void PlayView::setFocusMode(bool focus) {
	focusMode = focus;
	updateFocusButton();
}

QWidget* PlayView::getEngineSettings() const {
	return engineSettings;
}

int PlayView::getSearchDepth() const {
	return searchSettings->getDepth();
}

int PlayView::getQuiescenceDepth() const {
	return searchSettings->getQuiescenceDepth();
}

void PlayView::setSearchRecorder(SearchRecorder& recorder) {
	searchStatus->setRecorder(recorder);
}

void PlayView::setSearchStatusShown(bool shown) {
	searchStatus->setVisible(shown);
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
	if (position.side_to_move != USER_COLOR && doorman.getGameStatus().result == GameResult::NONE) {
		emit engineMoveRequested(position, getSearchDepth());
	}
}

void PlayView::rememberPosition(const Position& position) {
	seenSide = position.side_to_move;
	seenOccupancy = position.total_occupancy;
}

void PlayView::updateFocusButton() {
	focusButton->setIcon(makeFocusIcon(focusMode, withAlpha(palette().color(QPalette::Text), 190)));
	const KeyAction action = focusMode ? KeyAction::CLOSE_DRAWER : KeyAction::FOCUS_MODE;
	const QString name = focusMode ? tr("Leave focus mode") : tr("Focus mode");
	const QKeyCombination key = keyBindings.getKey(action);
	focusButton->setToolTip(KeyBindings::isBound(key) ? QString("%1 (%2)").arg(name, KeyBindings::getKeyName(key)) : name);
}
