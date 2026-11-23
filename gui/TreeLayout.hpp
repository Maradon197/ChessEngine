#ifndef TREELAYOUT_HPP
#define TREELAYOUT_HPP

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

//the line layout of the move tree, shared with the search window's tree:
//every node sits in the column of its depth; a node's first child continues its row, every further child starts
//a new line on the nearest row that is free for the whole length of that line, moving away from the main line (row 0).
//children of the main line alternate below / above it, nested variations keep moving away in their direction
namespace treeLayout {

struct Bounds {
	int minRow = 0;
	int maxRow = 0;
	int maxColumn = 0;
};

//which columns are taken on which row; rows are kept in two vectors (row >= 0 and row < 0) so a lookup is O(1),
//the search tree walks over a lot of rows
class RowOccupancy {
public:
	bool isFree(int row, int firstColumn, int lastColumn) const {
		const std::vector<std::pair<int, int>>* taken = find(row);
		if (taken == nullptr) {
			return true;
		}
		for (const auto& [first, last] : *taken) {
			if (firstColumn <= last && first <= lastColumn) {
				return false;
			}
		}
		return true;
	}

	void occupy(int row, int firstColumn, int lastColumn) {
		std::vector<std::vector<std::pair<int, int>>>& side = row >= 0 ? below : above;
		const std::size_t index = row >= 0 ? static_cast<std::size_t>(row) : static_cast<std::size_t>(-row - 1);
		if (side.size() <= index) {
			side.resize(index + 1);
		}
		side[index].emplace_back(firstColumn, lastColumn);
	}

private:
	const std::vector<std::pair<int, int>>* find(int row) const {
		const std::vector<std::vector<std::pair<int, int>>>& side = row >= 0 ? below : above;
		const std::size_t index = row >= 0 ? static_cast<std::size_t>(row) : static_cast<std::size_t>(-row - 1);
		return index < side.size() ? &side[index] : nullptr;
	}

	std::vector<std::vector<std::pair<int, int>>> below;	//row 0, 1, 2, ...
	std::vector<std::vector<std::pair<int, int>>> above;	//row -1, -2, ...
};

//	getChildren(id) -> const std::vector<Id>&	in display order, the first one continues the line
//	getColumn(id) -> int						usually the depth
//	setRow(id, row)								receives the result
template <typename Id, typename GetChildren, typename GetColumn, typename SetRow>
class Layouter {
public:
	Layouter(const GetChildren& getChildren, const GetColumn& getColumn, const SetRow& setRow)
		: getChildren(getChildren), getColumn(getColumn), setRow(setRow) {}

	Bounds run(Id root) {
		bounds = {};
		occupancy = {};
		layoutLine(root, 0);
		return bounds;
	}

private:
	void layoutLine(Id first, int row) {
		std::vector<Id> line;
		for (Id id = first;;) {
			setRow(id, row);
			line.push_back(id);
			const std::vector<Id>& children = getChildren(id);
			if (children.empty()) {
				break;
			}
			id = children.front();
		}

		const int firstColumn = getColumn(first);
		const int lastColumn = getColumn(line.back());
		occupancy.occupy(row, firstColumn, lastColumn);
		bounds.minRow = std::min(bounds.minRow, row);
		bounds.maxRow = std::max(bounds.maxRow, row);
		bounds.maxColumn = std::max(bounds.maxColumn, lastColumn);

		for (Id id : line) {
			const std::vector<Id>& children = getChildren(id);
			for (std::size_t index = 1; index < children.size(); ++index) {
				const Id child = children[index];
				const int direction = row != 0 ? (row > 0 ? 1 : -1) : (index % 2 == 1 ? 1 : -1);	//nested variations move away from the main line
				const int variationFirst = getColumn(child);
				const int variationLast = variationFirst + getLineLength(child) - 1;

				int targetRow = row + direction;
				while (!occupancy.isFree(targetRow, variationFirst, variationLast)) {
					targetRow += direction;
				}
				layoutLine(child, targetRow);
			}
		}
	}

	int getLineLength(Id first) const {
		int length = 1;
		for (Id id = first; !getChildren(id).empty(); id = getChildren(id).front()) {
			++length;
		}
		return length;
	}

	const GetChildren& getChildren;
	const GetColumn& getColumn;
	const SetRow& setRow;
	RowOccupancy occupancy;
	Bounds bounds;
};

template <typename Id, typename GetChildren, typename GetColumn, typename SetRow>
Bounds layoutRows(Id root, const GetChildren& getChildren, const GetColumn& getColumn, const SetRow& setRow) {
	return Layouter<Id, GetChildren, GetColumn, SetRow>(getChildren, getColumn, setRow).run(root);
}

} // !namespace treeLayout

#endif
