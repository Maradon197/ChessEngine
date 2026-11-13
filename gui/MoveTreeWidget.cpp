#include "MoveTreeWidget.hpp"

#include <algorithm>
#include <map>
#include <utility>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "Doorman.hpp"

namespace {

constexpr int COLUMN_WIDTH = 88;
constexpr int NODE_WIDTH = 76;
constexpr int ROW_HEIGHT = 30;
constexpr int NODE_HEIGHT = 22;
constexpr int MARGIN = 8;

} // namespace

//Which column ranges are taken in each row, so variations don't overlap.
struct MoveTreeWidget::RowOccupancy {
	std::map<int, std::vector<std::pair<int, int>>> rows;

	bool isFree(int row, int firstColumn, int lastColumn) const {
		const auto found = rows.find(row);
		if (found == rows.end()) {
			return true;
		}
		for (const auto& [first, last] : found->second) {
			if (firstColumn <= last && first <= lastColumn) {
				return false;
			}
		}
		return true;
	}

	void occupy(int row, int firstColumn, int lastColumn) {
		rows[row].emplace_back(firstColumn, lastColumn);
	}
};

//Painting surface inside the scroll area; forwards everything to the MoveTreeWidget.
class MoveTreeCanvas : public QWidget {
public:
	explicit MoveTreeCanvas(MoveTreeWidget& tree)
		: tree(tree) {
		setFocusPolicy(Qt::NoFocus);
	}

protected:
	void paintEvent(QPaintEvent* event) override {
		Q_UNUSED(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		tree.paintTree(painter);
	}

	void mousePressEvent(QMouseEvent* event) override {
		if (event->button() == Qt::LeftButton) {
			tree.handleClick(event->position().toPoint());
		}
	}

private:
	MoveTreeWidget& tree;
};

MoveTreeWidget::MoveTreeWidget(Doorman& doorman, QWidget* parent)
	: QScrollArea(parent) {
	canvas = new MoveTreeCanvas(*this);
	setWidget(canvas);
	setWidgetResizable(false);
	setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	setFocusPolicy(Qt::StrongFocus);
	setMinimumHeight(3 * ROW_HEIGHT + 2 * MARGIN + 2 * frameWidth());
	clearTree();
	doorman.attach(*this);
}

// ================= public interface =================

void MoveTreeWidget::setOrigin(Clock fullmoveNumber, Color sideToMove) {
	originFullmove = fullmoveNumber;
	originSide = sideToMove;
	canvas->update();
}

void MoveTreeWidget::addNode(NodeId id, NodeId parent, Move move, const QString& label, const Position& position) {
	if (id == ROOT_NODE || nodes.contains(id)) {
		return;
	}
	const auto parentNode = nodes.find(parent);
	if (parentNode == nodes.end()) {
		qWarning("MoveTreeWidget::addNode: unknown parent %u", parent);
		return;
	}

	Node node;
	node.parent = parent;
	node.move = move;
	node.label = label;
	node.position = position;
	node.depth = parentNode->second.depth + 1;
	parentNode->second.children.push_back(id);
	nodes.emplace(id, std::move(node));
	relayout();
}

void MoveTreeWidget::removeNode(NodeId id) {
	if (id == ROOT_NODE) {
		clearTree();
		return;
	}
	const auto found = nodes.find(id);
	if (found == nodes.end()) {
		return;
	}

	const NodeId parent = found->second.parent;
	std::vector<NodeId>& siblings = nodes.at(parent).children;
	std::erase(siblings, id);

	std::vector<NodeId> pending = {id};
	while (!pending.empty()) {
		const NodeId current = pending.back();
		pending.pop_back();
		const auto node = nodes.find(current);
		if (node == nodes.end()) {
			continue;
		}
		pending.insert(pending.end(), node->second.children.begin(), node->second.children.end());
		if (current == currentNode) {
			currentNode = parent;
		}
		nodes.erase(node);
	}
	relayout();
}

void MoveTreeWidget::clearTree() {
	nodes.clear();
	nodes.emplace(ROOT_NODE, Node{});
	currentNode = ROOT_NODE;
	relayout();
}

void MoveTreeWidget::setCurrentNode(NodeId id) {
	selectNode(id, false);
}

NodeId MoveTreeWidget::getCurrentNode() const {
	return currentNode;
}

bool MoveTreeWidget::hasNode(NodeId id) const {
	return nodes.contains(id);
}

std::optional<NodeId> MoveTreeWidget::findChild(NodeId parent, Move move) const {
	const auto parentNode = nodes.find(parent);
	if (parentNode == nodes.end()) {
		return std::nullopt;
	}
	for (NodeId child : parentNode->second.children) {
		if (nodes.at(child).move == move) {
			return child;
		}
	}
	return std::nullopt;
}

void MoveTreeWidget::setNodePosition(NodeId id, const Position& position) {
	const auto found = nodes.find(id);
	if (found != nodes.end()) {
		found->second.position = position;
	}
}

Position MoveTreeWidget::getNodePosition(NodeId id) const {
	const auto found = nodes.find(id);
	return found != nodes.end() ? found->second.position : Position{};
}

Move MoveTreeWidget::getNodeMove(NodeId id) const {
	const auto found = nodes.find(id);
	return found != nodes.end() ? found->second.move : Move{0};
}

std::vector<Move> MoveTreeWidget::getMovesToNode(NodeId id) const {
	std::vector<Move> moves;
	for (auto found = nodes.find(id); found != nodes.end() && found->first != ROOT_NODE; found = nodes.find(found->second.parent)) {
		moves.push_back(found->second.move);
	}
	std::reverse(moves.begin(), moves.end());
	return moves;
}

void MoveTreeWidget::setPanelColor(const QColor& color) {
	QPalette colors = viewport()->palette();
	colors.setColor(QPalette::Window, color);
	viewport()->setPalette(colors);
	viewport()->setAutoFillBackground(true);
	canvas->update();
}

// ================= layout =================

void MoveTreeWidget::relayout() {
	RowOccupancy occupancy;
	minRow = 0;
	maxRow = 0;
	maxColumn = 0;
	layoutLine(ROOT_NODE, 0, occupancy);

	canvas->setFixedSize(2 * MARGIN + maxColumn * COLUMN_WIDTH + NODE_WIDTH,
		2 * MARGIN + (maxRow - minRow) * ROW_HEIGHT + NODE_HEIGHT);
	canvas->update();
}

//Places a line (a node and its chain of first children) in one row, then its variations.
//Variations of the main line alternate below/above; nested ones keep moving away from the main line.
void MoveTreeWidget::layoutLine(NodeId first, int row, RowOccupancy& occupancy) {
	std::vector<NodeId> line;
	for (NodeId id = first;;) {
		Node& node = nodes.at(id);
		node.column = node.depth;
		node.row = row;
		line.push_back(id);
		if (node.children.empty()) {
			break;
		}
		id = node.children.front();
	}

	const int firstColumn = nodes.at(first).column;
	const int lastColumn = nodes.at(line.back()).column;
	occupancy.occupy(row, firstColumn, lastColumn);
	minRow = std::min(minRow, row);
	maxRow = std::max(maxRow, row);
	maxColumn = std::max(maxColumn, lastColumn);

	for (NodeId id : line) {
		const std::vector<NodeId> children = nodes.at(id).children;
		for (std::size_t index = 1; index < children.size(); ++index) {
			const NodeId child = children[index];
			const int direction = row != 0 ? (row > 0 ? 1 : -1) : (index % 2 == 1 ? 1 : -1);
			const int variationFirst = nodes.at(child).depth;
			const int variationLast = variationFirst + getLineLength(child) - 1;

			int targetRow = row + direction;
			while (!occupancy.isFree(targetRow, variationFirst, variationLast)) {
				targetRow += direction;
			}
			layoutLine(child, targetRow, occupancy);
		}
	}
}

int MoveTreeWidget::getLineLength(NodeId first) const {
	int length = 1;
	for (const Node* node = &nodes.at(first); !node->children.empty(); node = &nodes.at(node->children.front())) {
		++length;
	}
	return length;
}

QRect MoveTreeWidget::getNodeRect(const Node& node) const {
	return {MARGIN + node.column * COLUMN_WIDTH, MARGIN + (node.row - minRow) * ROW_HEIGHT, NODE_WIDTH, NODE_HEIGHT};
}

QString MoveTreeWidget::getDisplayText(const Node& node) const {
	if (node.depth == 0) {
		return tr("Start");
	}
	const int ply = node.depth - 1 + (originSide == BLACK ? 1 : 0);
	const int moveNumber = originFullmove + ply / 2;
	const bool isWhiteMove = ply % 2 == 0;
	return QString("%1%2 %3").arg(moveNumber).arg(isWhiteMove ? "." : QString::fromUtf8("…")).arg(node.label);
}

// ================= selection / input =================

void MoveTreeWidget::selectNode(NodeId id, bool notify) {
	const auto found = nodes.find(id);
	if (found == nodes.end()) {
		return;
	}
	currentNode = id;
	const QRect rect = getNodeRect(found->second);
	ensureVisible(rect.center().x(), rect.center().y(), COLUMN_WIDTH, ROW_HEIGHT);
	canvas->update();
	if (notify) {
		emit nodeSelected(id);
	}
}

void MoveTreeWidget::handleClick(const QPoint& point) {
	setFocus();
	for (const auto& [id, node] : nodes) {
		if (getNodeRect(node).contains(point)) {
			selectNode(id, true);
			return;
		}
	}
}

void MoveTreeWidget::keyPressEvent(QKeyEvent* event) {
	const Node& current = nodes.at(currentNode);
	if (event->key() == Qt::Key_Left) {
		if (currentNode != ROOT_NODE) {
			selectNode(current.parent, true);
		}
		return;
	}
	if (event->key() == Qt::Key_Right) {
		if (!current.children.empty()) {
			selectNode(current.children.front(), true);
		}
		return;
	}
	QScrollArea::keyPressEvent(event);
}

// ================= painting =================

void MoveTreeWidget::paintTree(QPainter& painter) const {
	const QPalette& colors = palette();

	painter.setPen(QPen(colors.color(QPalette::Mid), 1.5));
	painter.setBrush(Qt::NoBrush);
	for (const auto& [id, node] : nodes) {
		if (id == ROOT_NODE) {
			continue;
		}
		const Node& parent = nodes.at(node.parent);
		const QRect parentRect = getNodeRect(parent);
		const QRect childRect = getNodeRect(node);
		const QPointF start(parentRect.right() + 1, parentRect.center().y());
		const QPointF end(childRect.left(), childRect.center().y());
		if (parent.row == node.row) {
			painter.drawLine(start, end);
		} else {
			const qreal middleX = (start.x() + end.x()) / 2;
			QPainterPath connector(start);
			connector.lineTo(middleX, start.y());
			connector.lineTo(middleX, end.y());
			connector.lineTo(end);
			painter.drawPath(connector);
		}
	}

	for (const auto& [id, node] : nodes) {
		const QRect rect = getNodeRect(node);
		const bool isCurrent = id == currentNode;
		const bool isMainLine = node.row == 0;
		const QColor fill = isCurrent ? colors.color(QPalette::Highlight)
			: isMainLine ? colors.color(QPalette::Base) : colors.color(QPalette::AlternateBase);

		painter.setPen(QPen(colors.color(QPalette::Mid), 1));
		painter.setBrush(fill);
		painter.drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

		painter.setPen(isCurrent ? colors.color(QPalette::HighlightedText) : colors.color(QPalette::Text));
		const QString text = painter.fontMetrics().elidedText(getDisplayText(node), Qt::ElideRight, rect.width() - 8);
		painter.drawText(rect, Qt::AlignCenter, text);
	}
}
