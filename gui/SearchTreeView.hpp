#ifndef SEARCHTREEVIEW_HPP
#define SEARCHTREEVIEW_HPP

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QElapsedTimer>
#include <QPointF>
#include <QWidget>

#include "DataTypes.hpp"
#include "TreeLayout.hpp"

class QPainter;
class QTimer;
class SearchTreeModel;

//the search tree, laid out like the move tree: each row is a line the engine looked at, the best line is row 0
//	fill		evaluation from white's view, black <-> white
//	outline		what the move turned out to be: caused a cutoff, cut short, raised alpha, failed low, still searching
//	edges		get thicker the more nodes were searched below them
//	columns		one lane per ply up to the search horizon (dashed line) where positions are evaluated; behind it,
//				barely visible, the captures the quiescence search played out on the shown lines
//	order		a faint bar on a move's left edge: the brighter, the earlier the engine's move ordering tried it
//
//the layout holds still while the engine searches, so it can be watched:
//	every root move gets a band of rows, top to bottom in search order (like the strip above); all bands exist from the
//	start, root moves not searched yet are placeholders. The first row of a band is the root move with its best line so
//	far, when a better move turns up only the names in that row change. With two plies shown the band has some more rows
//	for the root move's other replies: a few on their own (with their best lines) and a group symbol for the rest. The
//	MAX_LINES lines are split evenly between the bands once, when the search starts.
//	Things only move when the user opens or folds something, and once when the search is done: rows that stayed empty
//	are dropped and the view is framed again. The frame is set by the full search only; when quiescence lines reach
//	past the right edge, the view slides left (never zooms) as far as the root stays visible.
//Clicking a group opens it, the "fold" symbol closes it again, the "+N" badge shows a move's other replies.
//Clicking a move asks to play the line up to it (see nodeActivated).
//wheel zooms (with Shift only the rows), dragging pans, F fits everything, + / - switch between one and two plies
class SearchTreeView : public QWidget {
	Q_OBJECT

public:
	explicit SearchTreeView(QWidget* parent = nullptr);
	~SearchTreeView() override;

	void setModel(std::shared_ptr<const SearchTreeModel> model);

	void setExpandPly(int ply);
	int getExpandPly() const { return expandPly; }
	int getLineCount() const { return lineCount; }

	void fitToView();	//and follow the search again
	void centerOn(int node);	//opens the groups on the way if needed
	bool isFollowing() const { return following; }

	static constexpr int MAX_LINES = 35;

signals:
	void displayChanged();	//expand depth or line count changed
	void followingChanged(bool following);
	void nodeActivated(int node);	//a move was clicked

protected:
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void leaveEvent(QEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private:
	//one thing drawn in the tree: a move, or a group standing for some moves of a node
	struct Item {
		int node = 0;	//the move; for groups the node whose moves are gathered; -1 for placeholders
		bool group = false;
		bool placeholder = false;	//a root move not searched yet
		Move move = 0;	//placeholders: which one
		int rootIndex = 0;	//placeholders: its place among the root moves
		bool fold = false;	//the group of an opened node: closes it again
		int parent = -1;	//item index, -1 for the root
		int ply = 0;
		std::vector<int> children;	//item indices, display order: the preferred move first
		std::vector<int> members;	//groups: the moves inside
		int hidden = 0;	//moves: searched below it but neither shown nor in a group ("+N" badge)
		int hiddenRecorded = 0;	//of those, the ones that could be shown: clicking the badge opens the node
		int row = 0;
		QPointF from;
		QPointF target;
		QPointF draw;

		std::int64_t getKey() const {	//stable across snapshots
			if (placeholder) return -1'000'000'000LL - rootIndex;
			return group ? -static_cast<std::int64_t>(node) - 2 : node;
		}
	};

	void rebuildDisplay(bool animate);
	int getContinuation(int node) const;	//the move shown in the node's row after it: its best so far, or the one being searched
	std::vector<int> getOtherChildren(int node) const;	//all but the continuation: by score once done, in search order before
	void addLine(int node, int parentItem, int row, int& nextRow);	//the node and its continuations on one row, opened ones below
	int addItem(Item item);
	void setFollowing(bool follow);
	void startFrames();
	void onFrame();
	void computeFit(QPointF& center, QPointF& scale) const;
	qreal getSlideCenterX(qreal centeredX, qreal zoomX) const;	//centeredX moved left as far as quiescence needs and the root allows
	QPointF toScreen(QPointF treePoint) const;
	QPointF toTree(QPointF screenPoint) const;
	int findItemAt(QPointF screenPoint) const;
	int findBadgeAt(QPointF screenPoint) const;	//an item whose "+N" badge is there
	QRectF getBadgeRect(const Item& item, const QString& label, bool above) const;
	void showItemTip(int item, QPoint globalPos);

	void paintLanes(QPainter& painter) const;
	void paintEdges(QPainter& painter) const;
	void paintItems(QPainter& painter) const;
	void paintGroup(QPainter& painter, const Item& item, const QRectF& box, bool detailed) const;
	void paintLaneHeaders(QPainter& painter) const;
	void paintEmpty(QPainter& painter) const;

	std::shared_ptr<const SearchTreeModel> model;
	std::unordered_set<int> openedNodes;	//groups clicked open, for the current search
	int expandPly = 2;
	int lineCount = 0;

	std::vector<Item> items;	//items[0] is the root, parents before children
	int rowCount = 0;
	std::unordered_map<int, int> itemOfNode;	//shown moves
	treeLayout::Bounds bounds;	//maxColumn includes quiescence
	int mainMaxColumn = 0;	//deepest column up to the horizon
	int quiescenceMaxColumn = 0;	//deepest quiescence column shown, 0 if none
	qreal moveProgress = 1;
	QElapsedTimer moveClock;

	QPointF viewCenter;	//tree coordinates in the middle of the widget
	QPointF zoom{1, 1};	//screen pixels per tree unit, horizontally and vertically
	QPointF targetCenter;
	QPointF targetZoom{1, 1};
	bool following = true;

	QTimer* frameTimer = nullptr;
	QElapsedTimer pulseClock;
	int hoveredItem = -1;
	bool dragging = false;
	bool dragMoved = false;
	QPointF dragStart;
	QPointF dragLast;
};

#endif
