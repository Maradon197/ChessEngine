#include "SearchTreeView.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QTimer>
#include <QToolTip>
#include <QWheelEvent>

#include "GuiTypes.hpp"
#include "MoveNotation.hpp"
#include "SearchTreeModel.hpp"

namespace {

constexpr qreal COLUMN_WIDTH = 150;
constexpr qreal ROW_HEIGHT = 30;
constexpr qreal BOX_WIDTH = 112;
constexpr qreal BOX_HEIGHT = 24;
constexpr qreal MIN_ZOOM = 0.02;
constexpr qreal MAX_ZOOM = 2.5;
constexpr qreal TEXT_MIN_HEIGHT = 12;	//box height on screen from which names and scores are written
constexpr qreal HEADER_HEIGHT = 24;
constexpr int MOVE_DURATION = 280;	//ms, items glide to their new place
constexpr qreal VIEW_EASING = 0.22;	//share of the way the view moves per frame
constexpr int GROUP_TIP_MOVES = 14;	//moves listed in a group's tooltip
const char* const EXPAND_KEY = "searchWindow/expandPlies";

const QColor CUTOFF_COLOR(245, 166, 35);
const QColor BEST_COLOR(80, 200, 140);
const QColor LINE_COLOR(66, 153, 255);
const QColor SEARCHING_COLOR(0, 210, 230);

//black <-> white like the board's sides, middle grey is equal; tanh keeps big leads apart without exploding
int getShade(float whiteScore) {
	return static_cast<int>(std::lround(128 + 112 * std::tanh(whiteScore / 1.8f)));
}

QColor getFill(const SearchTreeModel& model, int node) {
	if (!model.getNode(node).finished) {
		return QColor(110, 110, 118);
	}
	const int shade = getShade(model.getWhiteScore(node));
	return QColor(shade, shade, shade);
}

QColor withAlpha(QColor color, int alpha) {
	color.setAlpha(alpha);
	return color;
}

QString formatCount(std::uint64_t count) {
	if (count >= 1'000'000) return QString::number(count / 1e6, 'f', 1) + "M";
	if (count >= 10'000) return QString::number(count / 1e3, 'f', 0) + "k";
	return QString::number(count);
}

qreal easeOut(qreal t) {
	return 1 - std::pow(1 - t, 3);
}

} // !namespace

SearchTreeView::SearchTreeView(QWidget* parent)
	: QWidget(parent) {
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
	setMinimumSize(300, 200);
	frameTimer = new QTimer(this);
	frameTimer->setInterval(16);
	connect(frameTimer, &QTimer::timeout, this, &SearchTreeView::onFrame);
	pulseClock.start();
	expandPly = std::clamp(QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).value(EXPAND_KEY, 2).toInt(), 1, 2);
}

SearchTreeView::~SearchTreeView() = default;

void SearchTreeView::setModel(std::shared_ptr<const SearchTreeModel> newModel) {
	const bool sameSearch = model && newModel && model->getSnapshot().searchId == newModel->getSnapshot().searchId;
	const bool justFinished = sameSearch && !model->getSnapshot().finished && newModel->getSnapshot().finished;
	model = std::move(newModel);
	if (!sameSearch) {
		openedNodes.clear();
		items.clear();
		following = true;
		emit followingChanged(true);
	}
	rebuildDisplay(sameSearch);
	//the frame is set when the search starts and again when it is done, not while it runs
	if (!sameSearch || (justFinished && following)) {
		QPointF center;
		QPointF fitZoom;
		computeFit(center, fitZoom);
		targetCenter = center;
		targetZoom = fitZoom;
		if (!sameSearch) {	//a new search: jump there
			viewCenter = center;
			zoom = fitZoom;
		}
	}
	startFrames();
	update();
}

void SearchTreeView::setExpandPly(int ply) {
	ply = std::clamp(ply, 1, 2);
	if (ply == expandPly) {
		return;
	}
	expandPly = ply;
	QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(EXPAND_KEY, ply);
	rebuildDisplay(true);
	if (following) {
		computeFit(targetCenter, targetZoom);
	}
	startFrames();
	update();
}

int SearchTreeView::getContinuation(int node) const {
	const int best = model->getBestChild(node);
	if (best >= 0) {
		return best;
	}
	const int last = model->getLastChild(node);	//nothing better yet: the move being searched shows where the engine is
	return last >= 0 && !model->getNode(last).finished ? last : -1;
}

std::vector<int> SearchTreeView::getOtherChildren(int node) const {
	std::vector<int> list = model->getChildren(node);
	const int continuation = getContinuation(node);
	std::erase(list, continuation);
	if (model->getSnapshot().finished) {	//done: the better alternatives first; before, search order keeps rows still
		std::stable_sort(list.begin(), list.end(), [&](int a, int b) {
			return model->getNode(a).score > model->getNode(b).score;	//the mover's view: higher is better for it
		});
	}
	return list;
}

int SearchTreeView::addItem(Item item) {
	const int index = static_cast<int>(items.size());
	if (item.parent >= 0) {
		items[static_cast<std::size_t>(item.parent)].children.push_back(index);
	}
	if (!item.group && !item.placeholder) {
		itemOfNode[item.node] = index;
	}
	items.push_back(std::move(item));
	return index;
}

//one row: the node, its continuation, the continuation's continuation...; nodes on it that were opened get rows
//below for their other moves (and those rows' opened nodes rows below them, and so on)
void SearchTreeView::addLine(int node, int parentItem, int row, int& nextRow) {
	std::vector<int> line;	//item indices
	for (int current = node, parent = parentItem; current >= 0; current = getContinuation(current)) {
		Item item;
		item.node = current;
		item.parent = parent;
		item.ply = model->getNode(current).ply;
		item.row = row;
		parent = addItem(std::move(item));
		line.push_back(parent);
	}
	for (int index : line) {
		const int lineNode = items[static_cast<std::size_t>(index)].node;
		if (!openedNodes.contains(lineNode)) {
			continue;
		}
		const std::vector<int> others = getOtherChildren(lineNode);
		for (int child : others) {
			addLine(child, index, nextRow++, nextRow);
		}
		if (!others.empty()) {
			Item fold;
			fold.node = lineNode;
			fold.group = true;
			fold.fold = true;
			fold.parent = index;
			fold.ply = model->getNode(lineNode).ply + 1;
			fold.row = nextRow++;
			fold.members = others;
			addItem(std::move(fold));
		}
	}
}

//bands of rows per root move, see the class comment; rows don't depend on anything the search changes later
void SearchTreeView::rebuildDisplay(bool animate) {
	std::unordered_map<std::int64_t, QPointF> previous;	//where things are drawn now
	for (const Item& item : items) {
		previous.emplace(item.getKey(), item.draw);
	}
	items.clear();
	itemOfNode.clear();
	bounds = {};
	lineCount = 0;
	rowCount = 0;
	if (!model || model->getNodeCount() == 0) {
		emit displayChanged();
		return;
	}
	const bool finished = model->getSnapshot().finished;

	addItem(Item{});	//the root, its row is set at the end
	const std::vector<SearchTreeModel::RootMove>& rootMoves = model->getRootMoves();
	const int rootCount = static_cast<int>(rootMoves.size());
	const int spareLines = expandPly >= 2 ? std::max(0, MAX_LINES - rootCount) : 0;
	int nextRow = 0;
	for (int index = 0; index < rootCount; ++index) {
		//this band's share of the spare lines, spread evenly (the remainder isn't all at the start)
		const int extra = rootCount > 0 ? (index + 1) * spareLines / rootCount - index * spareLines / rootCount : 0;
		const SearchTreeModel::RootMove& rootMove = rootMoves[static_cast<std::size_t>(index)];
		if (rootMove.node < 0) {
			Item placeholder;
			placeholder.node = -1;
			placeholder.placeholder = true;
			placeholder.move = rootMove.move;
			placeholder.rootIndex = index;
			placeholder.parent = 0;
			placeholder.ply = 1;
			placeholder.row = nextRow++;
			addItem(std::move(placeholder));
			nextRow += extra;	//kept free for its replies
			continue;
		}

		const int row = nextRow++;
		addLine(rootMove.node, 0, row, nextRow);
		if (extra == 0 || openedNodes.contains(rootMove.node)) {
			continue;
		}
		//its other replies: a few on their own rows, the rest in a group; the rows are kept even while unused
		const int bandEnd = nextRow + extra;
		const std::vector<int> others = getOtherChildren(rootMove.node);
		const auto alone = static_cast<std::size_t>(others.size() <= static_cast<std::size_t>(extra) ? extra : extra - 1);
		const int ownItem = itemOfNode.at(rootMove.node);
		for (std::size_t position = 0; position < others.size() && position < alone; ++position) {
			addLine(others[position], ownItem, nextRow++, nextRow);
		}
		if (others.size() > alone) {
			Item group;
			group.node = rootMove.node;
			group.group = true;
			group.parent = ownItem;
			group.ply = 2;
			group.row = nextRow++;
			group.members.assign(others.begin() + static_cast<std::ptrdiff_t>(alone), others.end());
			addItem(std::move(group));
		}
		if (!finished) {
			nextRow = std::max(nextRow, bandEnd);	//while searching the band keeps its size, done: empty rows go
		}
	}
	rowCount = nextRow;
	lineCount = rowCount;
	items[0].row = std::max(0, (rowCount - 1) / 2);	//the root in the middle of its moves

	//"+N": moves searched below an item that are neither shown nor in a group
	std::vector<int> accounted(static_cast<std::size_t>(model->getNodeCount()), 0);
	for (const Item& item : items) {
		if (item.parent < 0 || item.placeholder) {
			continue;
		}
		const int parentNode = items[static_cast<std::size_t>(item.parent)].node;
		if (item.group) {
			if (!item.fold) accounted[static_cast<std::size_t>(parentNode)] += static_cast<int>(item.members.size());
		} else {
			++accounted[static_cast<std::size_t>(parentNode)];
		}
	}
	for (Item& item : items) {
		if (item.group || item.placeholder) {
			continue;
		}
		const int shown = accounted[static_cast<std::size_t>(item.node)];
		item.hidden = item.node == 0 ? 0 : static_cast<int>(model->getNode(item.node).searchedChildren) - shown;
		item.hiddenRecorded = item.node == 0 ? 0 : model->getChildCount(item.node) - shown;
	}

	int maxPly = model->getSnapshot().depth;
	for (const Item& item : items) {
		maxPly = std::max(maxPly, item.ply);
	}
	bounds.minRow = 0;
	bounds.maxRow = std::max(0, rowCount - 1);
	bounds.maxColumn = maxPly;

	//things keep their place; only what changed place (folding, the end of the search) glides there
	for (Item& item : items) {
		item.target = {item.ply * COLUMN_WIDTH, item.row * ROW_HEIGHT};
		const auto found = previous.find(item.getKey());
		item.from = animate && found != previous.end() ? found->second : item.target;	//new ones appear in place
		item.draw = item.from;
	}
	moveProgress = animate ? 0 : 1;
	moveClock.start();
	hoveredItem = -1;
	emit displayChanged();
}

void SearchTreeView::computeFit(QPointF& center, QPointF& fitZoom) const {
	const QRectF area(-BOX_WIDTH / 2, bounds.minRow * ROW_HEIGHT - BOX_HEIGHT / 2,
		bounds.maxColumn * COLUMN_WIDTH + BOX_WIDTH + 40, (bounds.maxRow - bounds.minRow) * ROW_HEIGHT + BOX_HEIGHT);
	const qreal width = std::max(50.0, static_cast<qreal>(this->width()) - 60);
	const qreal height = std::max(50.0, static_cast<qreal>(this->height()) - HEADER_HEIGHT - 40);
	const qreal zoomX = std::clamp(width / area.width(), MIN_ZOOM, 1.2);
	const qreal zoomY = std::clamp(height / area.height(), MIN_ZOOM, zoomX);	//rows never taller than the boxes are wide
	fitZoom = {zoomX, zoomY};
	center = area.center() - QPointF(0, HEADER_HEIGHT / 2 / zoomY);
}

void SearchTreeView::fitToView() {
	setFollowing(true);
	computeFit(targetCenter, targetZoom);
	startFrames();
}

void SearchTreeView::centerOn(int node) {
	if (!model || node < 0 || node >= model->getNodeCount()) {
		return;
	}
	if (!itemOfNode.contains(node)) {	//open the groups on the way to it
		for (int current = model->getNode(node).parent; current >= 0; current = current > 0 ? model->getNode(current).parent : -1) {
			openedNodes.insert(current);
		}
		rebuildDisplay(true);
	}
	const auto found = itemOfNode.find(node);
	if (found == itemOfNode.end()) {
		return;
	}
	setFollowing(false);
	targetCenter = items[static_cast<std::size_t>(found->second)].target;
	targetZoom = {std::max(targetZoom.x(), 0.9), std::max(targetZoom.y(), 0.9)};
	startFrames();
}

void SearchTreeView::setFollowing(bool follow) {
	if (following != follow) {
		following = follow;
		emit followingChanged(follow);
	}
}

void SearchTreeView::startFrames() {
	if (!frameTimer->isActive()) {
		frameTimer->start();
	}
}

//moves the items and the view a step; keeps running while the engine searches, the searching nodes pulse
void SearchTreeView::onFrame() {
	bool busy = false;
	if (moveProgress < 1) {
		moveProgress = std::min<qreal>(1, moveClock.elapsed() / static_cast<qreal>(MOVE_DURATION));
		const qreal t = easeOut(moveProgress);
		for (Item& item : items) {
			item.draw = item.from + (item.target - item.from) * t;
		}
		busy = true;
	}
	const QPointF centerOffset = targetCenter - viewCenter;
	const qreal ratioX = targetZoom.x() / zoom.x();
	const qreal ratioY = targetZoom.y() / zoom.y();
	if (std::abs(centerOffset.x()) * zoom.x() > 0.3 || std::abs(centerOffset.y()) * zoom.y() > 0.3
		|| std::abs(ratioX - 1) > 0.002 || std::abs(ratioY - 1) > 0.002) {
		viewCenter += centerOffset * VIEW_EASING;
		zoom = {zoom.x() * std::pow(ratioX, VIEW_EASING), zoom.y() * std::pow(ratioY, VIEW_EASING)};
		busy = true;
	} else {
		viewCenter = targetCenter;
		zoom = targetZoom;
	}
	const bool searching = model && !model->getSnapshot().finished;
	if (!busy && !searching) {
		frameTimer->stop();
	}
	update();
}

QPointF SearchTreeView::toScreen(QPointF treePoint) const {
	const QPointF offset = treePoint - viewCenter;
	return QPointF(offset.x() * zoom.x(), offset.y() * zoom.y()) + QRectF(rect()).center();
}

QPointF SearchTreeView::toTree(QPointF screenPoint) const {
	const QPointF offset = screenPoint - QRectF(rect()).center();
	return QPointF(offset.x() / zoom.x(), offset.y() / zoom.y()) + viewCenter;
}

int SearchTreeView::findItemAt(QPointF screenPoint) const {
	const QPointF tree = toTree(screenPoint);
	const qreal reachX = std::max(BOX_WIDTH / 2, 6 / zoom.x());
	const qreal reachY = std::max(BOX_HEIGHT / 2, 6 / zoom.y());
	int found = -1;
	qreal closest = std::numeric_limits<qreal>::max();
	for (std::size_t index = 0; index < items.size(); ++index) {
		const QPointF offset = tree - items[index].draw;
		if (std::abs(offset.x()) > reachX || std::abs(offset.y()) > reachY) {
			continue;
		}
		const qreal distance = std::pow(offset.x() * zoom.x(), 2) + 4 * std::pow(offset.y() * zoom.y(), 2);
		if (distance < closest) {
			closest = distance;
			found = static_cast<int>(index);
		}
	}
	return found;
}

//the same place paintItems() puts the badges
QRectF SearchTreeView::getBadgeRect(const Item& item, const QString& label, bool above) const {
	const qreal boxWidth = BOX_WIDTH * zoom.x();
	const qreal boxHeight = BOX_HEIGHT * zoom.y();
	QFont badgeFont = font();
	const int nameSize = static_cast<int>(std::clamp(std::min(boxHeight * 0.55, boxWidth * 0.12), 8.0, 18.0));
	badgeFont.setPixelSize(std::max(8, static_cast<int>(nameSize * 0.75)));
	const QFontMetricsF metrics(badgeFont);
	const QPointF center = toScreen(item.draw);
	const qreal badgeHeight = std::min(metrics.height() + 1, boxHeight * 0.48);
	const qreal width = metrics.horizontalAdvance(label) + 6;
	const qreal y = above ? center.y() - badgeHeight - 1 : center.y() + 1;
	return {center.x() + boxWidth / 2 + 3, y, width, badgeHeight};
}

int SearchTreeView::findBadgeAt(QPointF screenPoint) const {
	if (BOX_HEIGHT * zoom.y() < TEXT_MIN_HEIGHT || BOX_WIDTH * zoom.x() < 64) {
		return -1;	//no badges drawn
	}
	for (std::size_t index = 0; index < items.size(); ++index) {
		const Item& item = items[index];
		if (!item.group && item.hiddenRecorded > 0
			&& getBadgeRect(item, QString("+%1").arg(item.hidden), true).adjusted(-3, -3, 3, 3).contains(screenPoint)) {
			return static_cast<int>(index);
		}
	}
	return -1;
}

void SearchTreeView::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	if (following) {
		computeFit(targetCenter, targetZoom);
		viewCenter = targetCenter;
		zoom = targetZoom;
	}
}

void SearchTreeView::wheelEvent(QWheelEvent* event) {
	const QPointF anchor = event->position();
	const QPointF treeAnchor = toTree(anchor);
	const qreal factor = std::pow(1.0015, event->angleDelta().y() != 0 ? event->angleDelta().y() : event->angleDelta().x());
	const bool rowsOnly = event->modifiers() & Qt::ShiftModifier;
	zoom = {rowsOnly ? zoom.x() : std::clamp(zoom.x() * factor, MIN_ZOOM, MAX_ZOOM), std::clamp(zoom.y() * factor, MIN_ZOOM, MAX_ZOOM)};
	const QPointF offset = anchor - QRectF(rect()).center();
	viewCenter = treeAnchor - QPointF(offset.x() / zoom.x(), offset.y() / zoom.y());	//the point under the cursor stays put
	targetCenter = viewCenter;
	targetZoom = zoom;
	setFollowing(false);
	update();
}

void SearchTreeView::mousePressEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		dragging = true;
		dragMoved = false;
		dragStart = dragLast = event->position();
	}
}

void SearchTreeView::mouseMoveEvent(QMouseEvent* event) {
	if (dragging) {
		const QPointF delta = event->position() - dragLast;
		dragLast = event->position();
		if (!dragMoved && (event->position() - dragStart).manhattanLength() > 4) {
			dragMoved = true;
			setFollowing(false);
			setCursor(Qt::ClosedHandCursor);
		}
		if (dragMoved) {
			viewCenter -= QPointF(delta.x() / zoom.x(), delta.y() / zoom.y());
			targetCenter = viewCenter;
			targetZoom = zoom;
			QToolTip::hideText();
			update();
		}
		return;
	}
	const int badge = findBadgeAt(event->position());
	const int item = badge >= 0 ? badge : findItemAt(event->position());
	if (badge >= 0) {
		setCursor(Qt::PointingHandCursor);
		QToolTip::showText(event->globalPosition().toPoint() + QPoint(14, 14),
			tr("%1 more moves were searched here. Click to show the recorded ones.")
				.arg(items[static_cast<std::size_t>(badge)].hidden), this);
		hoveredItem = badge;
		update();
		return;
	}
	if (item != hoveredItem) {
		hoveredItem = item;
		setCursor(item > 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
		update();
		if (item < 0) {
			QToolTip::hideText();
		}
	}
	if (item >= 0) {
		showItemTip(item, event->globalPosition().toPoint());
	}
}

//a click on a group opens or folds it, a click on a move plays the line up to it
void SearchTreeView::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() != Qt::LeftButton || !dragging) {
		return;
	}
	dragging = false;
	unsetCursor();
	if (dragMoved || !model) {
		return;
	}
	if (const int badge = findBadgeAt(event->position()); badge >= 0) {	//"+N": show the moves behind it
		openedNodes.insert(items[static_cast<std::size_t>(badge)].node);
		QToolTip::hideText();
		rebuildDisplay(true);
		if (following) {
			computeFit(targetCenter, targetZoom);
		}
		startFrames();
		return;
	}
	const int index = findItemAt(event->position());
	if (index < 0) {
		return;
	}
	const Item& item = items[static_cast<std::size_t>(index)];
	if (item.group) {
		if (item.fold) {
			openedNodes.erase(item.node);
		} else {
			openedNodes.insert(item.node);
		}
		QToolTip::hideText();
		rebuildDisplay(true);
		if (following) {
			computeFit(targetCenter, targetZoom);
		}
		startFrames();
	} else if (item.node > 0) {
		emit nodeActivated(item.node);
	}
}

void SearchTreeView::leaveEvent(QEvent* event) {
	QWidget::leaveEvent(event);
	hoveredItem = -1;
	update();
}

void SearchTreeView::keyPressEvent(QKeyEvent* event) {
	switch (event->key()) {
		case Qt::Key_Plus:
		case Qt::Key_Equal:
			setExpandPly(expandPly + 1);
			return;
		case Qt::Key_Minus:
			setExpandPly(expandPly - 1);
			return;
		case Qt::Key_F:
			fitToView();
			return;
		default:
			QWidget::keyPressEvent(event);
	}
}

void SearchTreeView::showItemTip(int index, QPoint globalPos) {
	const SearchTreeModel& tree = *model;
	const Item& item = items[static_cast<std::size_t>(index)];
	QString text;

	if (item.group) {
		//summary of the moves inside
		int cutoffs = 0;
		int best = 0;
		int failedLow = 0;
		std::uint64_t nodes = 0;
		for (int member : item.members) {
			const SearchNodeKind kind = tree.getKind(member);
			cutoffs += kind == SearchNodeKind::CUTOFF;
			best += kind == SearchNodeKind::BEST;
			failedLow += kind == SearchNodeKind::FAILED_LOW;
			nodes += tree.getNode(member).subtreeNodes;
		}
		const QString owner = item.node == 0 ? tr("the root") : tr("%1").arg(tree.getMoveName(item.node).toHtmlEscaped());
		text += item.fold ? tr("<b>Fold the %1 other moves after %2</b>").arg(item.members.size()).arg(owner)
			: tr("<b>%1 more moves after %2</b>").arg(item.members.size()).arg(owner);
		text += "<br><span style='color:gray'>" + tr("%1 nodes searched below them").arg(formatCount(nodes)) + "</span><br>";
		QStringList parts;
		if (cutoffs) parts << tr("<span style='color:%1'>%2 caused a cutoff</span>").arg(CUTOFF_COLOR.name()).arg(cutoffs);
		if (best) parts << tr("<span style='color:%1'>%2 raised alpha</span>").arg(BEST_COLOR.name()).arg(best);
		if (failedLow) parts << tr("%1 not better").arg(failedLow);
		text += parts.join(", ") + "<br>";
		const int shown = std::min(static_cast<int>(item.members.size()), GROUP_TIP_MOVES);
		for (int position = 0; position < shown; ++position) {
			const int member = item.members[static_cast<std::size_t>(position)];
			text += QString("&nbsp;&nbsp;%1 &nbsp;<span style='color:gray'>%2</span><br>")
				.arg(tree.getMoveName(member).toHtmlEscaped(), tree.getScoreText(member).toHtmlEscaped());
		}
		if (static_cast<int>(item.members.size()) > shown) {
			text += tr("&nbsp;&nbsp;… and %1 more<br>").arg(item.members.size() - static_cast<std::size_t>(shown));
		}
		text += "<i>" + (item.fold ? tr("Click to fold them into a group again.") : tr("Click to show them.")) + "</i>";
		QToolTip::showText(globalPos + QPoint(14, 14), text, this);
		return;
	}

	if (item.placeholder) {
		QToolTip::showText(globalPos + QPoint(14, 14), tr("<b>%1</b><br>Not searched yet, Rhea gets to it later.")
			.arg(QString::fromStdString(MoveNotation::buildMoveString(item.move, tree.getSnapshot().root.getPosition())).toHtmlEscaped()), this);
		return;
	}
	const int node = item.node;
	const SearchTreeNode& data = tree.getNode(node);
	if (node == 0) {
		text += tr("<b>The position Rhea searches</b><br>");
		text += data.finished ? tr("Result: <b>%1</b> (white's view)").arg(tree.getScoreText(node)) : tr("Searching…");
	} else {
		text += QString("<b>%1</b><br>").arg(tree.getLineText(node).toHtmlEscaped());
		text += tr("Score: <b>%1</b> (white's view)").arg(tree.getScoreText(node));
		const QString mover = tree.getMover(node) == WHITE ? tr("White") : tr("Black");
		text += "<br>";
		switch (tree.getKind(node)) {
			case SearchNodeKind::SEARCHING:
				text += tr("<span style='color:%1'>Being searched right now.</span>").arg(SEARCHING_COLOR.name());
				break;
			case SearchNodeKind::CUTOFF:
				text += tr("<span style='color:%1'><b>Beta cutoff:</b></span> this move is so good for %2 that the opponent "
					"would never allow it. The opponent's other moves before it didn't need to be looked at.").arg(CUTOFF_COLOR.name(), mover);
				break;
			case SearchNodeKind::BEST:
				text += tr("<span style='color:%1'><b>Best so far:</b></span> better for %2 than every move searched before it "
					"here (raised alpha).").arg(BEST_COLOR.name(), mover);
				break;
			case SearchNodeKind::FAILED_LOW:
				text += tr("<b>Not better</b> for %1 than a move searched before it here, so the score is only a bound.").arg(mover);
				break;
			default:
				break;
		}
	}
	if (tree.isCutShort(node)) {
		const int skipped = tree.getSkippedMoves(node);
		text += "<br>" + tr("<span style='color:%1'><b>Cut short:</b></span> %2 refutes it, so %3 other replies were skipped.")
			.arg(CUTOFF_COLOR.name(), tree.getMoveName(tree.getLastChild(node)).toHtmlEscaped()).arg(skipped);
	}
	if (tree.isLeaf(node)) {
		text += "<br>" + tr("At the search horizon: evaluated directly, not searched further.");
	} else if (tree.isTerminal(node)) {
		text += "<br>" + tr("No legal moves: the game ends here.");
	}
	if (node > 0) {
		const float alpha = -data.beta;	//the window from the mover's view, as the parent had it
		const float beta = -data.alpha;
		text += "<br><span style='color:gray'>" + tr("Window when searched: α %1, β %2 (mover's view)")
			.arg(SearchTreeModel::formatScore(alpha), SearchTreeModel::formatScore(beta)) + "</span>";
	}
	const double share = 100.0 * data.subtreeNodes / std::max<std::uint64_t>(1, tree.getSnapshot().totalNodes);
	text += "<br><span style='color:gray'>" + tr("%1 nodes searched below (%2% of the search)")
		.arg(formatCount(data.subtreeNodes)).arg(share, 0, 'f', share < 1 ? 2 : 1) + "</span>";
	if (item.hiddenRecorded > 0) {
		text += "<br><span style='color:gray'>" + tr("%1 more searched moves aren't shown, click the +%1 badge for them.").arg(item.hidden) + "</span>";
	} else if (item.hidden > 0) {
		text += "<br><span style='color:gray'>" + tr("%1 more searched moves weren't recorded.").arg(item.hidden) + "</span>";
	}
	if (node > 0) {
		text += "<br><i>" + tr("Click to play this line in the analysis.") + "</i>";
	}
	QToolTip::showText(globalPos + QPoint(14, 14), text, this);
}

void SearchTreeView::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	QPainter painter(this);
	painter.fillRect(rect(), palette().color(QPalette::Base));
	if (!model || items.empty()) {
		paintEmpty(painter);
		return;
	}
	painter.setRenderHint(QPainter::Antialiasing);
	paintLanes(painter);
	paintEdges(painter);
	paintItems(painter);
	paintLaneHeaders(painter);
}

void SearchTreeView::paintEmpty(QPainter& painter) const {
	painter.setPen(withAlpha(palette().color(QPalette::Text), 130));
	painter.drawText(rect(), Qt::AlignCenter, tr("No search yet.\nLet Rhea move in the play view, or open the analysis, and watch it think."));
}

//one lane per ply, the horizon lane is where positions get evaluated
void SearchTreeView::paintLanes(QPainter& painter) const {
	const SearchSnapshot& snapshot = model->getSnapshot();
	const int lanes = std::max(bounds.maxColumn, snapshot.depth);
	const QColor text = palette().color(QPalette::Text);
	for (int ply = 0; ply <= lanes; ++ply) {
		const qreal left = toScreen({(ply - 0.5) * COLUMN_WIDTH, 0}).x();
		const qreal right = toScreen({(ply + 0.5) * COLUMN_WIDTH, 0}).x();
		const QRectF lane(left, 0, right - left, height());
		if (ply % 2 == 1) {
			painter.fillRect(lane, withAlpha(text, 10));
		}
		if (!snapshot.finished && ply == snapshot.currentPly) {
			painter.fillRect(lane, withAlpha(SEARCHING_COLOR, 16));
		}
	}
	const qreal horizon = toScreen({(snapshot.depth + 0.5) * COLUMN_WIDTH, 0}).x();
	painter.setPen(QPen(withAlpha(CUTOFF_COLOR, 140), 1.5, Qt::DashLine));
	painter.drawLine(QPointF(horizon, HEADER_HEIGHT), QPointF(horizon, height()));
}

void SearchTreeView::paintLaneHeaders(QPainter& painter) const {
	const SearchSnapshot& snapshot = model->getSnapshot();
	const QColor text = palette().color(QPalette::Text);
	painter.fillRect(QRectF(0, 0, width(), HEADER_HEIGHT), withAlpha(palette().color(QPalette::Window), 235));
	QFont font = this->font();	//the painter's font has a pixel size by now
	font.setPointSizeF(font.pointSizeF() * 0.85);
	painter.setFont(font);
	const Color rootSide = snapshot.root.getPosition().side_to_move;
	const int lanes = std::max(bounds.maxColumn, snapshot.depth);
	for (int ply = 0; ply <= lanes; ++ply) {
		const qreal left = toScreen({(ply - 0.5) * COLUMN_WIDTH, 0}).x();
		const qreal right = toScreen({(ply + 0.5) * COLUMN_WIDTH, 0}).x();
		if (right < 0 || left > width() || right - left < 34) {
			continue;
		}
		QString label;
		if (ply == 0) {
			label = tr("Root");
		} else {
			const Color mover = ply % 2 == 1 ? rootSide : !rootSide;
			label = right - left < 90 ? QString::number(ply)
				: tr("Ply %1 · %2").arg(ply).arg(mover == WHITE ? tr("White") : tr("Black"));
			if (ply == snapshot.depth && right - left >= 150) {
				label += tr(" · horizon");
			}
		}
		const bool current = !snapshot.finished && ply == snapshot.currentPly;
		painter.setPen(current ? SEARCHING_COLOR : withAlpha(text, ply == snapshot.depth ? 200 : 140));
		painter.drawText(QRectF(left, 0, right - left, HEADER_HEIGHT), Qt::AlignCenter, label);
	}
	painter.setPen(withAlpha(text, 40));
	painter.drawLine(QPointF(0, HEADER_HEIGHT), QPointF(width(), HEADER_HEIGHT));
}

//elbows like the move tree; thickness shows how much was searched below, the best line glows
void SearchTreeView::paintEdges(QPainter& painter) const {
	const SearchTreeModel& tree = *model;
	const QColor text = palette().color(QPalette::Text);
	const qreal halfBox = BOX_WIDTH / 2 * zoom.x();
	const qreal logMax = std::log(static_cast<qreal>(std::max<std::uint32_t>(2, tree.getMaxSubtree())));
	const qreal zoomFactor = std::clamp(zoom.y() * 1.4, 0.35, 1.0);
	const qreal pulse = 0.5 + 0.5 * std::sin(pulseClock.elapsed() / 180.0);

	for (int pass = 0; pass < 2; ++pass) {	//the highlighted edges go on top
		for (std::size_t index = 1; index < items.size(); ++index) {
			const Item& item = items[index];
			const bool real = !item.group && !item.placeholder;
			const bool onLine = real && tree.isOnBestLine(item.node);
			const bool current = real && tree.isOnCurrentPath(item.node);
			if ((onLine || current) != (pass == 1)) {
				continue;
			}
			const QPointF from = toScreen(items[static_cast<std::size_t>(item.parent)].draw) + QPointF(halfBox, 0);
			const QPointF to = toScreen(item.draw) - QPointF(halfBox, 0);
			QPainterPath path(from);
			if (std::abs(from.y() - to.y()) < 0.5) {
				path.lineTo(to);
			} else {
				const qreal middle = (from.x() + to.x()) / 2;
				path.lineTo(middle, from.y());
				path.lineTo(middle, to.y());
				path.lineTo(to);
			}
			std::uint64_t effortNodes = 0;
			if (item.placeholder) {
				effortNodes = 1;
			} else if (item.group) {
				for (int member : item.members) effortNodes += tree.getNode(member).subtreeNodes;
			} else {
				effortNodes = tree.getNode(item.node).subtreeNodes;
			}
			const qreal effort = std::log(static_cast<qreal>(std::max<std::uint64_t>(1, effortNodes))) / logMax;
			const qreal width = (0.7 + 3.6 * std::min<qreal>(effort, 1.2)) * zoomFactor;
			if (current) {
				painter.setPen(QPen(withAlpha(SEARCHING_COLOR, static_cast<int>(120 + 100 * pulse)), width + 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			} else if (onLine) {
				painter.setPen(QPen(withAlpha(LINE_COLOR, 60), width + 8 * zoomFactor, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
				painter.drawPath(path);
				painter.setPen(QPen(LINE_COLOR, width + 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			} else {
				const bool hovered = static_cast<int>(index) == hoveredItem;
				painter.setPen(QPen(withAlpha(text, item.placeholder ? 30 : hovered ? 170 : 70), width,
					item.group || item.placeholder ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			}
			painter.drawPath(path);
		}
	}
}

//a stack of cards: how many moves, how they were scored (a strip of their colors), how many caused cutoffs
void SearchTreeView::paintGroup(QPainter& painter, const Item& item, const QRectF& box, bool detailed) const {
	const SearchTreeModel& tree = *model;
	const QColor text = palette().color(QPalette::Text);
	const QColor base = palette().color(QPalette::Base);
	const qreal step = std::clamp(box.height() * 0.14, 1.5, 4.0);
	const qreal corner = std::min(6.0, box.height() * 0.3);
	if (!item.fold) {	//the cards behind
		for (int layer = 2; layer >= 1; --layer) {
			painter.setPen(QPen(withAlpha(text, 60), 1));
			painter.setBrush(base);
			painter.drawRoundedRect(box.translated(layer * step, -layer * step), corner, corner);
		}
	}
	const bool hovered = &item == &items[static_cast<std::size_t>(std::max(0, hoveredItem))] && hoveredItem >= 0;
	const bool searching = std::any_of(item.members.begin(), item.members.end(), [&](int member) { return tree.isOnCurrentPath(member); });
	if (searching) {	//the engine is in there right now
		const qreal pulse = 0.5 + 0.5 * std::sin(pulseClock.elapsed() / 180.0);
		painter.setPen(QPen(withAlpha(SEARCHING_COLOR, static_cast<int>(130 + 125 * pulse)), 2.2));
	} else {
		painter.setPen(QPen(withAlpha(text, hovered ? 220 : 140), hovered ? 1.8 : 1.2, Qt::DashLine));
	}
	painter.setBrush(base);
	painter.drawRoundedRect(box, corner, corner);
	if (!detailed) {
		return;
	}

	int cutoffs = 0;
	for (int member : item.members) {
		cutoffs += tree.getKind(member) == SearchNodeKind::CUTOFF;
	}
	QFont font = painter.font();
	font.setPixelSize(static_cast<int>(std::clamp(box.height() * 0.48, 8.0, 15.0)));
	font.setBold(true);
	painter.setFont(font);
	const QRectF inner = box.adjusted(7, 0, -6, 0);
	const QString label = item.fold ? QString::fromUtf8("▴ fold %1").arg(item.members.size())
		: QString::fromUtf8("▸ %1 more").arg(item.members.size());
	painter.setPen(text);
	painter.drawText(inner, Qt::AlignVCenter | Qt::AlignLeft, label);
	const qreal labelWidth = QFontMetricsF(font).horizontalAdvance(label) + 6;

	//their scores as a strip of shades, best first
	const QRectF strip(inner.left() + labelWidth, box.center().y() - box.height() * 0.22,
		std::max(0.0, inner.width() - labelWidth - (cutoffs > 0 ? 18 : 0)), box.height() * 0.44);
	if (strip.width() > 8) {
		const int count = static_cast<int>(item.members.size());
		const qreal width = strip.width() / count;
		for (int index = 0; index < count; ++index) {
			const int member = item.members[static_cast<std::size_t>(index)];
			const QRectF bar(strip.left() + index * width, strip.top(), std::max(1.0, width - (width > 3 ? 1 : 0)), strip.height());
			painter.fillRect(bar, getFill(tree, member));
		}
		painter.setPen(QPen(withAlpha(text, 70), 1));
		painter.setBrush(Qt::NoBrush);
		painter.drawRect(strip);
	}
	if (cutoffs > 0) {
		font.setBold(false);
		font.setPixelSize(std::max(8, font.pixelSize() - 2));
		painter.setFont(font);
		painter.setPen(CUTOFF_COLOR);
		painter.drawText(inner, Qt::AlignVCenter | Qt::AlignRight, QString::number(cutoffs));
	}
}

void SearchTreeView::paintItems(QPainter& painter) const {
	const SearchTreeModel& tree = *model;
	const QColor text = palette().color(QPalette::Text);
	const qreal boxWidth = BOX_WIDTH * zoom.x();
	const qreal boxHeight = BOX_HEIGHT * zoom.y();
	const QRectF visible = QRectF(rect()).adjusted(-boxWidth, -boxHeight, boxWidth, boxHeight);
	const bool detailed = boxHeight >= TEXT_MIN_HEIGHT && boxWidth >= 64;
	const bool boxes = boxHeight >= 5;
	const qreal textZoom = std::min(zoom.x(), zoom.y());
	const qreal pulse = 0.5 + 0.5 * std::sin(pulseClock.elapsed() / 180.0);

	QFont nameFont = painter.font();
	nameFont.setPixelSize(static_cast<int>(std::clamp(std::min(boxHeight * 0.55, boxWidth * 0.12), 8.0, 18.0)));
	nameFont.setBold(true);
	QFont scoreFont = nameFont;
	scoreFont.setBold(false);
	scoreFont.setPixelSize(std::max(8, static_cast<int>(nameFont.pixelSize() * 0.88)));
	QFont badgeFont = scoreFont;
	badgeFont.setPixelSize(std::max(8, static_cast<int>(nameFont.pixelSize() * 0.75)));

	for (std::size_t index = 0; index < items.size(); ++index) {
		const Item& item = items[index];
		const QPointF center = toScreen(item.draw);
		if (!visible.contains(center)) {
			continue;
		}
		const QRectF box(center.x() - boxWidth / 2, center.y() - boxHeight / 2, boxWidth, boxHeight);
		const bool hovered = static_cast<int>(index) == hoveredItem;
		if (item.group) {
			painter.setOpacity(1);
			paintGroup(painter, item, boxes ? box : QRectF(center.x() - 4, center.y() - 2, 8, 4), detailed);
			continue;
		}
		if (item.placeholder) {	//a root move still to come: an empty frame with its name
			painter.setOpacity(1);
			painter.setPen(QPen(withAlpha(text, hovered ? 140 : 70), 1, Qt::DashLine));
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(boxes ? box : QRectF(center.x() - 3, center.y() - 2, 6, 4), 6 * textZoom, 6 * textZoom);
			if (detailed) {
				painter.setFont(scoreFont);
				painter.setPen(withAlpha(text, 110));
				painter.drawText(box.adjusted(8 * textZoom, 0, -8 * textZoom, 0), Qt::AlignVCenter | Qt::AlignLeft,
					QString::fromStdString(MoveNotation::buildMoveString(item.move, tree.getSnapshot().root.getPosition())));
			}
			continue;
		}

		const int node = item.node;
		const SearchNodeKind kind = tree.getKind(node);
		const bool onLine = tree.isOnBestLine(node);
		const bool cutShort = tree.isCutShort(node);
		const QColor fill = getFill(tree, node);

		//outline: what the move turned out to be
		QPen outline(withAlpha(text, 90), 1);
		qreal opacity = 1;
		switch (kind) {
			case SearchNodeKind::ROOT: outline = QPen(LINE_COLOR, 2.4); break;
			case SearchNodeKind::SEARCHING: outline = QPen(withAlpha(SEARCHING_COLOR, static_cast<int>(130 + 125 * pulse)), 2.4); break;
			case SearchNodeKind::CUTOFF: outline = QPen(CUTOFF_COLOR, 2.4); break;
			case SearchNodeKind::BEST: outline = QPen(BEST_COLOR, 1.8); break;
			case SearchNodeKind::FAILED_LOW: opacity = 0.72; break;
		}
		if (cutShort && kind != SearchNodeKind::CUTOFF) {
			outline = QPen(CUTOFF_COLOR, 1.8, Qt::DashLine);
		}
		if (onLine && kind != SearchNodeKind::SEARCHING) {
			outline = QPen(LINE_COLOR, 2.6);
		}
		if (hovered) {
			opacity = 1;
			outline.setWidthF(outline.widthF() + 1);
		}
		painter.setOpacity(opacity);

		if (!boxes) {
			const qreal radius = std::clamp(boxHeight * 0.5, 1.8, 3.5) + (onLine ? 1.2 : 0);
			outline.setWidthF(std::min(outline.widthF(), 1.4));
			painter.setPen(kind == SearchNodeKind::FAILED_LOW && !onLine ? Qt::NoPen : outline);
			painter.setBrush(fill);
			painter.drawEllipse(center, radius, radius);
			continue;
		}

		if (onLine) {	//the best line glows
			painter.setPen(Qt::NoPen);
			painter.setBrush(withAlpha(LINE_COLOR, 55));
			painter.drawRoundedRect(box.adjusted(-4 * textZoom, -4 * textZoom, 4 * textZoom, 4 * textZoom), 8 * textZoom, 8 * textZoom);
		}
		painter.setPen(outline);
		painter.setBrush(fill);
		const qreal corner = tree.isLeaf(node) ? 2 * textZoom : 6 * textZoom;	//leaves (evaluated at the horizon) are squarer
		painter.drawRoundedRect(box, corner, corner);

		if (!detailed) {
			continue;
		}
		const QColor ink = fill.lightness() > 140 ? QColor(25, 25, 28) : QColor(238, 238, 240);
		const QRectF inner = box.adjusted(8 * textZoom, 0, -8 * textZoom, 0);
		painter.setPen(ink);
		painter.setFont(nameFont);
		painter.drawText(inner, Qt::AlignVCenter | Qt::AlignLeft, tree.getMoveName(node));
		painter.setFont(scoreFont);
		painter.setPen(withAlpha(ink, 200));
		painter.drawText(inner, Qt::AlignVCenter | Qt::AlignRight, tree.getScoreText(node));

		//stacked just right of the box: unrecorded moves above the middle, skipped ones below, the edge runs between them
		painter.setOpacity(1);
		painter.setFont(badgeFont);
		const QFontMetricsF metrics(badgeFont);
		const qreal badgeLeft = box.right() + 3;
		const qreal badgeHeight = std::min(metrics.height() + 1, boxHeight * 0.48);
		const auto drawBadge = [&](const QString& label, const QColor& color, bool dashed, bool above) {
			const qreal width = metrics.horizontalAdvance(label) + 6;
			const qreal y = above ? box.center().y() - badgeHeight - 1 : box.center().y() + 1;
			const QRectF badge(badgeLeft, y, width, badgeHeight);
			painter.setPen(QPen(color, 1, dashed ? Qt::DashLine : Qt::SolidLine));
			painter.setBrush(palette().color(QPalette::Base));
			painter.drawRoundedRect(badge, badgeHeight / 2, badgeHeight / 2);
			painter.setPen(color);
			painter.drawText(badge, Qt::AlignCenter, label);
		};
		if (item.hidden > 0) {
			drawBadge(QString("+%1").arg(item.hidden), withAlpha(text, 170), false, true);
		}
		if (cutShort) {
			drawBadge(QString::fromUtf8("✂%1").arg(tree.getSkippedMoves(node)), CUTOFF_COLOR, true, false);
		}
	}
	painter.setOpacity(1);
}
