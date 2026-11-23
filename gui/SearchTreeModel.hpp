#ifndef SEARCHTREEMODEL_HPP
#define SEARCHTREEMODEL_HPP

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <QString>

#include "DataTypes.hpp"
#include "SearchRecorder.hpp"

//what a searched move turned out to be, read from its score and the alpha / beta window it got
enum class SearchNodeKind : std::uint8_t {
	ROOT,
	SEARCHING,	//not finished yet
	BEST,		//better than everything the parent had so far: raised alpha
	CUTOFF,		//so good the opponent would never allow it: score >= beta, the parent's other moves were skipped
	FAILED_LOW,	//not better than what the parent already had (score <= alpha), its value is only an upper bound
};

//reads one SearchSnapshot: node kinds, the best line, move names, scores from white's view
//built on the GUI thread for every new snapshot; move names are carried over from the previous model of the same search
class SearchTreeModel {
public:
	SearchTreeModel(std::shared_ptr<const SearchSnapshot> snapshot, const SearchTreeModel* previous);

	const SearchSnapshot& getSnapshot() const { return *snapshot; }
	int getNodeCount() const { return static_cast<int>(snapshot->nodes.size()); }
	const SearchTreeNode& getNode(int index) const { return snapshot->nodes[static_cast<std::size_t>(index)]; }
	std::vector<int> getChildren(int index) const;	//recorded ones, in search order
	int getChildCount(int index) const { return childCounts[static_cast<std::size_t>(index)]; }	//recorded ones
	int getLastChild(int index) const { return lastChildren[static_cast<std::size_t>(index)]; }	//-1 if none

	SearchNodeKind getKind(int index) const { return kinds[static_cast<std::size_t>(index)]; }
	bool isCutShort(int index) const;	//one of its moves caused a beta cutoff, the moves after it were never searched
	int getSkippedMoves(int index) const;	//how many moves that cutoff skipped, -1 if unknown
	bool isLeaf(int index) const;		//at the search depth: evaluated, not searched further
	bool isTerminal(int index) const;	//finished before the depth without a move: mate or stalemate
	bool hasAllChildrenRecorded(int index) const;
	int getBestChild(int index) const;	//the move it currently prefers, -1 if none
	const std::vector<int>& getBestLine() const { return bestLine; }	//from the root's best move on
	bool isOnBestLine(int index) const { return onBestLine[static_cast<std::size_t>(index)]; }
	bool isOnCurrentPath(int index) const { return onCurrentPath[static_cast<std::size_t>(index)]; }
	std::uint32_t getMaxSubtree() const { return maxSubtree; }	//biggest subtree below the root's children

	Color getMover(int index) const;	//who made the node's move (the root: who is to move)
	float getWhiteScore(int index) const;	//pawns from white's view
	QString getScoreText(int index) const;	//"+0.35", "≥ −1.20", "#3", "…" while searching
	QString getMoveName(int index) const;	//SAN, "Root" for the root
	QString getLineText(int index, int maxMoves = 64) const;	//"1. e4 e5 2. Nf3" from the root to the node
	int getLegalMoveCount(int index) const;	//asks the core, cached

	struct RootMove {
		Move move = 0;
		int node = -1;	//-1: not searched yet
	};
	const std::vector<RootMove>& getRootMoves() const { return rootMoves; }	//all legal moves at the root, searched ones first in search order

	static QString formatScore(float whiteScore);

private:
	BoardState getBoard(int index) const;	//the root's board with the moves to the node made

	std::shared_ptr<const SearchSnapshot> snapshot;
	//children as linked lists in flat arrays: a vector per node would mean 250k allocations per snapshot
	std::vector<int> firstChildren;
	std::vector<int> lastChildren;
	std::vector<int> nextSiblings;
	std::vector<int> childCounts;
	std::vector<SearchNodeKind> kinds;
	std::vector<int> bestChildren;
	std::vector<int> bestLine;
	std::vector<bool> onBestLine;
	std::vector<bool> onCurrentPath;
	std::vector<RootMove> rootMoves;
	std::uint32_t maxSubtree = 1;

	mutable std::unordered_map<int, QString> moveNames;
	mutable std::unordered_map<int, int> legalMoveCounts;
};

#endif
