#include "SearchTreeModel.hpp"

#include <algorithm>
#include <cmath>

#include <QObject>

#include "LegalMoveGenerator.hpp"
#include "MoveNotation.hpp"
#include "Searcher.hpp"

namespace {

constexpr float MATE_THRESHOLD = 900.0f;	//|score| above this is a mate, see SearchLimits::MATE

} // !namespace

SearchTreeModel::SearchTreeModel(std::shared_ptr<const SearchSnapshot> newSnapshot, const SearchTreeModel* previous)
	: snapshot(std::move(newSnapshot)) {
	const std::size_t count = snapshot->nodes.size();
	children.resize(count);
	kinds.resize(count, SearchNodeKind::ROOT);
	bestChildren.resize(count, -1);
	onBestLine.resize(count, false);
	onCurrentPath.resize(count, false);

	if (previous != nullptr && previous->snapshot->searchId == snapshot->searchId) {
		moveNames = previous->moveNames;
		legalMoveCounts = previous->legalMoveCounts;
	}

	//children in search order: parents always come before their children
	for (std::size_t index = 1; index < count; ++index) {
		const SearchTreeNode& node = snapshot->nodes[index];
		children[static_cast<std::size_t>(node.parent)].push_back(static_cast<int>(index));
		maxSubtree = std::max(maxSubtree, node.subtreeNodes);

		//the node got the window (-parent beta, -parent alpha), its score is from the parent's view
		if (!node.finished) {
			kinds[index] = SearchNodeKind::SEARCHING;
		} else if (node.score >= -node.alpha) {
			kinds[index] = SearchNodeKind::CUTOFF;
		} else if (node.score > -node.beta) {
			kinds[index] = SearchNodeKind::BEST;
		} else {
			kinds[index] = SearchNodeKind::FAILED_LOW;
		}
	}

	//a node prefers the last move that raised its alpha (or cut it off)
	for (std::size_t index = 0; index < count; ++index) {
		for (int child : children[index]) {
			const SearchNodeKind kind = kinds[static_cast<std::size_t>(child)];
			if (kind == SearchNodeKind::BEST || kind == SearchNodeKind::CUTOFF) {
				bestChildren[index] = child;
			}
		}
	}
	if (count > 0) {
		onBestLine[0] = true;
		for (int index = bestChildren[0]; index >= 0; index = bestChildren[static_cast<std::size_t>(index)]) {
			bestLine.push_back(index);
			onBestLine[static_cast<std::size_t>(index)] = true;
		}
	}
	for (std::int32_t index : snapshot->currentPath) {
		onCurrentPath[static_cast<std::size_t>(index)] = true;
	}

	//every legal root move, the searcher goes through them in the generator's order
	if (count > 0) {
		BoardState board = snapshot->root;
		const Position position = board.getPosition();	//copy: the generator makes and unmakes moves on board
		const MoveList legal = LegalMoveGenerator{}.generateLegalMovesInPosition(position, board);
		const std::vector<int>& searched = children[0];
		for (int child : searched) {
			rootMoves.push_back({snapshot->nodes[static_cast<std::size_t>(child)].move, child});
		}
		for (std::size_t index = 0; index < legal.count; ++index) {
			const Move move = legal.moves[index];
			const bool known = std::any_of(rootMoves.begin(), rootMoves.end(), [move](const RootMove& root) { return root.move == move; });
			if (!known) {
				rootMoves.push_back({move, -1});
			}
		}
	}
}

bool SearchTreeModel::hasAllChildrenRecorded(int index) const {
	return getChildren(index).size() == getNode(index).searchedChildren;
}

bool SearchTreeModel::isCutShort(int index) const {
	const std::vector<int>& list = getChildren(index);
	return getNode(index).finished && !list.empty() && hasAllChildrenRecorded(index)
		&& getKind(list.back()) == SearchNodeKind::CUTOFF;
}

int SearchTreeModel::getSkippedMoves(int index) const {
	if (!isCutShort(index)) {
		return -1;
	}
	return std::max(0, getLegalMoveCount(index) - static_cast<int>(getNode(index).searchedChildren));
}

bool SearchTreeModel::isLeaf(int index) const {
	return getNode(index).ply >= snapshot->depth;
}

bool SearchTreeModel::isTerminal(int index) const {
	const SearchTreeNode& node = getNode(index);
	return node.finished && node.ply < snapshot->depth && node.searchedChildren == 0;
}

int SearchTreeModel::getBestChild(int index) const {
	return bestChildren[static_cast<std::size_t>(index)];
}

Color SearchTreeModel::getMover(int index) const {
	const Color rootSide = snapshot->root.getPosition().side_to_move;
	const int ply = getNode(index).ply;
	if (ply == 0) {
		return rootSide;	//the root's score is from the view of the side to move
	}
	return ply % 2 == 1 ? rootSide : !rootSide;
}

float SearchTreeModel::getWhiteScore(int index) const {
	const SearchTreeNode& node = getNode(index);
	if (!node.finished) {
		return 0;
	}
	return getMover(index) == WHITE ? node.score : -node.score;
}

QString SearchTreeModel::formatScore(float whiteScore) {
	if (std::abs(whiteScore) > -SearchLimits::MATE) {
		return QStringLiteral("—");	//one of the infinities
	}
	if (std::abs(whiteScore) >= MATE_THRESHOLD) {
		const int plies = static_cast<int>(std::lround(-SearchLimits::MATE - std::abs(whiteScore)));
		return QString("%1#%2").arg(whiteScore > 0 ? "" : QString::fromUtf8("−")).arg((plies + 1) / 2);
	}
	const QString number = QString::number(std::abs(whiteScore), 'f', 2);
	if (whiteScore > 0.005f) return "+" + number;
	if (whiteScore < -0.005f) return QString::fromUtf8("−") + number;
	return QStringLiteral("0.00");
}

QString SearchTreeModel::getScoreText(int index) const {
	const SearchTreeNode& node = getNode(index);
	if (!node.finished) {
		return QString::fromUtf8("…");
	}
	const QString value = formatScore(getWhiteScore(index));
	//bounds: a cutoff is "at least" for the mover, a fail-low "at most"; from white's view that flips for black's moves
	const SearchNodeKind kind = getKind(index);
	if (kind != SearchNodeKind::CUTOFF && kind != SearchNodeKind::FAILED_LOW) {
		return value;
	}
	const bool lowerForMover = kind == SearchNodeKind::CUTOFF;
	const bool lowerForWhite = getMover(index) == WHITE ? lowerForMover : !lowerForMover;
	return QString::fromUtf8(lowerForWhite ? "≥ " : "≤ ") + value;
}

BoardState SearchTreeModel::getBoard(int index) const {
	std::vector<Move> moves;
	for (int current = index; current > 0; current = getNode(current).parent) {
		moves.push_back(getNode(current).move);
	}
	BoardState board = snapshot->root;
	for (auto move = moves.rbegin(); move != moves.rend(); ++move) {
		board.makeMove(*move);
	}
	return board;
}

QString SearchTreeModel::getMoveName(int index) const {
	if (index <= 0) {
		return QObject::tr("Root");
	}
	if (const auto found = moveNames.find(index); found != moveNames.end()) {
		return found->second;
	}
	const BoardState before = getBoard(getNode(index).parent);
	const QString name = QString::fromStdString(MoveNotation::buildMoveString(getNode(index).move, before.getPosition()));
	moveNames.emplace(index, name);
	return name;
}

QString SearchTreeModel::getLineText(int index, int maxMoves) const {
	std::vector<int> line;
	for (int current = index; current > 0; current = getNode(current).parent) {
		line.push_back(current);
	}
	std::reverse(line.begin(), line.end());

	const Position& root = snapshot->root.getPosition();
	int fullmove = root.fullmove_counter;
	Color side = root.side_to_move;
	QString text;
	int shown = 0;
	for (int node : line) {
		if (shown++ == maxMoves) {
			text += QString::fromUtf8(" …");
			break;
		}
		if (side == WHITE) {
			text += QString("%1%2. ").arg(text.isEmpty() ? "" : " ").arg(fullmove);
		} else if (text.isEmpty()) {
			text += QString::fromUtf8("%1… ").arg(fullmove);
		} else {
			text += " ";
		}
		text += getMoveName(node);
		if (side == BLACK) {
			++fullmove;
		}
		side = !side;
	}
	return text;
}

int SearchTreeModel::getLegalMoveCount(int index) const {
	if (const auto found = legalMoveCounts.find(index); found != legalMoveCounts.end()) {
		return found->second;
	}
	BoardState board = getBoard(index);
	const Position position = board.getPosition();	//copy: the generator makes and unmakes moves on board
	const int count = static_cast<int>(LegalMoveGenerator{}.generateLegalMovesInPosition(position, board).count);
	legalMoveCounts.emplace(index, count);
	return count;
}
