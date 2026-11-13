#ifndef BOARDWIDGET_HPP
#define BOARDWIDGET_HPP

#include <array>
#include <optional>
#include <vector>

#include <QColor>
#include <QWidget>

#include "DataTypes.hpp"
#include "GuiTypes.hpp"

class Doorman;
class QPainter;

class BoardWidget : public QWidget {
	Q_OBJECT

public:
	explicit BoardWidget(Doorman& doorman, QWidget* parent = nullptr);

	void setPosition(const Position& position);
	const Position& getPosition() const;
	void playMove(Move move);

	void setLegalMoves(const MoveList& legalMoves);
	void clearLegalMoves();

	void setLastMove(Move move);
	void clearLastMove();

	void setOrientation(Color bottomColor);
	Color getOrientation() const;

	void setInteractive(bool interactive);
	bool isInteractive() const;

	void setUserHighlights(const std::vector<Highlight>& highlights);
	void setUserArrows(const std::vector<Arrow>& arrows);
	void clearUserAnnotations();
	const std::vector<Highlight>& getUserHighlights() const;
	const std::vector<Arrow>& getUserArrows() const;

	void setEngineHighlights(const std::vector<Highlight>& highlights);
	void setEngineArrows(const std::vector<Arrow>& arrows);
	void clearEngineAnnotations();

	void setColor(ColorRole role, const QColor& color);
	const QColor& getColor(ColorRole role) const;

	QSize sizeHint() const override;

signals:
	void moveAttempted(Move move);
	void squareSelected(Square square);
	void selectionCleared();
	void userAnnotationsChanged(const std::vector<Highlight>& highlights, const std::vector<Arrow>& arrows);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	enum class InputState {IDLE, DRAGGING, DIRTY_SELECT};

	int getSquareSize() const;
	QRect getBoardRect() const;
	QRectF getSquareRect(Square square) const;
	QPointF getSquareCenter(Square square) const;
	Square getSquareAt(const QPointF& point) const;

	bool getPieceAt(Square square, Color& color, PieceType& type) const;
	std::vector<Move> getLegalMovesFrom(Square source) const;
	std::vector<Move> getLegalMovesBetween(Square source, Square destination) const;

	void paintBoard(QPainter& painter) const;
	void paintSelections(QPainter& painter) const;
	void paintPieces(QPainter& painter) const;
	void paintLegalMoves(QPainter& painter) const;
	void paintArrows(QPainter& painter) const;
	void paintDraggedPiece(QPainter& painter) const;
	void paintPiece(QPainter& painter, Color color, PieceType type, const QPointF& center) const;
	void paintArrow(QPainter& painter, Square source, Square destination, const QColor& color) const;

	void handleLeftPress(Square square, const QPointF& cursor);
	void handleLeftRelease(Square square);
	void handleRightRelease(Square square);
	void beginDrag(Square square, const QPointF& cursor, bool fromDirtySelect);
	void endInteraction(bool notify);
	void attemptMove(Square source, Square destination);
	std::optional<Move> choosePromotion(std::vector<Move> candidates);
	void toggleUserHighlight(Highlight highlight);
	void toggleUserArrow(Arrow arrow);

	std::array<QColor, COLOR_ROLE_COUNT> colors;
	Position position{};
	MoveList legalMoves{};
	std::optional<Move> lastMove;
	Color orientation = WHITE;
	bool interactive = true;

	std::vector<Highlight> userHighlights;
	std::vector<Arrow> userArrows;
	std::vector<Highlight> engineHighlights;
	std::vector<Arrow> engineArrows;

	InputState inputState = InputState::IDLE;
	Square selectedSquare = NO_SQUARE;
	bool dragStartedFromDirtySelect = false;
	QPointF dragCursor;
	Square annotationStartSquare = NO_SQUARE;
	Square annotationCurrentSquare = NO_SQUARE;
};

#endif
