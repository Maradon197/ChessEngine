#ifndef MOVETREEWIDGET_HPP
#define MOVETREEWIDGET_HPP

#include <optional>
#include <unordered_map>
#include <vector>

#include <QScrollArea>

#include "DataTypes.hpp"
#include "GuiTypes.hpp"

class Doorman;
class MoveTreeCanvas;
class QPainter;

class MoveTreeWidget : public QScrollArea {
	Q_OBJECT

public:
	explicit MoveTreeWidget(Doorman& doorman, QWidget* parent = nullptr);

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

signals:
	void nodeSelected(NodeId id);

protected:
	void keyPressEvent(QKeyEvent* event) override;

private:
	friend class MoveTreeCanvas;

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
	struct RowOccupancy;

	void relayout();
	void layoutLine(NodeId first, int row, RowOccupancy& occupancy);
	int getLineLength(NodeId first) const;
	QRect getNodeRect(const Node& node) const;
	QString getDisplayText(const Node& node) const;
	void selectNode(NodeId id, bool notify);
	void paintTree(QPainter& painter) const;
	void handleClick(const QPoint& point);

	MoveTreeCanvas* canvas = nullptr;
	std::unordered_map<NodeId, Node> nodes;
	NodeId currentNode = ROOT_NODE;
	Clock originFullmove = 1;
	Color originSide = WHITE;
	int minRow = 0;
	int maxRow = 0;
	int maxColumn = 0;
};

#endif
