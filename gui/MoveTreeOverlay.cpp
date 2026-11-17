#include "MoveTreeOverlay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

#include <QApplication>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVariantAnimation>

#include "Doorman.hpp"

namespace {

constexpr qreal SCALE = 1.15;
constexpr qreal COLUMN_WIDTH = 104 * SCALE;
constexpr qreal NODE_WIDTH = 88 * SCALE;
constexpr qreal ROW_HEIGHT = 60 * SCALE;	//roomier than the columns so rows are harder to switch by accident
constexpr qreal NODE_HEIGHT = 34 * SCALE;
constexpr qreal TEXT_PADDING = 10 * SCALE;
constexpr qreal FONT_SCALE = 1.15 * SCALE;

constexpr qreal HORIZONTAL_GAIN = 0.95 * SCALE;	//tree pixels per mouse pixel, scaled so mouse travel per move stays the same
constexpr qreal VERTICAL_GAIN = 0.45 * SCALE;
constexpr qreal SKIM_RATIO = 1.5;	//movement counts as a skim along the row when |dx| > SKIM_RATIO * |dy|
constexpr qreal SKIM_DAMPING = 0.3;	//vertical movement left over while skimming
constexpr qreal SNAP_THRESHOLD = 0.75;	//rows the cursor has to leave the snapped row by before it switches
constexpr qreal VISUAL_DRIFT = 0.35;	//how much of the unsnapped offset the view follows
constexpr qreal VERTICAL_WEIGHT = 2.0;	//vertical distance counts more when looking for the closest node

constexpr int FADE_DURATION = 140;
constexpr int BACKDROP_ALPHA = 110;
constexpr qreal NODE_OPACITY = 0.88;
constexpr qreal EDGE_OPACITY = 0.7;
constexpr int FLASH_DURATION = 380;
constexpr int FLASH_ALPHA = 70;
constexpr qreal FLASH_WIDTH = 70;

} // !namespace

struct MoveTreeOverlay::RowOccupancy {
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

MoveTreeOverlay::MoveTreeOverlay(Doorman& doorman, QWidget& window)
	: QWidget(&window) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setFocusPolicy(Qt::NoFocus);
	setGeometry(window.rect());
	hide();

	fade = new QVariantAnimation(this);
	connect(fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
		opacity = value.toReal();
		update();
	});
	connect(fade, &QVariantAnimation::finished, this, [this] {
		if (!navigating) {
			frameTimer->stop();
			hide();
		}
	});

	flash = new QVariantAnimation(this);
	flash->setStartValue(1.0);
	flash->setEndValue(0.0);
	flash->setDuration(FLASH_DURATION);
	flash->setEasingCurve(QEasingCurve::OutQuad);
	connect(flash, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
		flashStrength = value.toReal();
		update();
	});

	frameTimer = new QTimer(this);
	frameTimer->setInterval(16);
	connect(frameTimer, &QTimer::timeout, this, [this] {
		const qreal target = getTargetViewY();
		if (std::abs(target - viewY) > 0.1) {
			viewY += (target - viewY) * 0.35;
			update();
		}
	});

	window.installEventFilter(this);
	qApp->installEventFilter(this);	//M and the arrow keys work no matter which widget has focus

	clearTree();
	doorman.attach(*this);
}

void MoveTreeOverlay::setOrigin(Clock fullmoveNumber, Color sideToMove) {
	originFullmove = fullmoveNumber;
	originSide = sideToMove;
	update();
}

void MoveTreeOverlay::addNode(NodeId id, NodeId parent, Move move, const QString& label, const Position& position) {
	if (id == ROOT_NODE || nodes.contains(id)) {
		return;
	}
	const auto parentNode = nodes.find(parent);
	if (parentNode == nodes.end()) {
		qWarning("MoveTreeOverlay::addNode: unknown parent %u", parent);
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

void MoveTreeOverlay::removeNode(NodeId id) {
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

void MoveTreeOverlay::clearTree() {
	nodes.clear();
	nodes.emplace(ROOT_NODE, Node{});
	currentNode = ROOT_NODE;
	relayout();
}

void MoveTreeOverlay::setCurrentNode(NodeId id) {
	selectNode(id, false);
}

NodeId MoveTreeOverlay::getCurrentNode() const {
	return currentNode;
}

bool MoveTreeOverlay::hasNode(NodeId id) const {
	return nodes.contains(id);
}

std::optional<NodeId> MoveTreeOverlay::findChild(NodeId parent, Move move) const {
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

void MoveTreeOverlay::setNodePosition(NodeId id, const Position& position) {
	const auto found = nodes.find(id);
	if (found != nodes.end()) {
		found->second.position = position;
	}
}

Position MoveTreeOverlay::getNodePosition(NodeId id) const {
	const auto found = nodes.find(id);
	return found != nodes.end() ? found->second.position : Position{};
}

Move MoveTreeOverlay::getNodeMove(NodeId id) const {
	const auto found = nodes.find(id);
	return found != nodes.end() ? found->second.move : Move{0};
}

std::vector<Move> MoveTreeOverlay::getMovesToNode(NodeId id) const {
	std::vector<Move> moves;
	for (auto found = nodes.find(id); found != nodes.end() && found->first != ROOT_NODE; found = nodes.find(found->second.parent)) {
		moves.push_back(found->second.move);
	}
	std::reverse(moves.begin(), moves.end());
	return moves;
}

void MoveTreeOverlay::setPanelColor(const QColor& color) {
	panelColor = color;
	update();
}

void MoveTreeOverlay::relayout() {
	RowOccupancy occupancy;
	minRow = 0;
	maxRow = 0;
	maxColumn = 0;
	layoutLine(ROOT_NODE, 0, occupancy);
	clampPan();
	update();
}

void MoveTreeOverlay::layoutLine(NodeId first, int row, RowOccupancy& occupancy) {
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
			const int direction = row != 0 ? (row > 0 ? 1 : -1) : (index % 2 == 1 ? 1 : -1);	//nested variations move away from the main line
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

int MoveTreeOverlay::getLineLength(NodeId first) const {
	int length = 1;
	for (const Node* node = &nodes.at(first); !node->children.empty(); node = &nodes.at(node->children.front())) {
		++length;
	}
	return length;
}

QPointF MoveTreeOverlay::getNodeCenter(const Node& node) const {
	return {node.column * COLUMN_WIDTH, node.row * ROW_HEIGHT};
}

QRectF MoveTreeOverlay::getNodeRect(const Node& node) const {
	const QPointF center = getNodeCenter(node);
	return {center.x() - NODE_WIDTH / 2, center.y() - NODE_HEIGHT / 2, NODE_WIDTH, NODE_HEIGHT};
}

QString MoveTreeOverlay::getDisplayText(const Node& node) const {
	if (node.depth == 0) {
		return tr("Start");
	}
	const int ply = node.depth - 1 + (originSide == BLACK ? 1 : 0);
	const int moveNumber = originFullmove + ply / 2;
	const bool isWhiteMove = ply % 2 == 0;
	return QString("%1%2 %3").arg(moveNumber).arg(isWhiteMove ? "." : QString::fromUtf8("…")).arg(node.label);
}

void MoveTreeOverlay::selectNode(NodeId id, bool notify) {
	const auto found = nodes.find(id);
	if (found == nodes.end()) {
		return;
	}
	if (navigating && found->second.row != nodes.at(currentNode).row) {
		flash->stop();
		flash->start();	//switched to another line
	}
	currentNode = id;
	update();
	if (notify) {
		emit nodeSelected(id);
	}
}

bool MoveTreeOverlay::eventFilter(QObject* watched, QEvent* event) {
	if (watched == parentWidget()) {
		if (event->type() == QEvent::Resize) {
			setGeometry(parentWidget()->rect());
			return false;
		}
		if (event->type() == QEvent::WindowDeactivate) {
			endNavigation();
			return false;
		}
	}
	if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) {
		return false;
	}
	auto* receiver = qobject_cast<QWidget*>(watched);
	if (receiver == nullptr || receiver->window() != window()) {
		return false;	//popups and dialogs keep their keys
	}
	return handleKey(*static_cast<QKeyEvent*>(event), *receiver);
}

bool MoveTreeOverlay::handleKey(QKeyEvent& event, QWidget& receiver) {
	const bool isPress = event.type() == QEvent::KeyPress;
	const bool isTextInput = receiver.inherits("QLineEdit") || (receiver.inherits("QTextEdit") && !receiver.property("readOnly").toBool());
	if (isTextInput && !navigating) {
		return false;	//typing an m
	}
	if (event.key() == Qt::Key_M) {
		if (!event.isAutoRepeat()) {
			if (isPress) {
				beginNavigation();
			} else {
				endNavigation();
			}
		}
		return true;
	}

	if (!isPress || navigating || receiver.inherits("QComboBox") || receiver.inherits("QAbstractScrollArea") || receiver.property("ownsArrowKeys").toBool()) {
		return false;
	}
	const Node& current = nodes.at(currentNode);
	if (event.key() == Qt::Key_Left) {
		if (currentNode != ROOT_NODE) {
			selectNode(current.parent, true);
		}
		return true;
	}
	if (event.key() == Qt::Key_Right) {
		if (!current.children.empty()) {
			selectNode(current.children.front(), true);
		}
		return true;
	}
	return false;
}

void MoveTreeOverlay::beginNavigation() {
	if (navigating) {
		return;
	}
	navigating = true;

	const Node& current = nodes.at(currentNode);
	panPoint = getNodeCenter(current);
	snappedRow = current.row;
	viewY = getTargetViewY();

	setGeometry(parentWidget()->rect());
	setAttribute(Qt::WA_TransparentForMouseEvents, false);
	show();
	raise();

	savedCursor = QCursor::pos();
	setMouseTracking(true);
	grabMouse(Qt::BlankCursor);
	recenterCursor();

	frameTimer->start();
	fadeTo(1);
}

void MoveTreeOverlay::endNavigation() {
	if (!navigating) {
		return;
	}
	navigating = false;

	releaseMouse();
	setMouseTracking(false);
	setAttribute(Qt::WA_TransparentForMouseEvents);
	QCursor::setPos(savedCursor);

	fadeTo(0);
}

void MoveTreeOverlay::fadeTo(qreal target) {
	fade->stop();
	fade->setStartValue(opacity);
	fade->setEndValue(target);
	fade->setDuration(std::max(1, static_cast<int>(FADE_DURATION * std::abs(target - opacity))));
	fade->start();
}

//pins the cursor to the middle so it never hits the screen edge; the actual position is read back
//because the platform may round it, which would otherwise show up as movement
void MoveTreeOverlay::recenterCursor() {
	QCursor::setPos(mapToGlobal(rect().center()));
	cursorAnchor = QCursor::pos();
}

void MoveTreeOverlay::mouseMoveEvent(QMouseEvent* event) {
	if (!navigating) {
		return;
	}
	const QPointF delta = event->globalPosition() - cursorAnchor;
	if (std::abs(delta.x()) < 0.5 && std::abs(delta.y()) < 0.5) {
		return;	//the move caused by recentering
	}
	recenterCursor();
	pan(delta);
}

void MoveTreeOverlay::pan(QPointF delta) {
	const qreal dx = delta.x() * HORIZONTAL_GAIN;
	qreal dy = delta.y() * VERTICAL_GAIN;
	const qreal rowY = snappedRow * ROW_HEIGHT;
	if (std::abs(dx) > SKIM_RATIO * std::abs(dy)) {	//skimming along the row: drop most vertical drift and ease back onto the row
		dy *= SKIM_DAMPING;
		panPoint.ry() += (rowY - panPoint.y()) * std::min(0.25, std::abs(dx) * 0.02);
	}
	panPoint += QPointF(dx, dy);
	clampPan();

	if (std::abs(panPoint.y() - snappedRow * ROW_HEIGHT) > SNAP_THRESHOLD * ROW_HEIGHT) {
		snappedRow = std::clamp(static_cast<int>(std::lround(panPoint.y() / ROW_HEIGHT)), minRow, maxRow);
	}
	selectClosestNode();
	update();
}

void MoveTreeOverlay::clampPan() {
	panPoint.setX(std::clamp(panPoint.x(), 0.0, maxColumn * COLUMN_WIDTH));
	panPoint.setY(std::clamp(panPoint.y(), minRow * ROW_HEIGHT, maxRow * ROW_HEIGHT));
	snappedRow = std::clamp(snappedRow, minRow, maxRow);
}

void MoveTreeOverlay::selectClosestNode() {
	const QPointF target(panPoint.x(), snappedRow * ROW_HEIGHT);
	NodeId closest = currentNode;
	qreal closestDistance = std::numeric_limits<qreal>::max();
	for (const auto& [id, node] : nodes) {
		const QPointF offset = getNodeCenter(node) - target;
		const qreal distance = offset.x() * offset.x() + VERTICAL_WEIGHT * VERTICAL_WEIGHT * offset.y() * offset.y();
		if (distance < closestDistance) {
			closestDistance = distance;
			closest = id;
		}
	}
	if (closest != currentNode) {
		selectNode(closest, true);
	}
}

qreal MoveTreeOverlay::getTargetViewY() const {
	const qreal rowY = snappedRow * ROW_HEIGHT;
	return rowY + (panPoint.y() - rowY) * VISUAL_DRIFT;
}

QPointF MoveTreeOverlay::toScreen(const QPointF& treePoint) const {
	return treePoint - QPointF(panPoint.x(), viewY) + QRectF(rect()).center();
}

void MoveTreeOverlay::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	if (opacity <= 0) {
		return;
	}
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setOpacity(opacity);

	QColor backdrop = panelColor.isValid() ? panelColor : palette().color(QPalette::Window);
	backdrop.setAlpha(BACKDROP_ALPHA);
	painter.fillRect(rect(), backdrop);

	paintTree(painter);
	paintFlash(painter);
}

void MoveTreeOverlay::paintFlash(QPainter& painter) const {
	if (flashStrength <= 0) {
		return;
	}
	const QRectF area = rect();
	const QColor edge(255, 255, 255, static_cast<int>(FLASH_ALPHA * flashStrength));
	const QColor inner(255, 255, 255, 0);
	const auto paintEdge = [&](const QRectF& band, const QPointF& from, const QPointF& to) {
		QLinearGradient gradient(from, to);
		gradient.setColorAt(0, edge);
		gradient.setColorAt(1, inner);
		painter.fillRect(band, gradient);
	};
	const qreal width = FLASH_WIDTH;
	painter.setPen(Qt::NoPen);
	paintEdge({area.left(), area.top(), area.width(), width}, area.topLeft(), {area.left(), area.top() + width});
	paintEdge({area.left(), area.bottom() - width, area.width(), width}, area.bottomLeft(), {area.left(), area.bottom() - width});
	paintEdge({area.left(), area.top(), width, area.height()}, area.topLeft(), {area.left() + width, area.top()});
	paintEdge({area.right() - width, area.top(), width, area.height()}, area.topRight(), {area.right() - width, area.top()});
}

void MoveTreeOverlay::paintTree(QPainter& painter) const {
	const QPalette& colors = palette();
	const qreal baseOpacity = painter.opacity();
	const QRectF visible = QRectF(rect()).adjusted(-NODE_WIDTH, -NODE_HEIGHT, NODE_WIDTH, NODE_HEIGHT);

	painter.setOpacity(baseOpacity * EDGE_OPACITY);
	painter.setPen(QPen(colors.color(QPalette::Text), 1.5));
	painter.setBrush(Qt::NoBrush);
	for (const auto& [id, node] : nodes) {
		if (id == ROOT_NODE) {
			continue;
		}
		const QRectF parentRect = getNodeRect(nodes.at(node.parent)).translated(toScreen({}));
		const QRectF childRect = getNodeRect(node).translated(toScreen({}));
		const QPointF start(parentRect.right(), parentRect.center().y());
		const QPointF end(childRect.left(), childRect.center().y());
		if (start.y() == end.y()) {
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

	//the line from the cursor to the selected node
	const QPointF cursor = QRectF(rect()).center();
	const QColor highlight = colors.color(QPalette::Highlight);
	painter.setOpacity(baseOpacity);
	painter.setPen(QPen(highlight, 2.5, Qt::SolidLine, Qt::RoundCap));
	painter.drawLine(cursor, toScreen(getNodeCenter(nodes.at(currentNode))));

	QFont font = painter.font();
	font.setPointSizeF(font.pointSizeF() * FONT_SCALE);
	painter.setFont(font);
	for (const auto& [id, node] : nodes) {
		const QRectF rect = getNodeRect(node).translated(toScreen({}));
		if (!visible.contains(rect.center())) {
			continue;
		}
		const bool isCurrent = id == currentNode;
		const bool isMainLine = node.row == 0;
		const QColor fill = isCurrent ? highlight
			: isMainLine ? colors.color(QPalette::Base) : colors.color(QPalette::AlternateBase);

		painter.setOpacity(baseOpacity * (isCurrent ? 1.0 : NODE_OPACITY));
		painter.setPen(QPen(isCurrent ? highlight.lighter(130) : colors.color(QPalette::Mid), isCurrent ? 2 : 1));
		painter.setBrush(fill);
		painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);

		painter.setPen(isCurrent ? colors.color(QPalette::HighlightedText) : colors.color(QPalette::Text));
		const QString text = painter.fontMetrics().elidedText(getDisplayText(node), Qt::ElideRight, static_cast<int>(rect.width() - 2 * TEXT_PADDING));
		painter.drawText(rect, Qt::AlignCenter, text);
	}

	//stand-in for the hidden cursor
	painter.setOpacity(baseOpacity);
	painter.setPen(QPen(colors.color(QPalette::Base), 2));
	painter.setBrush(highlight);
	painter.drawEllipse(cursor, 6, 6);
}
