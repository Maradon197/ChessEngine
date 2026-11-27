#ifndef SEARCHRECORDER_HPP
#define SEARCHRECORDER_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "DataTypes.hpp"
#include "BoardState.hpp"
#include "EngineTypes.hpp"
#include "GuiTypes.hpp"
#include "SearchObserver.hpp"

struct SearchResult;

//one move the engine searched; all values exactly as the engine had them, the window interprets them
struct SearchTreeNode {
	std::int32_t parent = -1;	//index into SearchSnapshot::nodes, -1 for the root
	Move move = 0;	//leads from the parent here, 0 for the root
	EvalValue alpha = 0;	//the window the node was searched with, view of the side to move in the node
	EvalValue beta = 0;
	EvalValue score = 0;	//view of the side that made the move (the root: view of the side to move); only valid when finished
	std::uint32_t subtreeNodes = 1;	//this node and everything below it, also what wasn't recorded
	std::uint16_t searchedChildren = 0;	//moves searched below it, also unrecorded ones
	std::uint16_t order = 0;	//its place in the parent's move ordering: 0 = the move the engine tried first
	std::uint8_t ply = 0;	//0 = root, beyond SearchSnapshot::depth: quiescence (captures behind the horizon)
	bool finished = false;
};

//a copy of the recording at one moment, handed to the GUI thread
struct SearchSnapshot {
	std::uint64_t searchId = 0;	//counts searches, a new one means the tree started over
	std::uint64_t generation = 0;	//counts snapshots
	NodeId origin = ROOT_NODE;	//the move tree node of the searched position
	unsigned tag = 0;	//whatever the caller passed to begin(), e.g. which request this search answers
	BoardState root;	//the searched position with its history, the window replays moves on copies for the move names
	int depth = 0;	//plies, as requested: the horizon
	int quiescenceDepth = 0;	//plies of captures the engine may play out behind the horizon, 0 = off
	std::vector<Move> rootMoves;	//the root moves this search looks at, empty: all legal ones
	std::vector<SearchTreeNode> nodes;	//parents before their children, children in search order
	std::vector<std::int32_t> currentPath;	//recorded nodes from the root to where the engine is right now (empty when done)
	int currentPly = 0;	//how deep the engine is right now, also beyond the recording
	int deepestPly = 0;
	std::uint64_t totalNodes = 0;
	double seconds = 0;
	bool finished = false;
	bool truncated = false;	//the node budget ran out, deeper parts of later moves are only counted
	Move bestMove = 0;	//when finished
	EvalValue bestScore = 0;	//when finished, view of the side to move at the root
	Move bestMoveSoFar = 0;	//while searching: the root move that raised alpha last
	EvalValue bestScoreSoFar = 0;	//its score, view of the side to move at the root
};

//the GUI's SearchObserver: writes down what the engine searches, for the search window
//
//what is recorded: every node of the first ALWAYS_RECORDED_PLY plies, deeper ones while NODE_BUDGET lasts (quiescence
//nodes behind the horizon never, they only come in as parts of best lines). On top of
//that, every recorded node gets its best continuation down to the horizon once it is finished, also where the budget
//had run out: like an engine's PV table, each frame carries the best line found below it and hands it up when it raises
//the parent's alpha. So the window can always show each move's best line, and the root's best line is complete.
//
//threads: begin(), enterNode(), leaveNode() and finish() run on the engine thread, and only there is the recording
//touched, so the hot path needs no lock. The window calls requestSnapshot() a few times a second; at its next node the
//engine thread copies the recording into a snapshot and hands it over under the mutex, takeSnapshot() picks it up.
//While nobody asks, nothing is copied.
class SearchRecorder : public SearchObserver {
public:
	SearchRecorder();

	//engine thread, around Searcher::searchPosition
	void begin(const BoardState& root, int depth, NodeId origin = ROOT_NODE, unsigned tag = 0,
		const std::vector<Move>& rootMoves = {}, int quiescenceDepth = 0);	//rootMoves: only these are searched, empty: all
	void finish(const SearchResult& result);

	//SearchObserver, engine thread
	void enterNode(Move move, const SearchInfo& child_info) override;
	void leaveNode(EvalValue score) override;

	//GUI thread
	void requestSnapshot();
	std::shared_ptr<const SearchSnapshot> takeSnapshot(std::uint64_t newerThan) const;	//nullptr if nothing newer

	static constexpr int ALWAYS_RECORDED_PLY = 3;	//the first plies are always recorded completely
	static constexpr std::size_t NODE_BUDGET = 250'000;	//deeper than that only while the budget lasts (~7 MB)
	static constexpr int MAX_LINE = 64;	//longest best continuation kept per frame

private:
	struct LineEntry {
		SearchTreeNode node;
		std::int32_t recorded = -1;	//index in nodes once it is recorded
	};
	struct Frame {	//one per ply on the way from the root to the node being searched
		std::int32_t node = -1;	//recorded index, -1 if not recorded
		Move move = 0;
		EvalValue alpha = 0;
		EvalValue beta = 0;
		std::uint32_t subtreeNodes = 1;
		std::uint16_t searchedChildren = 0;
		std::uint16_t order = 0;
		int lineLength = 0;
		std::array<LineEntry, MAX_LINE> line;	//best continuation below it so far
	};

	void recordLine(Frame& frame);	//the frame's best continuation becomes recorded nodes below it
	void publish(bool finished);

	//engine thread only
	std::vector<SearchTreeNode> nodes;
	std::vector<Frame> frames;	//frames[0] is the root, frames[top] the node being searched; never shrinks, so no copies
	int top = -1;
	BoardState root;
	int depth = 0;
	int quiescenceDepth = 0;
	int deepestPly = 0;
	std::uint64_t totalNodes = 0;
	bool truncated = false;
	Move bestMove = 0;
	EvalValue bestScore = 0;
	Move bestMoveSoFar = 0;
	EvalValue bestScoreSoFar = 0;
	NodeId origin = ROOT_NODE;
	unsigned tag = 0;
	std::vector<Move> rootMoves;
	std::chrono::steady_clock::time_point startTime;
	std::uint64_t searchId = 0;
	std::uint64_t generation = 0;

	std::atomic<bool> snapshotRequested{false};
	mutable std::mutex snapshotMutex;
	std::shared_ptr<const SearchSnapshot> snapshot;
};

#endif
