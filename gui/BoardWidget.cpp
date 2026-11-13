#include "BoardWidget.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <QAction>
#include <QCursor>
#include <QFont>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

#include "MoveBits.hpp"

#include "Doorman.hpp"

using namespace moveBits;

namespace {

const QColor DRAG_HOVER_COLOR(255, 255, 255, 80);
const QColor WHITE_PIECE_FILL(250, 250, 250);
const QColor WHITE_PIECE_DETAIL(25, 25, 25);
const QColor BLACK_PIECE_FILL(35, 35, 35);
const QColor BLACK_PIECE_OUTLINE(10, 10, 10);

constexpr qreal PIECE_SCALE = 0.84;		//king height relative to square size

//Placeholder piece set built from the Unicode chess glyphs, normalized so the king is 1.0 high.
//To use SVG/PNG pieces later, only BoardWidget::paintPiece() has to change.
struct PieceShapes {
	std::array<QPainterPath, 6> solid;
	std::array<QPainterPath, 6> detail;		//outline glyph, drawn over white pieces
};

const PieceShapes& getPieceShapes() {
	static const PieceShapes shapes = [] {
		//indexed by PieceType: PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
		const std::array<char16_t, 6> solidGlyphs = {u'♟', u'♞', u'♝', u'♜', u'♛', u'♚'};
		const std::array<char16_t, 6> detailGlyphs = {u'♙', u'♘', u'♗', u'♖', u'♕', u'♔'};
		const std::array<char, 6> fallbackLetters = {'P', 'N', 'B', 'R', 'Q', 'K'};

		QFont font;
		font.setFamilies({"Segoe UI Symbol", "DejaVu Sans", "Noto Sans Symbols2", "FreeSerif"});
		font.setPixelSize(100);

		PieceShapes raw;
		for (int type = 0; type < 6; ++type) {
			raw.solid[type].addText(0, 0, font, QString(QChar(solidGlyphs[type])));
			raw.detail[type].addText(0, 0, font, QString(QChar(detailGlyphs[type])));
			if (raw.solid[type].isEmpty()) {
				raw.solid[type].addText(0, 0, font, QString(QChar(fallbackLetters[type])));
				raw.detail[type] = QPainterPath();
			}
		}

		const qreal reference = std::max<qreal>(1.0, raw.solid[KING].boundingRect().height());
		PieceShapes normalized;
		for (int type = 0; type < 6; ++type) {
			const QPointF center = raw.solid[type].boundingRect().center();
			QTransform transform;
			transform.scale(1.0 / reference, 1.0 / reference);
			transform.translate(-center.x(), -center.y());
			normalized.solid[type] = transform.map(raw.solid[type]);
			normalized.detail[type] = transform.map(raw.detail[type]);
		}
		return normalized;
	}();
	return shapes;
}

QString getPieceName(PieceType type) {
	switch (type) {
		case KNIGHT: return QObject::tr("Knight");
		case BISHOP: return QObject::tr("Bishop");
		case ROOK: return QObject::tr("Rook");
		case QUEEN: return QObject::tr("Queen");
		case KING: return QObject::tr("King");
		case PAWN: return QObject::tr("Pawn");
		default: return QObject::tr("Piece");
	}
}

} // namespace

BoardWidget::BoardWidget(Doorman& doorman, QWidget* parent)
	: QWidget(parent) {
	setMinimumSize(320, 320);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	position.ep_square = NO_SQUARE;
	doorman.attach(*this);
}

// ================= setters / getters =================

void BoardWidget::setPosition(const Position& newPosition) {
	position = newPosition;
	legalMoves.count = 0;
	endInteraction(false);
	update();
}

const Position& BoardWidget::getPosition() const {
	return position;
}

//Shifts the pieces on screen according to the bits of the move. No legality checks, no rights or clocks:
//this only keeps the picture right until the core sends the authoritative position via setPosition().
//Castling works for both "king to g/c-file" and "king takes own rook" encodings.
void BoardWidget::playMove(Move move) {
	const Square source = getSource(move);
	const Square destination = getDestination(move);
	Color color;
	PieceType type;

	if (getPieceAt(source, color, type)) {
		const Bitboard destinationBit = getSquareBit(destination);
		for (auto& colorPieces : position.pieces) {
			for (Bitboard& bitboard : colorPieces) {
				bitboard &= ~destinationBit;
			}
		}
		position.pieces[color][type] &= ~getSquareBit(source);

		if (isCastle(move)) {
			const int rank = getRank(source);
			const bool kingside = getFile(destination) > getFile(source);
			position.pieces[color][ROOK] &= ~getSquareBit(makeSquare(kingside ? 7 : 0, rank));
			position.pieces[color][ROOK] |= getSquareBit(makeSquare(kingside ? 5 : 3, rank));
			position.pieces[color][KING] |= getSquareBit(makeSquare(kingside ? 6 : 2, rank));
		} else if (isPromotion(move) && getPromotionType(move) > PAWN && getPromotionType(move) < KING) {
			position.pieces[color][getPromotionType(move)] |= destinationBit;
		} else {
			position.pieces[color][type] |= destinationBit;
		}

		if (isEnPassant(move)) {
			const Square capturedSquare = color == WHITE ? destination - 8 : destination + 8;
			position.pieces[!color][PAWN] &= ~getSquareBit(capturedSquare);
		}

		position.color_occupancy[WHITE] = 0;
		position.color_occupancy[BLACK] = 0;
		for (int pieceType = 0; pieceType < 6; ++pieceType) {
			position.color_occupancy[WHITE] |= position.pieces[WHITE][pieceType];
			position.color_occupancy[BLACK] |= position.pieces[BLACK][pieceType];
		}
		position.total_occupancy = position.color_occupancy[WHITE] | position.color_occupancy[BLACK];
		position.side_to_move = !color;
		position.ep_square = NO_SQUARE;
	}

	lastMove = move;
	legalMoves.count = 0;
	endInteraction(false);
	update();
}

void BoardWidget::setLegalMoves(const MoveList& newLegalMoves) {
	legalMoves = newLegalMoves;
	update();
}

void BoardWidget::clearLegalMoves() {
	legalMoves.count = 0;
	update();
}

void BoardWidget::setLastMove(Move move) {
	lastMove = move;
	update();
}

void BoardWidget::clearLastMove() {
	lastMove.reset();
	update();
}

void BoardWidget::setOrientation(Color bottomColor) {
	orientation = bottomColor;
	update();
}

Color BoardWidget::getOrientation() const {
	return orientation;
}

void BoardWidget::setInteractive(bool newInteractive) {
	interactive = newInteractive;
	if (!interactive) {
		endInteraction(false);
	}
}

bool BoardWidget::isInteractive() const {
	return interactive;
}

void BoardWidget::setUserHighlights(const std::vector<Highlight>& highlights) {
	userHighlights = highlights;
	update();
}

void BoardWidget::setUserArrows(const std::vector<Arrow>& arrows) {
	userArrows = arrows;
	update();
}

void BoardWidget::clearUserAnnotations() {
	userHighlights.clear();
	userArrows.clear();
	update();
}

const std::vector<Highlight>& BoardWidget::getUserHighlights() const {
	return userHighlights;
}

const std::vector<Arrow>& BoardWidget::getUserArrows() const {
	return userArrows;
}

void BoardWidget::setEngineHighlights(const std::vector<Highlight>& highlights) {
	engineHighlights = highlights;
	update();
}

void BoardWidget::setEngineArrows(const std::vector<Arrow>& arrows) {
	engineArrows = arrows;
	update();
}

void BoardWidget::clearEngineAnnotations() {
	engineHighlights.clear();
	engineArrows.clear();
	update();
}

void BoardWidget::setColor(ColorRole role, const QColor& color) {
	const auto index = static_cast<std::size_t>(role);
	if (index < COLOR_ROLE_COUNT) {
		colors[index] = color;
		update();
	}
}

const QColor& BoardWidget::getColor(ColorRole role) const {
	return colors[static_cast<std::size_t>(role)];
}

QSize BoardWidget::sizeHint() const {
	return {640, 640};
}

// ================= geometry =================

int BoardWidget::getSquareSize() const {
	return std::max(1, std::min(width(), height()) / 8);
}

QRect BoardWidget::getBoardRect() const {
	const int boardSize = getSquareSize() * 8;
	return {(width() - boardSize) / 2, (height() - boardSize) / 2, boardSize, boardSize};
}

QRectF BoardWidget::getSquareRect(Square square) const {
	const int column = orientation == WHITE ? getFile(square) : 7 - getFile(square);
	const int row = orientation == WHITE ? 7 - getRank(square) : getRank(square);
	const QRect board = getBoardRect();
	const int size = getSquareSize();
	return QRectF(board.left() + column * size, board.top() + row * size, size, size);
}

QPointF BoardWidget::getSquareCenter(Square square) const {
	return getSquareRect(square).center();
}

Square BoardWidget::getSquareAt(const QPointF& point) const {
	const QRect board = getBoardRect();
	if (point.x() < board.left() || point.y() < board.top()
		|| point.x() >= board.left() + board.width() || point.y() >= board.top() + board.height()) {
		return NO_SQUARE;
	}
	const int size = getSquareSize();
	const int column = std::clamp(static_cast<int>((point.x() - board.left()) / size), 0, 7);
	const int row = std::clamp(static_cast<int>((point.y() - board.top()) / size), 0, 7);
	const int file = orientation == WHITE ? column : 7 - column;
	const int rank = orientation == WHITE ? 7 - row : row;
	return makeSquare(file, rank);
}

// ================= lookups =================

bool BoardWidget::getPieceAt(Square square, Color& color, PieceType& type) const {
	if (!isValidSquare(square)) {
		return false;
	}
	const Bitboard bit = getSquareBit(square);
	for (int pieceColor = 0; pieceColor < 2; ++pieceColor) {
		for (int pieceType = 0; pieceType < 6; ++pieceType) {
			if (position.pieces[pieceColor][pieceType] & bit) {
				color = static_cast<Color>(pieceColor);
				type = static_cast<PieceType>(pieceType);
				return true;
			}
		}
	}
	return false;
}

std::vector<Move> BoardWidget::getLegalMovesFrom(Square source) const {
	std::vector<Move> moves;
	const std::size_t count = std::min(legalMoves.count, MAX_MOVES);
	for (std::size_t index = 0; index < count; ++index) {
		if (getSource(legalMoves.moves[index]) == source) {
			moves.push_back(legalMoves.moves[index]);
		}
	}
	return moves;
}

std::vector<Move> BoardWidget::getLegalMovesBetween(Square source, Square destination) const {
	std::vector<Move> moves = getLegalMovesFrom(source);
	std::erase_if(moves, [destination](Move move) { return getDestination(move) != destination; });
	return moves;
}

// ================= painting =================

void BoardWidget::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	paintBoard(painter);
	paintSelections(painter);
	paintPieces(painter);
	paintLegalMoves(painter);
	paintArrows(painter);
	paintDraggedPiece(painter);
}

void BoardWidget::paintBoard(QPainter& painter) const {
	for (Square square = 0; square < 64; ++square) {
		const bool isLight = (getFile(square) + getRank(square)) % 2 == 1;
		painter.fillRect(getSquareRect(square), isLight ? getColor(ColorRole::LIGHT_SQUARE) : getColor(ColorRole::DARK_SQUARE));
	}

	//coordinates on the bottom rank and left file
	const int size = getSquareSize();
	const qreal padding = std::max(2, size / 24);
	QFont font = painter.font();
	font.setPixelSize(std::max(8, size / 6));
	font.setBold(true);
	painter.setFont(font);

	const int bottomRank = orientation == WHITE ? 0 : 7;
	const int leftFile = orientation == WHITE ? 0 : 7;
	for (int index = 0; index < 8; ++index) {
		const Square fileSquare = makeSquare(index, bottomRank);
		painter.setPen((index + bottomRank) % 2 == 1 ? getColor(ColorRole::DARK_SQUARE) : getColor(ColorRole::LIGHT_SQUARE));
		painter.drawText(getSquareRect(fileSquare).adjusted(padding, padding, -padding, -padding),
			Qt::AlignRight | Qt::AlignBottom, QString(QChar('a' + index)));

		const Square rankSquare = makeSquare(leftFile, index);
		painter.setPen((leftFile + index) % 2 == 1 ? getColor(ColorRole::DARK_SQUARE) : getColor(ColorRole::LIGHT_SQUARE));
		painter.drawText(getSquareRect(rankSquare).adjusted(padding, padding, -padding, -padding),
			Qt::AlignLeft | Qt::AlignTop, QString(QChar('1' + index)));
	}
}

void BoardWidget::paintSelections(QPainter& painter) const {
	if (lastMove) {
		painter.fillRect(getSquareRect(getSource(*lastMove)), getColor(ColorRole::LAST_MOVE));
		painter.fillRect(getSquareRect(getDestination(*lastMove)), getColor(ColorRole::LAST_MOVE));
	}
	if (inputState != InputState::IDLE && isValidSquare(selectedSquare)) {
		painter.fillRect(getSquareRect(selectedSquare), getColor(ColorRole::SELECTION));
	}
	if (inputState == InputState::DRAGGING) {
		const Square hoverSquare = getSquareAt(dragCursor);
		if (isValidSquare(hoverSquare) && hoverSquare != selectedSquare) {
			painter.fillRect(getSquareRect(hoverSquare), DRAG_HOVER_COLOR);
		}
	}
	for (const Highlight& highlight : engineHighlights) {
		if (isValidSquare(highlight.toSquare())) {
			painter.fillRect(getSquareRect(highlight.toSquare()), getColor(ColorRole::ENGINE_HIGHLIGHT));
		}
	}
	for (const Highlight& highlight : userHighlights) {
		if (isValidSquare(highlight.toSquare())) {
			painter.fillRect(getSquareRect(highlight.toSquare()), getColor(ColorRole::USER_HIGHLIGHT));
		}
	}
}

void BoardWidget::paintPieces(QPainter& painter) const {
	for (Square square = 0; square < 64; ++square) {
		Color color;
		PieceType type;
		if (!getPieceAt(square, color, type)) {
			continue;
		}
		if (inputState == InputState::DRAGGING && square == selectedSquare) {
			continue;	//drawn at the cursor instead
		}
		paintPiece(painter, color, type, getSquareCenter(square));
	}
}

void BoardWidget::paintLegalMoves(QPainter& painter) const {
	if (inputState == InputState::IDLE || !isValidSquare(selectedSquare)) {
		return;
	}
	const qreal size = getSquareSize();
	Bitboard paintedSquares = 0;	//promotions list the same destination several times

	painter.save();
	painter.setPen(Qt::NoPen);
	painter.setBrush(getColor(ColorRole::LEGAL_MOVE));
	for (Move move : getLegalMovesFrom(selectedSquare)) {
		const Square destination = getDestination(move);
		if (paintedSquares & getSquareBit(destination)) {
			continue;
		}
		paintedSquares |= getSquareBit(destination);

		const QPointF center = getSquareCenter(destination);
		Color color;
		PieceType type;
		if (getPieceAt(destination, color, type)) {
			QPainterPath ring;
			ring.addEllipse(center, size * 0.5, size * 0.5);
			ring.addEllipse(center, size * 0.41, size * 0.41);
			ring.setFillRule(Qt::OddEvenFill);
			painter.drawPath(ring);
		} else {
			painter.drawEllipse(center, size * 0.16, size * 0.16);
		}
	}
	painter.restore();
}

void BoardWidget::paintArrows(QPainter& painter) const {
	for (const Arrow& arrow : engineArrows) {
		paintArrow(painter, arrow.getSource(), arrow.getDestination(), getColor(ColorRole::ENGINE_ARROW));
	}
	for (const Arrow& arrow : userArrows) {
		paintArrow(painter, arrow.getSource(), arrow.getDestination(), getColor(ColorRole::USER_ARROW));
	}
	if (isValidSquare(annotationStartSquare) && isValidSquare(annotationCurrentSquare)
		&& annotationStartSquare != annotationCurrentSquare) {
		QColor previewColor = getColor(ColorRole::USER_ARROW);
		previewColor.setAlphaF(previewColor.alphaF() * 0.6f);
		paintArrow(painter, annotationStartSquare, annotationCurrentSquare, previewColor);
	}
}

void BoardWidget::paintDraggedPiece(QPainter& painter) const {
	Color color;
	PieceType type;
	if (inputState == InputState::DRAGGING && getPieceAt(selectedSquare, color, type)) {
		paintPiece(painter, color, type, dragCursor);
	}
}

void BoardWidget::paintPiece(QPainter& painter, Color color, PieceType type, const QPointF& center) const {
	if (type < PAWN || type > KING) {
		return;
	}
	const PieceShapes& shapes = getPieceShapes();
	const qreal scale = getSquareSize() * PIECE_SCALE;

	QTransform transform;
	transform.translate(center.x(), center.y());
	transform.scale(scale, scale);
	const QPainterPath solid = transform.map(shapes.solid[type]);
	const QPainterPath detail = transform.map(shapes.detail[type]);

	if (color == WHITE) {
		painter.fillPath(solid, WHITE_PIECE_FILL);
		if (detail.isEmpty()) {
			painter.strokePath(solid, QPen(WHITE_PIECE_DETAIL, std::max(1.0, scale * 0.015)));
		} else {
			painter.fillPath(detail, WHITE_PIECE_DETAIL);
		}
	} else {
		painter.fillPath(solid, BLACK_PIECE_FILL);
		painter.strokePath(solid, QPen(BLACK_PIECE_OUTLINE, std::max(1.0, scale * 0.012)));
	}
}

//Straight arrow, or an L-shaped one for knight jumps (like chess.com).
void BoardWidget::paintArrow(QPainter& painter, Square source, Square destination, const QColor& color) const {
	if (!isValidSquare(source) || !isValidSquare(destination) || source == destination) {
		return;
	}
	const qreal size = getSquareSize();
	const qreal shaftWidth = size * 0.2;
	const qreal headLength = size * 0.45;
	const qreal headWidth = size * 0.5;

	const QPointF start = getSquareCenter(source);
	const QPointF tip = getSquareCenter(destination);
	std::vector<QPointF> points = {start};

	const int fileDistance = std::abs(getFile(destination) - getFile(source));
	const int rankDistance = std::abs(getRank(destination) - getRank(source));
	if ((fileDistance == 1 && rankDistance == 2) || (fileDistance == 2 && rankDistance == 1)) {
		//go along the long leg first; screen axes are independent of orientation here
		const bool longLegVertical = rankDistance == 2;
		points.push_back(longLegVertical ? QPointF(start.x(), tip.y()) : QPointF(tip.x(), start.y()));
	}

	const QPointF lastCorner = points.back();
	const QLineF lastSegment(lastCorner, tip);
	if (lastSegment.length() <= headLength) {
		return;
	}
	const QPointF direction = (tip - lastCorner) / lastSegment.length();
	const QPointF normal(-direction.y(), direction.x());
	const QPointF headBase = tip - direction * headLength;

	QPainterPath shaftLine(points.front());
	for (std::size_t index = 1; index < points.size(); ++index) {
		shaftLine.lineTo(points[index]);
	}
	shaftLine.lineTo(headBase);

	QPainterPathStroker stroker;
	stroker.setWidth(shaftWidth);
	stroker.setCapStyle(Qt::FlatCap);
	stroker.setJoinStyle(Qt::MiterJoin);
	QPainterPath arrow = stroker.createStroke(shaftLine);

	QPainterPath head;
	head.moveTo(tip);
	head.lineTo(headBase + normal * (headWidth / 2));
	head.lineTo(headBase - normal * (headWidth / 2));
	head.closeSubpath();

	arrow = arrow.united(head);
	arrow.setFillRule(Qt::WindingFill);
	painter.fillPath(arrow, color);
}

// ================= input =================

void BoardWidget::mousePressEvent(QMouseEvent* event) {
	const QPointF cursor = event->position();
	const Square square = getSquareAt(cursor);

	if (event->button() == Qt::RightButton) {
		if (inputState == InputState::DRAGGING) {
			endInteraction(true);	//right click cancels a drag
		}
		annotationStartSquare = square;
		annotationCurrentSquare = square;
		update();
		return;
	}

	if (event->button() == Qt::LeftButton) {
		annotationStartSquare = NO_SQUARE;
		annotationCurrentSquare = NO_SQUARE;
		if (!userHighlights.empty() || !userArrows.empty()) {
			userHighlights.clear();
			userArrows.clear();
			emit userAnnotationsChanged(userHighlights, userArrows);
		}
		if (interactive) {
			handleLeftPress(square, cursor);
		}
		update();
	}
}

void BoardWidget::mouseMoveEvent(QMouseEvent* event) {
	if (inputState == InputState::DRAGGING) {
		dragCursor = event->position();
		update();
	}
	if ((event->buttons() & Qt::RightButton) && isValidSquare(annotationStartSquare)) {
		const Square square = getSquareAt(event->position());
		if (square != annotationCurrentSquare) {
			annotationCurrentSquare = square;
			update();
		}
	}
}

void BoardWidget::mouseReleaseEvent(QMouseEvent* event) {
	const Square square = getSquareAt(event->position());
	if (event->button() == Qt::LeftButton && interactive) {
		handleLeftRelease(square);
	} else if (event->button() == Qt::RightButton) {
		handleRightRelease(square);
	}
	update();
}

void BoardWidget::handleLeftPress(Square square, const QPointF& cursor) {
	if (!isValidSquare(square)) {
		endInteraction(true);
		return;
	}
	Color color;
	PieceType type;
	const bool hasPiece = getPieceAt(square, color, type);

	switch (inputState) {
		case InputState::IDLE:
			if (hasPiece) {
				beginDrag(square, cursor, false);
			}
			break;

		case InputState::DIRTY_SELECT: {
			if (square == selectedSquare) {
				beginDrag(square, cursor, true);	//may become a drag, or a deselect on release
				break;
			}
			Color selectedColor;
			PieceType selectedType;
			const bool selectedHasPiece = getPieceAt(selectedSquare, selectedColor, selectedType);
			const bool isListedMove = !getLegalMovesBetween(selectedSquare, square).empty();
			if (!isListedMove && hasPiece && selectedHasPiece && color == selectedColor) {
				beginDrag(square, cursor, false);	//switch to another piece of the same color
				break;
			}
			attemptMove(selectedSquare, square);
			break;
		}

		case InputState::DRAGGING:
			break;
	}
}

void BoardWidget::handleLeftRelease(Square square) {
	if (inputState != InputState::DRAGGING) {
		return;
	}
	if (square == selectedSquare) {
		if (dragStartedFromDirtySelect) {
			endInteraction(true);	//clicked the dirty-selected piece again: deselect
		} else {
			inputState = InputState::DIRTY_SELECT;	//dirty select: wait for the target click
			unsetCursor();
		}
	} else if (!isValidSquare(square)) {
		endInteraction(true);	//dropped outside the board
	} else {
		attemptMove(selectedSquare, square);
	}
}

void BoardWidget::handleRightRelease(Square square) {
	if (isValidSquare(annotationStartSquare) && isValidSquare(square)) {
		if (square == annotationStartSquare) {
			toggleUserHighlight(Highlight(square));
		} else {
			toggleUserArrow(Arrow(annotationStartSquare, square));
		}
	}
	annotationStartSquare = NO_SQUARE;
	annotationCurrentSquare = NO_SQUARE;
}

void BoardWidget::beginDrag(Square square, const QPointF& cursor, bool fromDirtySelect) {
	const bool selectionChanged = selectedSquare != square;
	selectedSquare = square;
	inputState = InputState::DRAGGING;
	dragStartedFromDirtySelect = fromDirtySelect;
	dragCursor = cursor;
	setCursor(Qt::ClosedHandCursor);
	if (selectionChanged) {
		emit squareSelected(square);	//the core may answer with setLegalMoves()
	}
}

void BoardWidget::endInteraction(bool notify) {
	const bool hadSelection = isValidSquare(selectedSquare);
	inputState = InputState::IDLE;
	selectedSquare = NO_SQUARE;
	dragStartedFromDirtySelect = false;
	unsetCursor();
	update();
	if (notify && hadSelection) {
		emit selectionCleared();
	}
}

//No legality check: if the legal move list knows this move, the full move (with flags) is sent,
//otherwise a bare source/destination move. Several matches mean a promotion choice.
void BoardWidget::attemptMove(Square source, Square destination) {
	const std::vector<Move> candidates = getLegalMovesBetween(source, destination);
	std::optional<Move> move;

	if (candidates.empty()) {
		move = makeMove(source, destination);
	} else if (candidates.size() == 1) {
		move = candidates.front();
	} else {
		inputState = InputState::DIRTY_SELECT;	//piece back on its square while the menu is open
		unsetCursor();
		repaint();
		move = choosePromotion(candidates);
	}

	endInteraction(true);
	if (move) {
		emit moveAttempted(*move);
	}
}

std::optional<Move> BoardWidget::choosePromotion(std::vector<Move> candidates) {
	std::sort(candidates.begin(), candidates.end(), [](Move first, Move second) {
		return getPromotionType(first) > getPromotionType(second);
	});

	QMenu menu(this);
	for (Move candidate : candidates) {
		QAction* action = menu.addAction(getPieceName(getPromotionType(candidate)));
		action->setData(QVariant::fromValue(static_cast<uint>(candidate)));
	}
	const QAction* chosen = menu.exec(QCursor::pos());
	if (chosen == nullptr) {
		return std::nullopt;
	}
	return static_cast<Move>(chosen->data().toUInt());
}

void BoardWidget::toggleUserHighlight(Highlight highlight) {
	const auto found = std::find(userHighlights.begin(), userHighlights.end(), highlight);
	if (found != userHighlights.end()) {
		userHighlights.erase(found);
	} else {
		userHighlights.push_back(highlight);
	}
	emit userAnnotationsChanged(userHighlights, userArrows);
	update();
}

void BoardWidget::toggleUserArrow(Arrow arrow) {
	const auto found = std::find(userArrows.begin(), userArrows.end(), arrow);
	if (found != userArrows.end()) {
		userArrows.erase(found);
	} else {
		userArrows.push_back(arrow);
	}
	emit userAnnotationsChanged(userHighlights, userArrows);
	update();
}
