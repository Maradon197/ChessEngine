#include "Doorman.hpp"

#include <QColor>
#include <QGuiApplication>
#include <QStyleHints>

#include "BoardWidget.hpp"
#include "EvalCriteriaWidget.hpp"
#include "EvalSquareWidget.hpp"
#include "MoveTreeWidget.hpp"
#include "Theme.hpp"

namespace {

//fallback when the core passes no SAN, e.g. "e2e4"
QString makeCoordinateLabel(Move move) {
	const auto squareName = [](Square square) {
		return QString(QChar('a' + moveBits::getFile(square))) + QChar('1' + moveBits::getRank(square));
	};
	QString label = squareName(moveBits::getSource(move)) + squareName(moveBits::getDestination(move));
	if (moveBits::isPromotion(move)) {
		const PieceType type = moveBits::getPromotionType(move);
		if (type > PAWN && type < KING) {
			label += QChar("nbrq"[type - KNIGHT]);
		}
	}
	return label;
}

} // !namespace

Doorman::Doorman(QObject* parent)
	: QObject(parent) {
	currentPosition.ep_square = NO_SQUARE;
	connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
		if (getThemeMode() == ThemeMode::SYSTEM) {
			applyTheme();
		}
	});
}

Doorman::~Doorman() = default;

void Doorman::setPosition(const Position& position) {
	runInGuiThread([this, position] {
		if (board) board->setPosition(position);
		if (moveTree) moveTree->setNodePosition(moveTree->getCurrentNode(), position);
		rememberBoardState();
	});
}

void Doorman::playMove(Move move, const std::string& label) {
	const QString qLabel = QString::fromStdString(label);
	runInGuiThread([this, move, qLabel] {
		if (!board) {
			return;
		}
		const std::optional<NodeId> existing = moveTree ? moveTree->findChild(moveTree->getCurrentNode(), move) : std::nullopt;
		if (existing) {
			moveTree->setCurrentNode(*existing);
			showTreeNode(*existing);
			return;
		}
		board->playMove(move);
		if (moveTree) {
			const NodeId id = nextNodeId.fetch_add(1);
			moveTree->addNode(id, moveTree->getCurrentNode(), move, qLabel.isEmpty() ? makeCoordinateLabel(move) : qLabel, board->getPosition());
			moveTree->setCurrentNode(id);
		}
		rememberBoardState();
	});
}

void Doorman::setLegalMoves(const MoveList& legalMoves) {
	runInGuiThread([this, legalMoves] { if (board) board->setLegalMoves(legalMoves); });
}

void Doorman::clearLegalMoves() {
	runInGuiThread([this] { if (board) board->clearLegalMoves(); });
}

void Doorman::setLastMove(Move move) {
	runInGuiThread([this, move] { if (board) board->setLastMove(move); });
}

void Doorman::clearLastMove() {
	runInGuiThread([this] { if (board) board->clearLastMove(); });
}

void Doorman::setBoardOrientation(Color bottomColor) {
	runInGuiThread([this, bottomColor] { if (board) board->setOrientation(bottomColor); });
}

void Doorman::setBoardInteractive(bool interactive) {
	runInGuiThread([this, interactive] { if (board) board->setInteractive(interactive); });
}

void Doorman::setUserHighlights(const std::vector<Highlight>& highlights) {
	{
		std::lock_guard lock(stateMutex);
		userHighlights = highlights;
	}
	runInGuiThread([this, highlights] { if (board) board->setUserHighlights(highlights); });
}

void Doorman::setUserArrows(const std::vector<Arrow>& arrows) {
	{
		std::lock_guard lock(stateMutex);
		userArrows = arrows;
	}
	runInGuiThread([this, arrows] { if (board) board->setUserArrows(arrows); });
}

void Doorman::clearUserAnnotations() {
	{
		std::lock_guard lock(stateMutex);
		userHighlights.clear();
		userArrows.clear();
	}
	runInGuiThread([this] { if (board) board->clearUserAnnotations(); });
}

void Doorman::setEngineHighlights(const std::vector<Highlight>& highlights) {
	runInGuiThread([this, highlights] { if (board) board->setEngineHighlights(highlights); });
}

void Doorman::setEngineArrows(const std::vector<Arrow>& arrows) {
	runInGuiThread([this, arrows] { if (board) board->setEngineArrows(arrows); });
}

void Doorman::clearEngineAnnotations() {
	runInGuiThread([this] { if (board) board->clearEngineAnnotations(); });
}

void Doorman::setOverallCentipawns(int centipawns) {
	runInGuiThread([this, centipawns] { if (evalCriteria) evalCriteria->setOverallCentipawns(centipawns); });
}

void Doorman::setOverallMate(int movesToMate) {
	runInGuiThread([this, movesToMate] { if (evalCriteria) evalCriteria->setOverallMate(movesToMate); });
}

void Doorman::clearOverallEval() {
	runInGuiThread([this] { if (evalCriteria) evalCriteria->clearOverall(); });
}

void Doorman::setCriterion(EvalCriterion criterion, int value) {
	runInGuiThread([this, criterion, value] { if (evalCriteria) evalCriteria->setCriterion(criterion, value); });
}

void Doorman::clearCriteria() {
	runInGuiThread([this] { if (evalCriteria) evalCriteria->clearCriteria(); });
}

void Doorman::setTreeOrigin(Clock fullmoveNumber, Color sideToMove) {
	runInGuiThread([this, fullmoveNumber, sideToMove] { if (moveTree) moveTree->setOrigin(fullmoveNumber, sideToMove); });
}

NodeId Doorman::addTreeNode(NodeId parent, Move move, const Position& position, const std::string& label) {
	const NodeId id = nextNodeId.fetch_add(1);
	const QString qLabel = label.empty() ? makeCoordinateLabel(move) : QString::fromStdString(label);
	runInGuiThread([this, id, parent, move, position, qLabel] {
		if (moveTree) moveTree->addNode(id, parent, move, qLabel, position);
	});
	return id;
}

void Doorman::removeTreeNode(NodeId id) {
	runInGuiThread([this, id] {
		if (!moveTree) {
			return;
		}
		const NodeId before = moveTree->getCurrentNode();
		moveTree->removeNode(id);
		if (moveTree->getCurrentNode() != before) {
			showTreeNode(moveTree->getCurrentNode());	//current node was removed, parent is shown
		}
	});
}

void Doorman::clearTree() {
	runInGuiThread([this] {
		if (moveTree) {
			moveTree->clearTree();
			if (board) moveTree->setNodePosition(ROOT_NODE, board->getPosition());
		}
		if (board) board->clearLastMove();
		rememberBoardState();
	});
}

void Doorman::setCurrentTreeNode(NodeId id) {
	runInGuiThread([this, id] {
		if (moveTree && moveTree->hasNode(id)) {
			moveTree->setCurrentNode(id);
			showTreeNode(id);
		}
	});
}

void Doorman::setThemeMode(ThemeMode mode) {
	runInGuiThread([this, mode] {
		{
			std::lock_guard lock(stateMutex);
			themeMode = mode;
		}
		applyTheme();
	});
}

void Doorman::setColor(ColorRole role, ColorValue color) {
	const auto index = static_cast<std::size_t>(role);
	if (index >= COLOR_ROLE_COUNT) {
		return;
	}
	runInGuiThread([this, index, color] {
		{
			std::lock_guard lock(stateMutex);
			customColors[index] = color;
		}
		applyColors();
	});
}

void Doorman::resetColors() {
	runInGuiThread([this] {
		{
			std::lock_guard lock(stateMutex);
			customColors.fill(std::nullopt);
		}
		applyColors();
	});
}

std::vector<Move> Doorman::getMovesToTreeNode(NodeId id) const {
	return moveTree ? moveTree->getMovesToNode(id) : std::vector<Move>{};
}

Position Doorman::getPosition() const {
	std::lock_guard lock(stateMutex);
	return currentPosition;
}

NodeId Doorman::getCurrentTreeNode() const {
	std::lock_guard lock(stateMutex);
	return currentNode;
}

ThemeMode Doorman::getThemeMode() const {
	std::lock_guard lock(stateMutex);
	return themeMode;
}

ColorValue Doorman::getColor(ColorRole role) const {
	std::lock_guard lock(stateMutex);
	const auto index = static_cast<std::size_t>(role);
	if (index < COLOR_ROLE_COUNT && customColors[index]) {
		return *customColors[index];
	}
	return theme::getDefaultColor(role, darkTheme);
}

std::vector<Highlight> Doorman::getUserHighlights() const {
	std::lock_guard lock(stateMutex);
	return userHighlights;
}

std::vector<Arrow> Doorman::getUserArrows() const {
	std::lock_guard lock(stateMutex);
	return userArrows;
}

void Doorman::attach(BoardWidget& boardWidget) {
	board = &boardWidget;
	applyColors();
	connect(&boardWidget, &BoardWidget::moveAttempted, this, &Doorman::moveAttempted);
	connect(&boardWidget, &BoardWidget::squareSelected, this, &Doorman::squareSelected);
	connect(&boardWidget, &BoardWidget::selectionCleared, this, &Doorman::selectionCleared);
	connect(&boardWidget, &BoardWidget::userAnnotationsChanged, this,
		[this](const std::vector<Highlight>& highlights, const std::vector<Arrow>& arrows) {
			{
				std::lock_guard lock(stateMutex);
				userHighlights = highlights;
				userArrows = arrows;
			}
			emit userAnnotationsChanged(highlights, arrows);
		});
}

void Doorman::attach(MoveTreeWidget& moveTreeWidget) {
	moveTree = &moveTreeWidget;
	applyColors();
	connect(&moveTreeWidget, &MoveTreeWidget::nodeSelected, this, [this](NodeId id) {
		showTreeNode(id);
		emit treeNodeSelected(id, getPosition());
	});
}

void Doorman::attach(EvalSquareWidget& evalSquareWidget) {
	evalSquare = &evalSquareWidget;
	applyColors();
}

void Doorman::attach(EvalCriteriaWidget& evalCriteriaWidget) {
	evalCriteria = &evalCriteriaWidget;
	applyColors();
}

void Doorman::showTreeNode(NodeId id) {
	if (board && moveTree) {
		board->setPosition(moveTree->getNodePosition(id));
		if (id == ROOT_NODE) {
			board->clearLastMove();
		} else {
			board->setLastMove(moveTree->getNodeMove(id));
		}
	}
	rememberBoardState();
}

void Doorman::rememberBoardState() {
	std::lock_guard lock(stateMutex);
	if (board) currentPosition = board->getPosition();
	if (moveTree) currentNode = moveTree->getCurrentNode();
}

void Doorman::applyTheme() {
	const ThemeMode mode = getThemeMode();
	theme::applyMode(mode);
	{
		std::lock_guard lock(stateMutex);
		darkTheme = theme::isDark(mode);
	}
	applyColors();
}

void Doorman::applyColors() {
	for (std::size_t index = 0; index < COLOR_ROLE_COUNT; ++index) {
		const auto role = static_cast<ColorRole>(index);
		const QColor color = QColor::fromRgba(getColor(role));
		switch (role) {
			case ColorRole::EVAL_PANEL:
				if (evalSquare) evalSquare->setPanelColor(color);
				if (evalCriteria) evalCriteria->setPanelColor(color);
				break;
			case ColorRole::MOVES_PANEL:
				if (moveTree) moveTree->setPanelColor(color);
				break;
			default:
				if (board) board->setColor(role, color);
				break;
		}
	}
}
