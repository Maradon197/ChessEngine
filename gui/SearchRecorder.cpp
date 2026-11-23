#include "SearchRecorder.hpp"

#include <algorithm>

#include "Searcher.hpp"

SearchRecorder::SearchRecorder() {
	nodes.reserve(NODE_BUDGET + 1);
	frames.resize(SearchLimits::MAX_DEPTH + 2);
}

void SearchRecorder::begin(const BoardState& rootBoard, int searchDepth) {
	nodes.clear();
	root = rootBoard;
	depth = searchDepth;
	deepestPly = 0;
	totalNodes = 1;
	truncated = false;
	bestMove = 0;
	bestScore = 0;
	startTime = std::chrono::steady_clock::now();
	++searchId;

	SearchTreeNode rootNode;
	rootNode.alpha = SearchLimits::MINUS_INFINITY;
	rootNode.beta = SearchLimits::PLUS_INFINITY;
	nodes.push_back(rootNode);
	top = 0;
	frames[0] = {};
	frames[0].node = 0;
	publish(false);	//the window shows the new search right away
}

void SearchRecorder::finish(const SearchResult& result) {
	bestMove = result.bestMove;
	bestScore = result.evaluation;
	if (top >= 0) {
		SearchTreeNode& rootNode = nodes.front();
		rootNode.score = result.evaluation;
		rootNode.subtreeNodes = frames[0].subtreeNodes;
		rootNode.searchedChildren = frames[0].searchedChildren;
		rootNode.finished = true;
		recordLine(frames[0]);
	}
	top = -1;
	publish(true);
}

void SearchRecorder::enterNode(Move move, const SearchInfo& childInfo) {
	++totalNodes;
	Frame& parent = frames[static_cast<std::size_t>(top)];
	++parent.searchedChildren;
	const int ply = top + 1;
	deepestPly = std::max(deepestPly, ply);

	std::int32_t index = -1;
	if (parent.node >= 0) {
		if (ply <= ALWAYS_RECORDED_PLY || nodes.size() < NODE_BUDGET) {
			SearchTreeNode node;
			node.parent = parent.node;
			node.move = move;
			node.alpha = childInfo.minGuaranteedScore;
			node.beta = childInfo.mostAllowedScore;
			node.ply = static_cast<std::uint8_t>(std::min(ply, 255));
			nodes.push_back(node);
			index = static_cast<std::int32_t>(nodes.size() - 1);
		} else {
			truncated = true;
		}
	}

	Frame& frame = frames[static_cast<std::size_t>(++top)];
	frame.node = index;
	frame.move = move;
	frame.alpha = childInfo.minGuaranteedScore;
	frame.beta = childInfo.mostAllowedScore;
	frame.subtreeNodes = 1;
	frame.searchedChildren = 0;
	frame.lineLength = 0;

	if (snapshotRequested.load(std::memory_order_relaxed)) {
		publish(false);
	}
}

void SearchRecorder::leaveNode(EvalValue score) {
	Frame& frame = frames[static_cast<std::size_t>(top)];
	Frame& parent = frames[static_cast<std::size_t>(top - 1)];

	SearchTreeNode result;
	result.move = frame.move;
	result.alpha = frame.alpha;
	result.beta = frame.beta;
	result.score = score;
	result.subtreeNodes = frame.subtreeNodes;
	result.searchedChildren = frame.searchedChildren;
	result.ply = static_cast<std::uint8_t>(std::min(top, 255));
	result.finished = true;

	if (frame.node >= 0) {
		result.parent = nodes[static_cast<std::size_t>(frame.node)].parent;
		nodes[static_cast<std::size_t>(frame.node)] = result;
		recordLine(frame);
	}

	//better than the parent's alpha (-beta from here): the parent's best continuation goes through this move now
	if (score > -frame.beta) {
		const int length = std::min(frame.lineLength, MAX_LINE - 1);
		parent.line[0] = {result, frame.node};
		std::copy_n(frame.line.begin(), length, parent.line.begin() + 1);
		parent.lineLength = length + 1;
	}

	parent.subtreeNodes += frame.subtreeNodes;
	--top;
}

void SearchRecorder::recordLine(Frame& frame) {
	std::int32_t parent = frame.node;
	for (int index = 0; index < frame.lineLength; ++index) {
		LineEntry& entry = frame.line[static_cast<std::size_t>(index)];
		if (entry.recorded < 0) {
			entry.node.parent = parent;
			nodes.push_back(entry.node);
			entry.recorded = static_cast<std::int32_t>(nodes.size() - 1);
		}
		parent = entry.recorded;
	}
}

void SearchRecorder::requestSnapshot() {
	snapshotRequested.store(true, std::memory_order_relaxed);
}

std::shared_ptr<const SearchSnapshot> SearchRecorder::takeSnapshot(std::uint64_t newerThan) const {
	const std::lock_guard lock(snapshotMutex);
	return snapshot && snapshot->generation > newerThan ? snapshot : nullptr;
}

//engine thread: copies the recording, the nodes still being searched get their counts so far
void SearchRecorder::publish(bool finished) {
	snapshotRequested.store(false, std::memory_order_relaxed);

	auto copy = std::make_shared<SearchSnapshot>();
	copy->searchId = searchId;
	copy->generation = ++generation;
	copy->root = root;
	copy->depth = depth;
	copy->nodes = nodes;
	for (int ply = 0; ply <= top; ++ply) {
		const Frame& frame = frames[static_cast<std::size_t>(ply)];
		if (frame.node < 0) {
			break;
		}
		SearchTreeNode& node = copy->nodes[static_cast<std::size_t>(frame.node)];
		node.subtreeNodes = frame.subtreeNodes;
		node.searchedChildren = frame.searchedChildren;
		copy->currentPath.push_back(frame.node);
	}
	copy->currentPly = std::max(top, 0);
	copy->deepestPly = deepestPly;
	copy->totalNodes = totalNodes;
	copy->seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
	copy->finished = finished;
	copy->truncated = truncated;
	copy->bestMove = bestMove;
	copy->bestScore = bestScore;

	const std::lock_guard lock(snapshotMutex);
	snapshot = std::move(copy);
}
