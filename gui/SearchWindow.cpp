#include "SearchWindow.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "GuiTypes.hpp"
#include "KeyBindings.hpp"
#include "MoveNotation.hpp"
#include "SearchRecorder.hpp"
#include "SearchTreeModel.hpp"
#include "SearchTreeView.hpp"

namespace {

const QColor CUTOFF_COLOR(245, 166, 35);
const QColor BEST_COLOR(80, 200, 140);
const QColor LINE_COLOR(66, 153, 255);
const QColor SEARCHING_COLOR(0, 210, 230);
const char* const GEOMETRY_KEY = "searchWindow/geometry";

QColor withAlpha(QColor color, int alpha) {
	color.setAlpha(alpha);
	return color;
}

QColor getShade(float whiteScore) {
	const int shade = static_cast<int>(std::lround(128 + 112 * std::tanh(whiteScore / 1.8f)));
	return QColor(shade, shade, shade);
}

QString formatCount(double count) {
	if (count >= 1e6) return QString::number(count / 1e6, 'f', 2) + "M";
	if (count >= 1e4) return QString::number(count / 1e3, 'f', 0) + "k";
	return QString::number(static_cast<qint64>(count));
}

qreal getPulse(const QElapsedTimer& clock) {
	return 0.5 + 0.5 * std::sin(clock.elapsed() / 180.0);
}

//the best root move so far and its score from white's view
int getBestRootNode(const SearchTreeModel& model) {
	return model.getBestLine().empty() ? -1 : model.getBestLine().front();
}

} // !namespace

//the numbers: status, depth, where it is, mode, nodes, speed, time, best move
class SearchStatsBar : public QWidget {
public:
	explicit SearchStatsBar(QWidget* parent) : QWidget(parent) {
		setFixedHeight(64);
		clock.start();
	}

	void setModel(std::shared_ptr<const SearchTreeModel> newModel) {
		model = std::move(newModel);
		update();
	}

	QElapsedTimer clock;

protected:
	void paintEvent(QPaintEvent*) override {
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		const QColor text = palette().color(QPalette::Text);

		struct Tile {
			QString value;
			QString caption;
			QColor accent;
		};
		std::vector<Tile> tiles;
		if (!model) {
			tiles.push_back({tr("Waiting"), tr("no search yet"), withAlpha(text, 120)});
		} else {
			const SearchSnapshot& snapshot = model->getSnapshot();
			const bool done = snapshot.finished;
			tiles.push_back({done ? tr("Done") : tr("Thinking"), done ? tr("move chosen") : tr("live, %1× per second").arg(1000 / SearchWindow::UPDATE_INTERVAL_MS),
				done ? BEST_COLOR : SEARCHING_COLOR});
			tiles.push_back({QString::number(snapshot.depth), tr("plies deep (max)"), {}});
			tiles.push_back({done ? QString::fromUtf8("–") : tr("ply %1").arg(snapshot.currentPly),
				tr("now · deepest %1").arg(snapshot.deepestPly), done ? QColor() : SEARCHING_COLOR});
			tiles.push_back({tr("Max strength"), tr("alpha-beta · fixed depth"), {}});
			tiles.push_back({formatCount(static_cast<double>(snapshot.totalNodes)),
				snapshot.truncated ? tr("nodes · %1 recorded").arg(formatCount(static_cast<double>(snapshot.nodes.size()))) : tr("nodes"), {}});
			tiles.push_back({formatCount(snapshot.seconds > 0.01 ? snapshot.totalNodes / snapshot.seconds : 0.0) + tr("/s"), tr("nodes per second"), {}});
			tiles.push_back({QString::number(snapshot.seconds, 'f', snapshot.seconds < 10 ? 2 : 1) + " s", tr("thinking time"), {}});
			const int best = getBestRootNode(*model);
			if (best >= 0) {
				const float score = done ? (snapshot.root.getPosition().side_to_move == WHITE ? snapshot.bestScore : -snapshot.bestScore)
					: model->getWhiteScore(best);
				tiles.push_back({model->getMoveName(best), (done ? tr("plays · %1") : tr("best so far · %1")).arg(SearchTreeModel::formatScore(score)), LINE_COLOR});
			} else {
				tiles.push_back({QString::fromUtf8("–"), tr("best so far"), LINE_COLOR});
			}
		}

		const qreal gap = 8;
		const qreal width = (this->width() - gap * (static_cast<qreal>(tiles.size()) + 1)) / static_cast<qreal>(tiles.size());
		QFont valueFont = font();
		valueFont.setPointSizeF(valueFont.pointSizeF() * 1.35);
		valueFont.setBold(true);
		QFont captionFont = font();
		captionFont.setPointSizeF(captionFont.pointSizeF() * 0.82);
		const bool searching = model && !model->getSnapshot().finished;
		for (std::size_t index = 0; index < tiles.size(); ++index) {
			const Tile& tile = tiles[index];
			const QRectF box(gap + static_cast<qreal>(index) * (width + gap), 8, width, height() - 16);
			painter.setPen(QPen(tile.accent.isValid() ? withAlpha(tile.accent, 150) : withAlpha(text, 35), tile.accent.isValid() ? 1.5 : 1));
			painter.setBrush(palette().color(QPalette::Base));
			painter.drawRoundedRect(box, 7, 7);

			QRectF inner = box.adjusted(10, 4, -8, -4);
			if (index == 0) {	//status light
				const QPointF light(inner.left() + 5, inner.top() + inner.height() * 0.34);
				const qreal alpha = searching ? 120 + 135 * getPulse(clock) : 255;
				painter.setPen(Qt::NoPen);
				painter.setBrush(withAlpha(tile.accent, static_cast<int>(alpha)));
				painter.drawEllipse(light, 4.5, 4.5);
				inner.adjust(15, 0, 0, 0);
			}
			painter.setFont(valueFont);
			painter.setPen(tile.accent.isValid() && index != 2 ? tile.accent : text);
			const QString value = QFontMetricsF(valueFont).elidedText(tile.value, Qt::ElideRight, inner.width());
			painter.drawText(QRectF(inner.left(), inner.top(), inner.width(), inner.height() * 0.62), Qt::AlignLeft | Qt::AlignVCenter, value);
			painter.setFont(captionFont);
			painter.setPen(withAlpha(text, 140));
			const QString caption = QFontMetricsF(captionFont).elidedText(tile.caption, Qt::ElideRight, inner.width());
			painter.drawText(QRectF(inner.left(), inner.top() + inner.height() * 0.58, inner.width(), inner.height() * 0.42), Qt::AlignLeft | Qt::AlignVCenter, caption);
		}
	}

private:
	std::shared_ptr<const SearchTreeModel> model;
};

//every legal move at the root in the order the engine goes through them: searched ones filled with their score,
//the one being searched pulses, the best one is ringed; the bar underneath is how much was searched below it
class RootMovesStrip : public QWidget {
public:
	explicit RootMovesStrip(QWidget* parent) : QWidget(parent) {
		setFixedHeight(58);
		setMouseTracking(true);
		clock.start();
	}

	void setModel(std::shared_ptr<const SearchTreeModel> newModel) {
		if (!newModel || !model || newModel->getSnapshot().searchId != model->getSnapshot().searchId) {
			names.clear();
		}
		model = std::move(newModel);
		update();
	}

	std::function<void(int)> onNodeClicked;
	QElapsedTimer clock;

protected:
	void paintEvent(QPaintEvent*) override {
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		const QColor text = palette().color(QPalette::Text);
		if (!model || model->getRootMoves().empty()) {
			return;
		}
		const auto& moves = model->getRootMoves();
		const int searched = static_cast<int>(model->getChildren(0).size());

		QFont captionFont = font();
		captionFont.setPointSizeF(captionFont.pointSizeF() * 0.85);
		painter.setFont(captionFont);
		painter.setPen(withAlpha(text, 150));
		const QRectF captionRect(10, 0, CAPTION_WIDTH - 14, height());
		painter.drawText(captionRect, Qt::AlignVCenter | Qt::AlignLeft,
			tr("Root moves\n%1 / %2 searched").arg(searched).arg(moves.size()));

		const int best = getBestRootNode(*model);
		std::uint32_t maxEffort = 1;
		for (int child : model->getChildren(0)) {
			maxEffort = std::max(maxEffort, model->getNode(child).subtreeNodes);
		}
		const qreal pulse = getPulse(clock);
		QFont chipFont = font();
		chipFont.setPointSizeF(chipFont.pointSizeF() * 0.85);
		chipFont.setBold(true);
		painter.setFont(chipFont);

		for (std::size_t index = 0; index < moves.size(); ++index) {
			const QRectF chip = getChipRect(index);
			const int node = moves[index].node;
			const bool done = node >= 0 && model->getNode(node).finished;
			QColor fill = done ? getShade(model->getWhiteScore(node)) : palette().color(QPalette::Base);
			QPen pen(withAlpha(text, 70), 1, Qt::DashLine);
			if (node >= 0 && !done) {
				pen = QPen(withAlpha(SEARCHING_COLOR, static_cast<int>(130 + 125 * pulse)), 2.2);
			} else if (done) {
				const SearchNodeKind kind = model->getKind(node);
				pen = QPen(kind == SearchNodeKind::BEST ? BEST_COLOR : withAlpha(text, 70), kind == SearchNodeKind::BEST ? 1.6 : 1);
			}
			if (node == best) {
				pen = QPen(LINE_COLOR, 2.6);
				painter.setPen(Qt::NoPen);
				painter.setBrush(withAlpha(LINE_COLOR, 60));
				painter.drawRoundedRect(chip.adjusted(-3, -3, 3, 3), 7, 7);
			}
			if (node == hovered) {
				pen.setWidthF(pen.widthF() + 1);
			}
			painter.setPen(pen);
			painter.setBrush(fill);
			painter.drawRoundedRect(chip, 5, 5);
			const QColor ink = !done ? withAlpha(text, node >= 0 ? 230 : 120) : fill.lightness() > 140 ? QColor(25, 25, 28) : QColor(238, 238, 240);
			painter.setPen(ink);
			painter.drawText(chip, Qt::AlignCenter, QFontMetricsF(chipFont).elidedText(getName(index), Qt::ElideRight, chip.width() - 4));

			//effort below it
			if (node >= 0) {
				const qreal share = std::log(1.0 + model->getNode(node).subtreeNodes) / std::log(1.0 + maxEffort);
				const QRectF bar(chip.left() + 2, chip.bottom() + 4, (chip.width() - 4) * share, 3);
				painter.setPen(Qt::NoPen);
				painter.setBrush(node == best ? LINE_COLOR : withAlpha(text, 90));
				painter.drawRoundedRect(bar, 1.5, 1.5);
			}
		}
	}

	void mouseMoveEvent(QMouseEvent* event) override {
		const int node = findNode(event->position());
		if (node != hovered) {
			hovered = node;
			setCursor(node >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
			update();
		}
	}

	void leaveEvent(QEvent*) override {
		hovered = -1;
		update();
	}

	void mouseReleaseEvent(QMouseEvent* event) override {
		const int node = findNode(event->position());
		if (node >= 0 && onNodeClicked) {
			onNodeClicked(node);
		}
	}

private:
	static constexpr qreal CAPTION_WIDTH = 130;

	QRectF getChipRect(std::size_t index) const {
		const auto count = static_cast<qreal>(model->getRootMoves().size());
		const qreal space = width() - CAPTION_WIDTH - 10;
		const qreal step = std::min(72.0, space / count);
		const qreal chipWidth = std::max(6.0, step - 5);
		return {CAPTION_WIDTH + static_cast<qreal>(index) * step, 12, chipWidth, 26};
	}

	int findNode(QPointF point) const {
		if (!model) {
			return -1;
		}
		for (std::size_t index = 0; index < model->getRootMoves().size(); ++index) {
			if (getChipRect(index).adjusted(-2, -4, 2, 8).contains(point)) {
				return model->getRootMoves()[index].node;
			}
		}
		return -1;
	}

	QString getName(std::size_t index) {
		const auto& root = model->getRootMoves()[index];
		if (root.node >= 0) {
			return model->getMoveName(root.node);
		}
		if (index >= names.size()) {
			names.resize(model->getRootMoves().size());
		}
		if (names[index].isEmpty()) {
			names[index] = QString::fromStdString(MoveNotation::buildMoveString(root.move, model->getSnapshot().root.getPosition()));
		}
		return names[index];
	}

	std::shared_ptr<const SearchTreeModel> model;
	std::vector<QString> names;	//of moves not searched yet
	int hovered = -1;
};

//the line Rhea expects and its score, with a legend of the colors and outlines underneath
class BestLineBar : public QWidget {
public:
	explicit BestLineBar(QWidget* parent) : QWidget(parent) {
		setFixedHeight(62);
	}

	void setModel(std::shared_ptr<const SearchTreeModel> newModel) {
		model = std::move(newModel);
		update();
	}

protected:
	void paintEvent(QPaintEvent*) override {
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		const QColor text = palette().color(QPalette::Text);

		//line 1: score chip, best line
		const QRectF top(10, 4, width() - 20, 28);
		QString line = tr("No best line yet");
		float score = 0;
		bool haveScore = false;
		if (model && !model->getBestLine().empty()) {
			const SearchSnapshot& snapshot = model->getSnapshot();
			line = model->getLineText(model->getBestLine().back(), 24);
			if (model->isLeaf(model->getBestLine().back())) {
				line += tr("   → evaluated");
			}
			score = snapshot.finished ? (snapshot.root.getPosition().side_to_move == WHITE ? snapshot.bestScore : -snapshot.bestScore)
				: model->getWhiteScore(model->getBestLine().front());
			haveScore = true;
		}
		QFont scoreFont = font();
		scoreFont.setBold(true);
		scoreFont.setPointSizeF(scoreFont.pointSizeF() * 1.1);
		const QRectF chip(top.left(), top.top() + 2, 74, top.height() - 4);
		const QColor fill = haveScore ? getShade(score) : palette().color(QPalette::Base);
		painter.setPen(QPen(LINE_COLOR, 2));
		painter.setBrush(fill);
		painter.drawRoundedRect(chip, 6, 6);
		painter.setFont(scoreFont);
		painter.setPen(fill.lightness() > 140 ? QColor(25, 25, 28) : QColor(238, 238, 240));
		painter.drawText(chip, Qt::AlignCenter, haveScore ? SearchTreeModel::formatScore(score) : QString::fromUtf8("–"));

		QFont lineFont = font();
		lineFont.setPointSizeF(lineFont.pointSizeF() * 1.1);
		painter.setFont(lineFont);
		painter.setPen(text);
		const QRectF lineRect = top.adjusted(chip.width() + 12, 0, 0, 0);
		const QString caption = tr("Best line  ");
		QFont captionFont = lineFont;
		captionFont.setBold(true);
		painter.setFont(captionFont);
		painter.setPen(LINE_COLOR);
		painter.drawText(lineRect, Qt::AlignVCenter | Qt::AlignLeft, caption);
		const qreal captionWidth = QFontMetricsF(captionFont).horizontalAdvance(caption);
		painter.setFont(lineFont);
		painter.setPen(text);
		const QRectF rest = lineRect.adjusted(captionWidth, 0, 0, 0);
		painter.drawText(rest, Qt::AlignVCenter | Qt::AlignLeft, QFontMetricsF(lineFont).elidedText(line, Qt::ElideRight, rest.width()));

		//line 2: legend
		QFont legendFont = font();
		legendFont.setPointSizeF(legendFont.pointSizeF() * 0.82);
		painter.setFont(legendFont);
		const QFontMetricsF metrics(legendFont);
		qreal x = 10;
		const qreal y = 46;
		//black <-> white gradient
		QLinearGradient gradient(x, 0, x + 70, 0);
		gradient.setColorAt(0, getShade(-6));
		gradient.setColorAt(0.5, getShade(0));
		gradient.setColorAt(1, getShade(6));
		painter.setPen(QPen(withAlpha(text, 80), 1));
		painter.setBrush(gradient);
		painter.drawRoundedRect(QRectF(x, y - 6, 70, 12), 3, 3);
		x += 76;
		painter.setPen(withAlpha(text, 160));
		const QString evalText = tr("good for Black ↔ good for White");
		painter.drawText(QPointF(x, y + metrics.ascent() / 2 - 1), evalText);
		x += metrics.horizontalAdvance(evalText) + 22;

		const auto item = [&](const QPen& pen, const QString& label) {
			painter.setPen(pen);
			painter.setBrush(getShade(0));
			painter.drawRoundedRect(QRectF(x, y - 6, 20, 12), 3, 3);
			x += 25;
			painter.setPen(withAlpha(text, 160));
			painter.drawText(QPointF(x, y + metrics.ascent() / 2 - 1), label);
			x += metrics.horizontalAdvance(label) + 18;
		};
		item(QPen(LINE_COLOR, 2.4), tr("best line"));
		item(QPen(CUTOFF_COLOR, 2.4), tr("caused a beta cutoff"));
		item(QPen(CUTOFF_COLOR, 1.8, Qt::DashLine), tr("cut short (✂ moves skipped)"));
		item(QPen(BEST_COLOR, 1.8), tr("best so far (raised alpha)"));
		item(QPen(withAlpha(text, 90), 1), tr("not better (bound)"));
		item(QPen(SEARCHING_COLOR, 2.4), tr("searching"));
		painter.setPen(withAlpha(text, 120));
		painter.drawText(QPointF(x, y + metrics.ascent() / 2 - 1), tr("· thicker edge = more nodes below"));
	}

private:
	std::shared_ptr<const SearchTreeModel> model;
};

SearchWindow::SearchWindow(KeyBindings& keyBindings, QWidget& mainWindow)
	: QWidget(&mainWindow, Qt::Window)
	, keyBindings(keyBindings) {
	setWindowTitle(tr("Rhea is thinking"));
	resize(1280, 780);
	if (const QVariant geometry = QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).value(GEOMETRY_KEY); geometry.isValid()) {
		restoreGeometry(geometry.toByteArray());
	}

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 4, 6, 6);
	layout->setSpacing(2);

	stats = new SearchStatsBar(this);
	layout->addWidget(stats);
	rootMoves = new RootMovesStrip(this);
	layout->addWidget(rootMoves);
	treeView = new SearchTreeView(this);
	layout->addWidget(treeView, 1);

	auto* bottom = new QHBoxLayout;
	bottom->setSpacing(6);
	bestLine = new BestLineBar(this);
	bottom->addWidget(bestLine, 1);

	const auto makeButton = [this](const QString& text, const QString& tip) {
		auto* button = new QToolButton(this);
		button->setText(text);
		button->setToolTip(tip);
		button->setFocusPolicy(Qt::NoFocus);
		button->setCursor(Qt::PointingHandCursor);
		return button;
	};
	auto* controls = new QVBoxLayout;
	controls->setSpacing(2);
	auto* expandRow = new QHBoxLayout;
	expandRow->setSpacing(2);
	QToolButton* less = makeButton(QString::fromUtf8("−"), tr("Show fewer plies completely (-)"));
	expandLabel = new QLabel(this);
	expandLabel->setAlignment(Qt::AlignCenter);
	expandLabel->setMinimumWidth(118);
	QToolButton* more = makeButton("+", tr("Show more plies completely (+)"));
	expandRow->addWidget(less);
	expandRow->addWidget(expandLabel);
	expandRow->addWidget(more);
	controls->addLayout(expandRow);
	auto* viewRow = new QHBoxLayout;
	viewRow->setSpacing(2);
	QToolButton* fit = makeButton(tr("Fit"), tr("Show the whole tree and follow the search (F)"));
	followButton = makeButton(tr("Follow"), tr("Keep the whole tree in view while the engine searches"));
	followButton->setCheckable(true);
	viewRow->addWidget(fit);
	viewRow->addWidget(followButton);
	controls->addLayout(viewRow);
	bottom->addLayout(controls);
	layout->addLayout(bottom);

	connect(less, &QToolButton::clicked, this, [this] { treeView->setExpandPly(treeView->getExpandPly() - 1); });
	connect(more, &QToolButton::clicked, this, [this] { treeView->setExpandPly(treeView->getExpandPly() + 1); });
	connect(fit, &QToolButton::clicked, treeView, &SearchTreeView::fitToView);
	connect(followButton, &QToolButton::clicked, this, [this](bool checked) {
		if (checked) {
			treeView->fitToView();
		} else {
			updateControls();
		}
	});
	connect(treeView, &SearchTreeView::followingChanged, this, &SearchWindow::updateControls);
	connect(treeView, &SearchTreeView::displayChanged, this, &SearchWindow::updateControls);
	rootMoves->onNodeClicked = [this](int node) { treeView->centerOn(node); };

	pollTimer = new QTimer(this);
	pollTimer->setInterval(UPDATE_INTERVAL_MS);
	connect(pollTimer, &QTimer::timeout, this, &SearchWindow::poll);

	//the status light and the strip pulse while the engine searches
	auto* pulseTimer = new QTimer(this);
	pulseTimer->setInterval(40);
	connect(pulseTimer, &QTimer::timeout, this, [this] {
		if (isVisible() && model && !model->getSnapshot().finished) {
			stats->update();
			rootMoves->update();
		}
	});
	pulseTimer->start();

	updateControls();
}

void SearchWindow::setRecorder(SearchRecorder& newRecorder) {
	recorder = &newRecorder;
}

void SearchWindow::toggle() {
	if (isVisible()) {
		hide();
		return;
	}
	show();
	raise();
	activateWindow();
}

void SearchWindow::showEvent(QShowEvent* event) {
	QWidget::showEvent(event);
	poll();
	pollTimer->start();
	treeView->setFocus();
}

void SearchWindow::hideEvent(QHideEvent* event) {
	QWidget::hideEvent(event);
	pollTimer->stop();	//nobody asks for snapshots anymore, the search runs undisturbed
	QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(GEOMETRY_KEY, saveGeometry());
}

//W (or whatever opens it) and Escape close it again; the main window's key bindings don't reach this window
void SearchWindow::keyPressEvent(QKeyEvent* event) {
	const QKeyCombination key(event->modifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier), static_cast<Qt::Key>(event->key()));
	if (event->key() == Qt::Key_Escape || key == keyBindings.getKey(KeyAction::SEARCH_WINDOW)) {
		hide();
		return;
	}
	QWidget::keyPressEvent(event);
}

void SearchWindow::poll() {
	if (recorder == nullptr) {
		return;
	}
	if (std::shared_ptr<const SearchSnapshot> snapshot = recorder->takeSnapshot(generation)) {
		generation = snapshot->generation;
		model = std::make_shared<const SearchTreeModel>(std::move(snapshot), model.get());
		stats->setModel(model);
		rootMoves->setModel(model);
		treeView->setModel(model);
		bestLine->setModel(model);
	}
	recorder->requestSnapshot();	//answered at the engine's next node, if it is searching
}

void SearchWindow::updateControls() {
	const int expand = treeView->getExpandPly();
	const int shown = treeView->getShownPly();
	expandLabel->setText(shown < expand ? tr("all of %1 plies (max %2)").arg(shown).arg(expand)
		: expand == 1 ? tr("all of 1 ply") : tr("all of %1 plies").arg(expand));
	expandLabel->setToolTip(tr("Plies shown completely; deeper only each move's best continuation. %1 nodes shown.")
		.arg(treeView->getShownNodeCount()));
	followButton->setChecked(treeView->isFollowing());
}
