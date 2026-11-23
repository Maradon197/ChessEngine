#ifndef SEARCHWINDOW_HPP
#define SEARCHWINDOW_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include <QWidget>

#include "GuiTypes.hpp"

class KeyBindings;
class QLabel;
class QTimer;
class QToolButton;
class SearchRecorder;
class SearchTreeModel;
class SearchTreeView;
class SearchStatsBar;
class RootMovesStrip;
class BestLineBar;

//what the engine is thinking, its own window (W): numbers at the top, the root moves as a strip, the search tree in the
//middle, the best line and a legend at the bottom. While shown it asks the recorder for a new snapshot UPDATE_INTERVAL
//times a second; closed, it asks nothing and the search isn't slowed down by copying
class SearchWindow : public QWidget {
	Q_OBJECT

public:
	SearchWindow(KeyBindings& keyBindings, QWidget& mainWindow);

	void setRecorder(SearchRecorder& recorder);
	void toggle();

	static constexpr int UPDATE_INTERVAL_MS = 250;	//4 times a second

signals:
	void lineChosen(NodeId origin, const std::vector<Move>& moves);	//a move in the tree was clicked: play these from origin
	void shownChanged(bool shown);

protected:
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private:
	void poll();	//takes a new snapshot if there is one and asks for the next
	void updateControls();

	KeyBindings& keyBindings;
	SearchRecorder* recorder = nullptr;
	std::shared_ptr<const SearchTreeModel> model;
	std::uint64_t generation = 0;

	QTimer* pollTimer = nullptr;
	SearchStatsBar* stats = nullptr;
	RootMovesStrip* rootMoves = nullptr;
	SearchTreeView* treeView = nullptr;
	BestLineBar* bestLine = nullptr;
	QLabel* expandLabel = nullptr;
	QToolButton* followButton = nullptr;
};

#endif
