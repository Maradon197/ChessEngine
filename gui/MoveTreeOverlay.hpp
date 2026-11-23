#ifndef MOVETREEOVERLAY_HPP
#define MOVETREEOVERLAY_HPP

#include <optional>
#include <unordered_map>
#include <vector>

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QWidget>

#include "DataTypes.hpp"
#include "GuiTypes.hpp"

class Doorman;
class KeyBindings;
class QPainter;
class QTimer;
class QVariantAnimation;

//covers the whole window while its key is held (or until it is pressed again): the hidden cursor stays in the middle,
//mouse movement pans the tree underneath it and the node closest to the middle becomes the current node
class MoveTreeOverlay : public QWidget {
	Q_OBJECT

public:
	MoveTreeOverlay(Doorman& doorman, KeyBindings& keyBindings, QWidget& window);

	void setOrigin(Clock fullmoveNumber, Color sideToMove);
	void addNode(NodeId id, NodeId parent, Move move, const QString& label, const Position& position);
	void removeNode(NodeId id);
	void clearTree();
	void setCurrentNode(NodeId id);
	NodeId getCurrentNode() const;
	bool hasNode(NodeId id) const;
	std::optional<NodeId> findChild(NodeId parent, Move move) const;

	void setNodePosition(NodeId id, const Position& position);
	Position getNodePosition(NodeId id) const;
	Move getNodeMove(NodeId id) const;
	std::vector<Move> getMovesToNode(NodeId id) const;

	void setPanelColor(const QColor& color);
	void endNavigation();	//closes the overlay if it is open

signals:
	void nodeSelected(NodeId id);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;

private:
	struct Node {
		NodeId parent = ROOT_NODE;
		Move move = 0;
		QString label;
		Position position{};
		std::vector<NodeId> children;
		int depth = 0;
		int column = 0;
		int row = 0;
	};

	void relayout();	//same line layout as the search window's tree, see TreeLayout.hpp
	QPointF getNodeCenter(const Node& node) const;
	QRectF getNodeRect(const Node& node) const;
	QString getDisplayText(const Node& node) const;
	void selectNode(NodeId id, bool notify);
	void step(KeyAction action);

	void beginNavigation();
	void fadeTo(qreal target);
	void recenterCursor();
	void pan(QPointF delta);
	void clampPan();
	void selectClosestNode();
	qreal getTargetViewY() const;
	QPointF toScreen(const QPointF& treePoint) const;

	void paintTree(QPainter& painter) const;
	void paintFlash(QPainter& painter) const;

	std::unordered_map<NodeId, Node> nodes;
	NodeId currentNode = ROOT_NODE;
	Clock originFullmove = 1;
	Color originSide = WHITE;
	int minRow = 0;
	int maxRow = 0;
	int maxColumn = 0;

	QColor panelColor;
	QVariantAnimation* fade = nullptr;
	QVariantAnimation* flash = nullptr;	//edge flash when the selection switches line
	qreal flashStrength = 0;
	QTimer* frameTimer = nullptr;
	qreal opacity = 0;
	bool navigating = false;
	QPoint savedCursor;	//global, restored when M is released
	QPointF cursorAnchor;	//global position the cursor is pinned to
	QPointF panPoint;	//tree point under the cursor, before row snapping
	int snappedRow = 0;
	qreal viewY = 0;	//smoothed vertical view position
};

#endif
