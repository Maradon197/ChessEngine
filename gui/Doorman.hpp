#ifndef DOORMAN_HPP
#define DOORMAN_HPP

#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QThread>

#include "DataTypes.hpp"
#include "GuiTypes.hpp"

class BoardWidget;
class ReviewWidget;
class MoveTreeWidget;
class EvalSquareWidget;
class EvalCriteriaWidget;
class SidePanelWidget;

//The single gate between the GUI and the rest of the project.
//
// IN  (core -> GUI): the public set/add/remove/clear functions below.
//     Safe to call from any thread; calls are forwarded to the GUI thread.
// OUT (GUI -> core): the signals below. Emitted in the GUI thread.
//
//Setters never emit signals, so the core can react to a signal by calling setters without loops.
//Widgets register themselves through attach() in their constructors; nothing else touches them.
//
//Board and move tree are kept in sync here: the current tree node always holds the position
//shown on the board. Selecting a node restores its position; playing a move from a node either
//follows an existing child with the same move or forks a new branch.
class Doorman : public QObject {
	Q_OBJECT

public:
	explicit Doorman(QObject* parent = nullptr);
	~Doorman() override;

	// ---------- IN: board ----------
	void setPosition(const Position& position);			//shows it and stores it in the current tree node
	void playMove(Move move, const std::string& label = {});	//moves the pieces and advances/forks the move tree
	void setLegalMoves(const MoveList& legalMoves);		//all legal moves of the position; shown for the selected piece
	void clearLegalMoves();
	void setLastMove(Move move);
	void clearLastMove();
	void setBoardOrientation(Color bottomColor);
	void setBoardInteractive(bool interactive);			//false: no move input (annotations still work)

	void setUserHighlights(const std::vector<Highlight>& highlights);
	void setUserArrows(const std::vector<Arrow>& arrows);
	void clearUserAnnotations();
	void setEngineHighlights(const std::vector<Highlight>& highlights);
	void setEngineArrows(const std::vector<Arrow>& arrows);
	void clearEngineAnnotations();

	// ---------- IN: evaluation ----------
	void setOverallCentipawns(int centipawns);			//White's perspective, like Stockfish
	void setOverallMate(int movesToMate);				//positive: White mates, negative: Black mates
	void clearOverallEval();
	void setCriterion(EvalCriterion criterion, int value);	//clamped to CRITERION_MIN..CRITERION_MAX
	void clearCriteria();

	// ---------- IN: review panel ----------
	void addReviewEntry(ReviewEntryId id, const std::string& title, const std::string& text);	//existing id: replaces content
	void removeReviewEntry(ReviewEntryId id);
	void clearReviewEntries();
	void focusReviewEntry(ReviewEntryId id);			//marks it active and scrolls to it

	// ---------- IN: move tree ----------
	void setTreeOrigin(Clock fullmoveNumber, Color sideToMove);	//move numbering of the root position
	//adds a node without touching the board (e.g. engine lines, loading a game); returns its id.
	//position = position after the move. First child of a node = main line.
	NodeId addTreeNode(NodeId parent, Move move, const Position& position, const std::string& label = {});
	void removeTreeNode(NodeId id);						//removes the whole subtree
	void clearTree();									//only the root remains, holding the current board position
	void setCurrentTreeNode(NodeId id);					//also shows the node's position on the board

	// ---------- IN: appearance ----------
	void setThemeMode(ThemeMode mode);
	void setColor(ColorRole role, ColorValue color);
	void resetColors();									//back to the theme defaults

	// ---------- queries (GUI thread only) ----------
	std::vector<Move> getMovesToTreeNode(NodeId id) const;	//moves from the root to the node, in order

	// ---------- queries (any thread; up to date immediately when called from the GUI thread) ----------
	Position getPosition() const;
	NodeId getCurrentTreeNode() const;
	std::vector<Highlight> getUserHighlights() const;
	std::vector<Arrow> getUserArrows() const;
	ThemeMode getThemeMode() const;
	ColorValue getColor(ColorRole role) const;

	// ---------- widget registration (called by the widgets themselves) ----------
	void attach(BoardWidget& boardWidget);
	void attach(ReviewWidget& reviewWidget);
	void attach(MoveTreeWidget& moveTreeWidget);
	void attach(EvalSquareWidget& evalSquareWidget);
	void attach(EvalCriteriaWidget& evalCriteriaWidget);
	void attach(SidePanelWidget& sidePanelWidget);

signals:
	// ---------- OUT ----------
	void moveAttempted(Move move);			//user tried a move: verify it, then answer with playMove()
	void squareSelected(Square square);		//user picked up / selected a piece
	void selectionCleared();
	void userAnnotationsChanged(const std::vector<Highlight>& highlights, const std::vector<Arrow>& arrows);
	void treeNodeSelected(NodeId id, const Position& position);	//user went to another node; the board already shows position
	void reviewEntryActivated(ReviewEntryId id);

private:
	template <typename Function>
	void runInGuiThread(Function&& function);

	//GUI thread only
	void showTreeNode(NodeId id);
	void rememberBoardState();
	void applyTheme();
	void applyColors();

	QPointer<BoardWidget> board;
	QPointer<ReviewWidget> review;
	QPointer<MoveTreeWidget> moveTree;
	QPointer<EvalSquareWidget> evalSquare;
	QPointer<EvalCriteriaWidget> evalCriteria;
	QPointer<SidePanelWidget> sidePanel;

	std::atomic<NodeId> nextNodeId{ROOT_NODE + 1};

	//copies for the thread-safe queries
	mutable std::mutex stateMutex;
	Position currentPosition{};
	NodeId currentNode = ROOT_NODE;
	std::vector<Highlight> userHighlights;
	std::vector<Arrow> userArrows;
	ThemeMode themeMode = ThemeMode::SYSTEM;
	bool darkTheme = false;
	std::array<std::optional<ColorValue>, COLOR_ROLE_COUNT> customColors;
};

template <typename Function>
void Doorman::runInGuiThread(Function&& function) {
	if (QThread::currentThread() == thread()) {
		function();
	} else {
		QMetaObject::invokeMethod(this, std::forward<Function>(function), Qt::QueuedConnection);
	}
}

#endif
