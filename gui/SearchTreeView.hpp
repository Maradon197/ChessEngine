#ifndef SEARCHTREEVIEW_HPP
#define SEARCHTREEVIEW_HPP

#include <memory>
#include <unordered_set>
#include <vector>

#include <QElapsedTimer>
#include <QPointF>
#include <QWidget>

#include "TreeLayout.hpp"

class QPainter;
class QTimer;
class SearchTreeModel;

//the search tree, laid out like the move tree: each row is a line the engine looked at, the best line is row 0
//	fill		evaluation from white's view, black <-> white
//	outline		what the move turned out to be: caused a cutoff, cut short, raised alpha, failed low, still searching
//	edges		get thicker the more nodes were searched below them
//	columns		one lane per ply, the last one is the search horizon where positions are evaluated
//every ply up to the expand depth is shown completely, deeper only each node's best continuation (click a node for all)
//wheel zooms (with Shift only the rows), dragging pans, F fits everything, + / - change the expand depth
//width and height are zoomed separately: a search tree is a few plies wide but hundreds of lines tall
class SearchTreeView : public QWidget {
	Q_OBJECT

public:
	explicit SearchTreeView(QWidget* parent = nullptr);
	~SearchTreeView() override;

	void setModel(std::shared_ptr<const SearchTreeModel> model);

	void setExpandPly(int ply);
	int getExpandPly() const { return expandPly; }
	int getShownPly() const { return shownPly; }	//can be lower than the expand ply when that would be too many nodes
	int getShownNodeCount() const { return static_cast<int>(shownNodes.size()); }

	void fitToView();	//and follow the search again
	void centerOn(int node);
	bool isFollowing() const { return following; }

	static constexpr int MAX_SHOWN_NODES = 6000;

signals:
	void displayChanged();	//shown ply or node count changed
	void followingChanged(bool following);

protected:
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void leaveEvent(QEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private:
	void rebuildDisplay(bool animate);	//which nodes are shown, their order and layout
	void setFollowing(bool follow);
	void startFrames();
	void onFrame();
	void computeFit(QPointF& center, QPointF& scale) const;
	QPointF toScreen(QPointF treePoint) const;
	QPointF toTree(QPointF screenPoint) const;
	int findNodeAt(QPointF screenPoint) const;
	void showNodeTip(int node, QPoint globalPos);

	void paintLanes(QPainter& painter) const;
	void paintEdges(QPainter& painter) const;
	void paintNodes(QPainter& painter) const;
	void paintLaneHeaders(QPainter& painter) const;
	void paintEmpty(QPainter& painter) const;

	std::shared_ptr<const SearchTreeModel> model;
	std::unordered_set<int> expandedNodes;	//clicked open, for the current search
	int expandPly = 2;
	int shownPly = 2;

	std::vector<int> shownNodes;	//parents before children
	std::vector<std::vector<int>> shownChildren;	//per model node, display order, the best move first
	std::vector<int> rows;	//per model node, only valid for shown ones
	std::vector<bool> isShown;
	treeLayout::Bounds bounds;

	std::vector<QPointF> drawPositions;	//per model node, animated towards the layout
	std::vector<QPointF> fromPositions;
	std::vector<QPointF> targetPositions;
	qreal moveProgress = 1;
	QElapsedTimer moveClock;

	QPointF viewCenter;	//tree coordinates in the middle of the widget
	QPointF zoom{1, 1};	//screen pixels per tree unit, horizontally and vertically
	QPointF targetCenter;
	QPointF targetZoom{1, 1};
	bool following = true;

	QTimer* frameTimer = nullptr;
	QElapsedTimer pulseClock;
	int hoveredNode = -1;
	bool dragging = false;
	bool dragMoved = false;
	QPointF dragStart;
	QPointF dragLast;
};

#endif
