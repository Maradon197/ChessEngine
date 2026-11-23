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
#include "SearchTreeModel.hpp"

namespace {

constexpr qreal COLUMN_WIDTH = 150;
constexpr qreal ROW_HEIGHT = 36;
constexpr qreal BOX_WIDTH = 112;
constexpr qreal BOX_HEIGHT = 26;
constexpr qreal MIN_ZOOM = 0.02;
constexpr qreal MAX_ZOOM = 2.5;
constexpr qreal TEXT_MIN_HEIGHT = 12;	//box height on screen from which names and scores are written
constexpr qreal HEADER_HEIGHT = 24;
constexpr int MOVE_DURATION = 280;	//ms, nodes glide to their new place
constexpr qreal VIEW_EASING = 0.22;	//share of the way the view moves per frame

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
	expandPly = std::clamp(QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).value(EXPAND_KEY, 2).toInt(), 1, 32);
	shownPly = expandPly;
}

SearchTreeView::~SearchTreeView() = default;

void SearchTreeView::setModel(std::shared_ptr<const SearchTreeModel> newModel) {
	const bool sameSearch = model && newModel && model->getSnapshot().searchId == newModel->getSnapshot().searchId;
	model = std::move(newModel);
	if (!sameSearch) {
		expandedNodes.clear();
		drawPositions.clear();
	}
	rebuildDisplay(sameSearch);
	if (!sameSearch || following) {
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
	ply = std::clamp(ply, 1, 32);
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

void SearchTreeView::rebuildDisplay(bool animate) {
	shownNodes.clear();
	const int count = model ? model->getNodeCount() : 0;
	shownChildren.assign(static_cast<std::size_t>(count), {});
	rows.assign(static_cast<std::size_t>(count), 0);
	isShown.assign(static_cast<std::size_t>(count), false);
	bounds = {};
	if (count == 0) {
		emit displayChanged();
		return;
	}

	//everything up to shownPly, below that only the best continuation, what was clicked open and where the engine is;
	//too many nodes for a readable picture: one ply less
	for (shownPly = expandPly;; --shownPly) {
		shownNodes.clear();
		std::fill(isShown.begin(), isShown.end(), false);
		shownNodes.push_back(0);
		isShown[0] = true;
		for (std::size_t next = 0; next < shownNodes.size(); ++next) {
			const int node = shownNodes[next];
			const int best = model->getBestChild(node);
			const bool open = expandedNodes.contains(node);
			for (int child : model->getChildren(node)) {
				if (model->getNode(child).ply <= shownPly || child == best || open || model->isOnCurrentPath(child)) {
					shownNodes.push_back(child);
					isShown[static_cast<std::size_t>(child)] = true;
				}
			}
		}
		if (static_cast<int>(shownNodes.size()) <= MAX_SHOWN_NODES || shownPly <= 1) {
			break;
		}
	}

	//display order: the preferred move continues the row, then the better alternatives closer to it, unfinished ones last
	for (int node : shownNodes) {
		if (node == 0) {
			continue;
		}
		shownChildren[static_cast<std::size_t>(model->getNode(node).parent)].push_back(node);
	}
	for (int node : shownNodes) {
		std::vector<int>& list = shownChildren[static_cast<std::size_t>(node)];
		const int best = model->getBestChild(node);
		std::stable_sort(list.begin(), list.end(), [&](int a, int b) {
			if ((a == best) != (b == best)) return a == best;
			const bool aDone = model->getNode(a).finished;
			const bool bDone = model->getNode(b).finished;
			if (aDone != bDone) return aDone;
			return aDone && model->getNode(a).score > model->getNode(b).score;	//the mover's view: higher is better for it
		});
	}

	bounds = treeLayout::layoutRows(0,
		[this](int node) -> const std::vector<int>& { return shownChildren[static_cast<std::size_t>(node)]; },
		[this](int node) { return static_cast<int>(model->getNode(node).ply); },
		[this](int node, int row) { rows[static_cast<std::size_t>(node)] = row; });

	//glide from where the nodes are drawn now, new ones grow out of their parent
	const std::vector<QPointF> previous = drawPositions;
	targetPositions.assign(static_cast<std::size_t>(count), {});
	fromPositions.assign(static_cast<std::size_t>(count), {});
	drawPositions.assign(static_cast<std::size_t>(count), {});
	for (int node : shownNodes) {
		const auto index = static_cast<std::size_t>(node);
		targetPositions[index] = {model->getNode(node).ply * COLUMN_WIDTH, rows[index] * ROW_HEIGHT};
		if (animate && index < previous.size() && !previous[index].isNull()) {
			fromPositions[index] = previous[index];
		} else if (animate && node != 0) {
			fromPositions[index] = fromPositions[static_cast<std::size_t>(model->getNode(node).parent)];
		} else {
			fromPositions[index] = targetPositions[index];
		}
		if (node == 0) {
			fromPositions[index] = animate && !previous.empty() ? previous[0] : targetPositions[index];
		}
		drawPositions[index] = fromPositions[index];
	}
	moveProgress = animate ? 0 : 1;
	if (!animate) {
		drawPositions = targetPositions;
	}
	moveClock.start();
	if (hoveredNode >= count || (hoveredNode >= 0 && !isShown[static_cast<std::size_t>(hoveredNode)])) {
		hoveredNode = -1;
	}
	emit displayChanged();
}

void SearchTreeView::computeFit(QPointF& center, QPointF& fitZoom) const {
	const QRectF area(-BOX_WIDTH / 2, bounds.minRow * ROW_HEIGHT - BOX_HEIGHT / 2,
		bounds.maxColumn * COLUMN_WIDTH + BOX_WIDTH, (bounds.maxRow - bounds.minRow) * ROW_HEIGHT + BOX_HEIGHT);
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
	if (!isShown[static_cast<std::size_t>(node)]) {	//open the way to it
		for (int current = model->getNode(node).parent; current > 0; current = model->getNode(current).parent) {
			expandedNodes.insert(current);
		}
		expandedNodes.insert(0);
		rebuildDisplay(true);
	}
	setFollowing(false);
	targetCenter = targetPositions[static_cast<std::size_t>(node)];
	targetZoom = {std::max(targetZoom.x(), 0.9), std::max(targetZoom.y(), 0.9)};
	hoveredNode = node;
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

//moves the nodes and the view a step; keeps running while the engine searches, the searching nodes pulse
void SearchTreeView::onFrame() {
	bool busy = false;
	if (moveProgress < 1) {
		moveProgress = std::min<qreal>(1, moveClock.elapsed() / static_cast<qreal>(MOVE_DURATION));
		const qreal t = easeOut(moveProgress);
		for (int node : shownNodes) {
			const auto index = static_cast<std::size_t>(node);
			drawPositions[index] = fromPositions[index] + (targetPositions[index] - fromPositions[index]) * t;
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

int SearchTreeView::findNodeAt(QPointF screenPoint) const {
	if (!model) {
		return -1;
	}
	const QPointF tree = toTree(screenPoint);
	const qreal reachX = std::max(BOX_WIDTH / 2, 6 / zoom.x());
	const qreal reachY = std::max(BOX_HEIGHT / 2, 6 / zoom.y());
	int found = -1;
	qreal closest = std::numeric_limits<qreal>::max();
	for (int node : shownNodes) {
		const QPointF offset = tree - drawPositions[static_cast<std::size_t>(node)];
		if (std::abs(offset.x()) > reachX || std::abs(offset.y()) > reachY) {
			continue;
		}
		const qreal distance = std::pow(offset.x() * zoom.x(), 2) + 4 * std::pow(offset.y() * zoom.y(), 2);
		if (distance < closest) {
			closest = distance;
			found = node;
		}
	}
	return found;
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
		setCursor(Qt::ClosedHandCursor);
	}
}

void SearchTreeView::mouseMoveEvent(QMouseEvent* event) {
	if (dragging) {
		const QPointF delta = event->position() - dragLast;
		dragLast = event->position();
		if ((event->position() - dragStart).manhattanLength() > 4) {
			dragMoved = true;
			setFollowing(false);
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
	const int node = findNodeAt(event->position());
	if (node != hoveredNode) {
		hoveredNode = node;
		update();
		if (node < 0) {
			QToolTip::hideText();
		}
	}
	if (node >= 0) {
		showNodeTip(node, event->globalPosition().toPoint());
	}
}

void SearchTreeView::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() != Qt::LeftButton || !dragging) {
		return;
	}
	dragging = false;
	unsetCursor();
	if (dragMoved || !model) {
		return;
	}
	//a click opens a node's other moves or closes them again
	const int node = findNodeAt(event->position());
	if (node < 0) {
		return;
	}
	if (expandedNodes.contains(node)) {
		expandedNodes.erase(node);
	} else {
		expandedNodes.insert(node);
	}
	rebuildDisplay(true);
	startFrames();
}

void SearchTreeView::mouseDoubleClickEvent(QMouseEvent* event) {
	const int node = findNodeAt(event->position());
	if (node >= 0) {
		setFollowing(false);
		targetCenter = targetPositions[static_cast<std::size_t>(node)];
		targetZoom = {1.1, 1.1};
		startFrames();
	}
}

void SearchTreeView::leaveEvent(QEvent* event) {
	QWidget::leaveEvent(event);
	hoveredNode = -1;
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

void SearchTreeView::showNodeTip(int node, QPoint globalPos) {
	const SearchTreeModel& tree = *model;
	const SearchTreeNode& data = tree.getNode(node);
	QString text;
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
		const int refutation = tree.getChildren(node).back();
		const int skipped = tree.getSkippedMoves(node);
		text += "<br>" + tr("<span style='color:%1'><b>Cut short:</b></span> %2 refutes it, so %3 other replies were skipped.")
			.arg(CUTOFF_COLOR.name(), tree.getMoveName(refutation).toHtmlEscaped()).arg(skipped);
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
	const int hidden = static_cast<int>(data.searchedChildren) - static_cast<int>(shownChildren[static_cast<std::size_t>(node)].size());
	if (hidden > 0) {
		text += "<br><i>" + (expandedNodes.contains(node) ? tr("%1 more moves weren't recorded.").arg(hidden)
			: tr("%1 more searched moves are hidden, click to show them.").arg(hidden)) + "</i>";
	} else if (expandedNodes.contains(node)) {
		text += "<br><i>" + tr("Click to fold its moves again.") + "</i>";
	}
	QToolTip::showText(globalPos + QPoint(14, 14), text, this);
}

void SearchTreeView::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	QPainter painter(this);
	painter.fillRect(rect(), palette().color(QPalette::Base));
	if (!model || model->getNodeCount() == 0) {
		paintEmpty(painter);
		return;
	}
	painter.setRenderHint(QPainter::Antialiasing);
	paintLanes(painter);
	paintEdges(painter);
	paintNodes(painter);
	paintLaneHeaders(painter);
}

void SearchTreeView::paintEmpty(QPainter& painter) const {
	painter.setPen(withAlpha(palette().color(QPalette::Text), 130));
	painter.drawText(rect(), Qt::AlignCenter, tr("No search yet.\nLet Rhea move in the play view and watch it think."));
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
	//the horizon
	const qreal horizon = toScreen({(snapshot.depth + 0.5) * COLUMN_WIDTH, 0}).x();
	painter.setPen(QPen(withAlpha(CUTOFF_COLOR, 140), 1.5, Qt::DashLine));
	painter.drawLine(QPointF(horizon, HEADER_HEIGHT), QPointF(horizon, height()));
}

void SearchTreeView::paintLaneHeaders(QPainter& painter) const {
	const SearchSnapshot& snapshot = model->getSnapshot();
	const QColor text = palette().color(QPalette::Text);
	painter.fillRect(QRectF(0, 0, width(), HEADER_HEIGHT), withAlpha(palette().color(QPalette::Window), 235));
	QFont font = painter.font();
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
	const qreal density = std::clamp(ROW_HEIGHT * zoom.y() / 10, 0.3, 1.0);	//rows close together: quieter edges
	const QRectF visible = QRectF(rect()).adjusted(-40, -40, 40, 40);
	const qreal pulse = 0.5 + 0.5 * std::sin(pulseClock.elapsed() / 180.0);

	for (int pass = 0; pass < 2; ++pass) {	//the highlighted edges go on top
		for (int node : shownNodes) {
			if (node == 0) {
				continue;
			}
			const bool onLine = tree.isOnBestLine(node);
			const bool current = tree.isOnCurrentPath(node);
			if ((onLine || current) != (pass == 1)) {
				continue;
			}
			const int parent = tree.getNode(node).parent;
			const QPointF from = toScreen(drawPositions[static_cast<std::size_t>(parent)]) + QPointF(halfBox, 0);
			const QPointF to = toScreen(drawPositions[static_cast<std::size_t>(node)]) - QPointF(halfBox, 0);
			if (!visible.intersects(QRectF(from, to).normalized().adjusted(-1, -1, 1, 1))) {
				continue;
			}
			QPainterPath path(from);
			if (std::abs(from.y() - to.y()) < 0.5) {
				path.lineTo(to);
			} else {
				const qreal middle = (from.x() + to.x()) / 2;
				path.lineTo(middle, from.y());
				path.lineTo(middle, to.y());
				path.lineTo(to);
			}
			const qreal effort = std::log(static_cast<qreal>(std::max<std::uint32_t>(1, tree.getNode(node).subtreeNodes))) / logMax;
			const qreal width = (0.7 + 3.6 * effort) * zoomFactor;
			if (current) {
				painter.setPen(QPen(withAlpha(SEARCHING_COLOR, static_cast<int>(120 + 100 * pulse)), width + 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			} else if (onLine) {
				painter.setPen(QPen(withAlpha(LINE_COLOR, 60), width + 8 * zoomFactor, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
				painter.drawPath(path);
				painter.setPen(QPen(LINE_COLOR, width + 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			} else {
				const bool hovered = node == hoveredNode;
				painter.setPen(QPen(withAlpha(text, hovered ? 170 : static_cast<int>(70 * density)), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			}
			painter.drawPath(path);
		}
	}
}

void SearchTreeView::paintNodes(QPainter& painter) const {
	const SearchTreeModel& tree = *model;
	const QColor text = palette().color(QPalette::Text);
	const qreal boxWidth = BOX_WIDTH * zoom.x();
	const qreal boxHeight = BOX_HEIGHT * zoom.y();
	const QRectF visible = QRectF(rect()).adjusted(-boxWidth, -boxHeight, boxWidth, boxHeight);
	const bool detailed = boxHeight >= TEXT_MIN_HEIGHT && boxWidth >= 64;
	const qreal textZoom = std::min(zoom.x(), zoom.y());
	const bool boxes = boxHeight >= 5;
	const qreal pulse = 0.5 + 0.5 * std::sin(pulseClock.elapsed() / 180.0);

	QFont nameFont = painter.font();
	nameFont.setPixelSize(static_cast<int>(std::clamp(std::min(boxHeight * 0.55, boxWidth * 0.12), 8.0, 18.0)));
	nameFont.setBold(true);
	QFont scoreFont = nameFont;
	scoreFont.setBold(false);
	scoreFont.setPixelSize(std::max(8, static_cast<int>(nameFont.pixelSize() * 0.88)));
	QFont badgeFont = scoreFont;
	badgeFont.setPixelSize(std::max(8, static_cast<int>(nameFont.pixelSize() * 0.75)));

	for (int node : shownNodes) {
		const QPointF center = toScreen(drawPositions[static_cast<std::size_t>(node)]);
		if (!visible.contains(center)) {
			continue;
		}
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
		if (node == hoveredNode) {
			opacity = 1;
			outline.setWidthF(outline.widthF() + 1);
		}
		if (!boxes) {
			outline.setWidthF(std::min(outline.widthF(), 1.4));
		}
		painter.setOpacity(opacity);

		if (!boxes) {
			const qreal radius = std::clamp(boxHeight * 0.5, 1.8, 3.5) + (onLine ? 1.2 : 0);
			painter.setPen(kind == SearchNodeKind::FAILED_LOW && !onLine ? Qt::NoPen : outline);
			painter.setBrush(fill);
			painter.drawEllipse(center, radius, radius);
			continue;
		}

		const QRectF box(center.x() - boxWidth / 2, center.y() - boxHeight / 2, boxWidth, boxHeight);
		if (onLine) {	//the best line glows
			painter.setPen(Qt::NoPen);
			painter.setBrush(withAlpha(LINE_COLOR, 55));
			painter.drawRoundedRect(box.adjusted(-4 * textZoom, -4 * textZoom, 4 * textZoom, 4 * textZoom), 8 * textZoom, 8 * textZoom);
		}
		painter.setPen(outline);
		painter.setBrush(fill);
		const qreal corner = tree.isLeaf(node) ? 2 * textZoom : 6 * textZoom;	//leaves (evaluated at the horizon) are squarer
		painter.drawRoundedRect(box, corner, corner);

		if (detailed) {
			const QColor ink = fill.lightness() > 140 ? QColor(25, 25, 28) : QColor(238, 238, 240);
			const QRectF inner = box.adjusted(8 * textZoom, 0, -8 * textZoom, 0);
			painter.setPen(ink);
			painter.setFont(nameFont);
			painter.drawText(inner, Qt::AlignVCenter | Qt::AlignLeft, tree.getMoveName(node));
			painter.setFont(scoreFont);
			painter.setPen(withAlpha(ink, 200));
			painter.drawText(inner, Qt::AlignVCenter | Qt::AlignRight, tree.getScoreText(node));
		}

		//badges: searched moves that aren't shown, and moves a cutoff skipped
		if (detailed) {
			painter.setOpacity(1);
			painter.setFont(badgeFont);
			const QFontMetricsF metrics(badgeFont);
			const int hidden = static_cast<int>(tree.getNode(node).searchedChildren)
				- static_cast<int>(shownChildren[static_cast<std::size_t>(node)].size());
			//stacked just right of the box: hidden moves above the middle, skipped ones below, the edge runs between them
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
			if (hidden > 0) {
				drawBadge(QString("+%1").arg(hidden), withAlpha(text, 170), false, true);
			}
			if (cutShort) {
				drawBadge(QString::fromUtf8("✂%1").arg(tree.getSkippedMoves(node)), CUTOFF_COLOR, true, false);
			}
		}
	}
	painter.setOpacity(1);
}
