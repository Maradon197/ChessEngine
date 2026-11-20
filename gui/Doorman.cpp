#include "Doorman.hpp"

#include <unordered_map>
#include <unordered_set>

#include <QColor>
#include <QGuiApplication>
#include <QStyleHints>

#include "PrincipleRegistry.hpp"
#include "BoardWidget.hpp"
#include "EvalCriteriaWidget.hpp"
#include "EvalSquareWidget.hpp"
#include "MoveTreeOverlay.hpp"
#include "PlayView.hpp"
#include "PrincipleDrawer.hpp"
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
	runInGuiThread([this, bottomColor] {
		if (board) board->setOrientation(bottomColor);
		if (evalSquare) evalSquare->setOrientation(bottomColor);
	});
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

void Doorman::setGameStatus(GameStatus status) {
	bool wasOver;
	{
		std::lock_guard lock(stateMutex);
		wasOver = gameStatus.result != GameResult::NONE;
		gameStatus = status;
	}
	const bool isOver = status.result != GameResult::NONE;
	runInGuiThread([this, status, wasOver, isOver] {
		if (board) board->setInteractive(!isOver);
		if (!evalCriteria) return;
		if (status.reason == GameEndingReason::CHECKMATE) evalCriteria->setOverallMate(0);
		else if (wasOver) evalCriteria->clearOverall();	//e.g. stepped back in the tree from a finished game
	});
}

GameStatus Doorman::getGameStatus() const {
	std::lock_guard lock(stateMutex);
	return gameStatus;
}

void Doorman::setCriterion(EvalCriterion criterion, int value) {
	runInGuiThread([this, criterion, value] { if (evalCriteria) evalCriteria->setCriterion(criterion, value); });
}

void Doorman::clearCriteria() {
	runInGuiThread([this] { if (evalCriteria) evalCriteria->clearCriteria(); });
}

void Doorman::setSquareValues(const SquareValues& values, float balance) {
	runInGuiThread([this, values, balance] { if (evalSquare) evalSquare->setValues(values, balance); });
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

void Doorman::setPrinciples(const std::vector<Principle>& newPrinciples) {
	std::vector<Principle> accepted;
	{
		std::lock_guard lock(stateMutex);
		std::unordered_map<std::string, const Principle*> known;
		for (const Principle& principle : principles) {
			known.emplace(principle.title, &principle);
		}
		std::unordered_set<std::string> titles;
		for (Principle principle : newPrinciples) {
			if (principle.title.empty() || !titles.insert(principle.title).second) {
				qWarning("Doorman::setPrinciples: skipped \"%s\", its title is empty or taken", principle.title.c_str());
				continue;
			}
			if (const auto found = known.find(principle.title); found != known.end()) {
				principle.enabled = found->second->enabled;
				principle.useful = found->second->useful;
			}
			accepted.push_back(std::move(principle));
		}
		principles = accepted;
	}
	runInGuiThread([this, accepted] { if (principleDrawer) principleDrawer->setPrinciples(accepted); });
}

void Doorman::setPrincipleEnabled(const std::string& title, bool enabled) {
	storePrincipleFlag(title, &Principle::enabled, enabled);
	runInGuiThread([this, title, enabled] { if (principleDrawer) principleDrawer->setPrincipleEnabled(title, enabled); });
}

void Doorman::setPrincipleUseful(const std::string& title, bool useful) {
	storePrincipleFlag(title, &Principle::useful, useful);
	runInGuiThread([this, title, useful] { if (principleDrawer) principleDrawer->setPrincipleUseful(title, useful); });
}

void Doorman::loadPrinciplesFromEngine() {
	std::vector<Principle> list;
	list.reserve(PRINCIPLES.size());

	for (const PrincipleInfo& info : PRINCIPLES) {
		Principle principle;
		principle.id = info.id;
		principle.title = std::string(info.title);
		principle.imagePath = std::string(info.imagePath);
		principle.spotElo = info.spotElo;
		principle.description = std::string(info.description);
		principle.explanation = std::string(info.explanation);
		principle.gamePart = info.gamePart;
		principle.focus = info.focus;
		list.push_back(principle);
	}

	setPrinciples(list);
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

std::vector<Principle> Doorman::getPrinciples() const {
	std::lock_guard lock(stateMutex);
	return principles;
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

void Doorman::attach(MoveTreeOverlay& moveTreeOverlay) {
	moveTree = &moveTreeOverlay;
	applyColors();
	connect(&moveTreeOverlay, &MoveTreeOverlay::nodeSelected, this, [this](NodeId id) {
		showTreeNode(id);
		emit treeNodeSelected(id, getPosition());
	});
}

void Doorman::attach(EvalSquareWidget& evalSquareWidget) {
	evalSquare = &evalSquareWidget;
	if (board) evalSquareWidget.setOrientation(board->getOrientation());
	applyColors();
}

void Doorman::attach(EvalCriteriaWidget& evalCriteriaWidget) {
	evalCriteria = &evalCriteriaWidget;
	applyColors();
}

void Doorman::attach(PrincipleDrawer& principleDrawerWidget) {
	principleDrawer = &principleDrawerWidget;
	applyColors();
	connect(&principleDrawerWidget, &PrincipleDrawer::principleEnabledChanged, this, [this](const std::string& title, bool enabled) {
		storePrincipleFlag(title, &Principle::enabled, enabled);
		emit principleEnabledChanged(title, enabled);
	});
	principleDrawerWidget.setPrinciples(getPrinciples());
}

void Doorman::attach(PlayView& playView) {
	connect(&playView, &PlayView::engineMoveRequested, this, &Doorman::engineMoveRequested);
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

void Doorman::storePrincipleFlag(const std::string& title, bool Principle::* flag, bool value) {
	std::lock_guard lock(stateMutex);
	for (Principle& principle : principles) {
		if (principle.title == title) {
			principle.*flag = value;
		}
	}
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
			case ColorRole::PRINCIPLES_PANEL:
				if (principleDrawer) principleDrawer->setPanelColor(color);
				break;
			default:
				if (board) board->setColor(role, color);
				break;
		}
	}
}
