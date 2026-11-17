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
class MoveTreeOverlay;
class EvalSquareWidget;
class EvalCriteriaWidget;

//all setters are thread-safe and never emit signals
class Doorman : public QObject {
	Q_OBJECT

public:
	explicit Doorman(QObject* parent = nullptr);
	~Doorman() override;

	//board
	void setPosition(const Position& position);	//also stored in the current tree node
	void playMove(Move move, const std::string& label = {});	//follows an existing child or forks a new branch
	void setLegalMoves(const MoveList& legalMoves);
	void clearLegalMoves();
	void setLastMove(Move move);
	void clearLastMove();
	void setBoardOrientation(Color bottomColor);
	void setBoardInteractive(bool interactive);

	void setUserHighlights(const std::vector<Highlight>& highlights);
	void setUserArrows(const std::vector<Arrow>& arrows);
	void clearUserAnnotations();
	void setEngineHighlights(const std::vector<Highlight>& highlights);
	void setEngineArrows(const std::vector<Arrow>& arrows);
	void clearEngineAnnotations();

	//evaluation
	void setOverallCentipawns(int centipawns);	//white's perspective
	void setOverallMate(int movesToMate);	//negative: black mates
	void clearOverallEval();
	void setCriterion(EvalCriterion criterion, int value);
	void clearCriteria();

	//move tree
	void setTreeOrigin(Clock fullmoveNumber, Color sideToMove);
	NodeId addTreeNode(NodeId parent, Move move, const Position& position, const std::string& label = {});
	void removeTreeNode(NodeId id);
	void clearTree();
	void setCurrentTreeNode(NodeId id);

	//appearance
	void setThemeMode(ThemeMode mode);
	void setColor(ColorRole role, ColorValue color);
	void resetColors();

	//queries
	std::vector<Move> getMovesToTreeNode(NodeId id) const;	//GUI thread only
	Position getPosition() const;
	NodeId getCurrentTreeNode() const;
	std::vector<Highlight> getUserHighlights() const;
	std::vector<Arrow> getUserArrows() const;
	ThemeMode getThemeMode() const;
	ColorValue getColor(ColorRole role) const;

	//called by the widgets in their constructors
	void attach(BoardWidget& boardWidget);
	void attach(MoveTreeOverlay& moveTreeOverlay);
	void attach(EvalSquareWidget& evalSquareWidget);
	void attach(EvalCriteriaWidget& evalCriteriaWidget);

signals:
	void moveAttempted(Move move);	//not verified, answer with playMove()
	void squareSelected(Square square);
	void selectionCleared();
	void userAnnotationsChanged(const std::vector<Highlight>& highlights, const std::vector<Arrow>& arrows);
	void treeNodeSelected(NodeId id, const Position& position);	//board already shows the position

private:
	template <typename Function>
	void runInGuiThread(Function&& function);

	void showTreeNode(NodeId id);
	void rememberBoardState();
	void applyTheme();
	void applyColors();

	QPointer<BoardWidget> board;
	QPointer<MoveTreeOverlay> moveTree;
	QPointer<EvalSquareWidget> evalSquare;
	QPointer<EvalCriteriaWidget> evalCriteria;

	std::atomic<NodeId> nextNodeId{ROOT_NODE + 1};

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
