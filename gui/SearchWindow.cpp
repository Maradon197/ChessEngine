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

//a column at the window's right edge: how far through the root moves the search is on top, the numbers as tiles below
class SearchStatsBar : public QWidget {
public:
	explicit SearchStatsBar(QWidget* parent) : QWidget(parent) {
		setFixedWidth(COLUMN_WIDTH);
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
		const bool searching = model && !model->getSnapshot().finished;
		if (!model) {
			tiles.push_back({tr("Waiting"), tr("status"), withAlpha(text, 120)});
		} else {
			const SearchSnapshot& snapshot = model->getSnapshot();
			const bool done = snapshot.finished;
			tiles.push_back({done ? tr("Done") : tr("Thinking"), tr("status"), done ? BEST_COLOR : SEARCHING_COLOR});
			tiles.push_back({QString::number(snapshot.depth), tr("Maximum depth"), {}});
			tiles.push_back({done ? QString::fromUtf8("–") : QString::number(snapshot.currentPly), tr("Current depth"),
				done ? QColor() : SEARCHING_COLOR});
			tiles.push_back({tr("Max"), tr("Target Elo"), {}});
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

		//progress on top, the tiles stacked below it
		constexpr qreal gap = 8;
		const QRectF progress(gap, gap, width() - 2 * gap, PROGRESS_HEIGHT);
		const auto count = static_cast<qreal>(tiles.size());
		const qreal tilesTop = progress.bottom() + gap;
		const qreal tileHeight = std::clamp((height() - tilesTop - gap * count) / count, 40.0, 62.0);
		QFont valueFont = font();
		valueFont.setPointSizeF(valueFont.pointSizeF() * 1.3);
		valueFont.setBold(true);
		QFont captionFont = font();
		captionFont.setPointSizeF(captionFont.pointSizeF() * 0.82);
		for (std::size_t index = 0; index < tiles.size(); ++index) {
			const Tile& tile = tiles[index];
			const QRectF box(gap, tilesTop + static_cast<qreal>(index) * (tileHeight + gap), width() - 2 * gap, tileHeight);
			painter.setPen(QPen(tile.accent.isValid() ? withAlpha(tile.accent, 150) : withAlpha(text, 35), tile.accent.isValid() ? 1.5 : 1));
			painter.setBrush(palette().color(QPalette::Base));
			painter.drawRoundedRect(box, 7, 7);

			QRectF inner = box.adjusted(12, 4, -10, -4);
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

		paintProgress(painter, progress);
	}

public:
	static constexpr int COLUMN_WIDTH = 230;

private:
	static constexpr qreal PROGRESS_HEIGHT = 100;

	//"7 / 20 root moves", the move being searched, a bar
	void paintProgress(QPainter& painter, const QRectF& box) {
		const QColor text = palette().color(QPalette::Text);
		const bool searching = model && !model->getSnapshot().finished;
		const QColor accent = !model ? withAlpha(text, 120) : searching ? SEARCHING_COLOR : BEST_COLOR;
		painter.setPen(QPen(withAlpha(accent, 150), 1.5));
		painter.setBrush(palette().color(QPalette::Base));
		painter.drawRoundedRect(box, 7, 7);
		if (!model || box.width() < 60) {
			return;
		}

		int total = static_cast<int>(model->getRootMoves().size());
		int done = 0;
		QString current;
		for (int child : model->getChildren(0)) {
			if (model->getNode(child).finished) {
				++done;
			} else {
				current = model->getMoveName(child);
			}
		}
		total = std::max(total, 1);
		const QRectF inner = box.adjusted(14, 8, -14, -10);

		//"7 / 20", underneath what it counts and the move being searched
		QFont bigFont = font();
		bigFont.setPointSizeF(bigFont.pointSizeF() * 2.0);
		bigFont.setBold(true);
		painter.setFont(bigFont);
		painter.setPen(accent);
		const qreal bigHeight = QFontMetricsF(bigFont).height();
		painter.drawText(QRectF(inner.left(), inner.top(), inner.width(), bigHeight), Qt::AlignLeft | Qt::AlignVCenter,
			QString("%1 / %2").arg(done).arg(total));
		painter.setPen(withAlpha(text, 150));
		QFont percentFont = font();
		painter.setFont(percentFont);
		painter.drawText(QRectF(inner.left(), inner.top(), inner.width(), bigHeight), Qt::AlignRight | Qt::AlignVCenter,
			QString("%1%").arg(100 * done / total));

		const qreal lineHeight = QFontMetricsF(font()).height();
		const QRectF captionRow(inner.left(), inner.top() + bigHeight, inner.width(), lineHeight);
		painter.setPen(withAlpha(text, 170));
		painter.drawText(captionRow, Qt::AlignLeft | Qt::AlignVCenter, tr("root moves searched"));
		if (searching && !current.isEmpty()) {
			QFont nowFont = font();
			nowFont.setBold(true);
			painter.setFont(nowFont);
			painter.setPen(SEARCHING_COLOR);
			painter.drawText(captionRow.translated(0, lineHeight), Qt::AlignLeft | Qt::AlignVCenter, tr("now: %1").arg(current));
		}

		//the bar: finished moves solid, the one being searched pulsing
		const QRectF track(inner.left(), inner.bottom() - 7, inner.width(), 7);
		painter.setPen(Qt::NoPen);
		painter.setBrush(withAlpha(text, 30));
		painter.drawRoundedRect(track, 3.5, 3.5);
		const qreal fraction = static_cast<qreal>(done) / total;
		painter.setBrush(accent);
		painter.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * fraction, track.height()), 3.5, 3.5);
		if (searching && done < total) {
			painter.setBrush(withAlpha(SEARCHING_COLOR, static_cast<int>(60 + 120 * getPulse(clock))));
			painter.drawRoundedRect(QRectF(track.left() + track.width() * fraction, track.top(), track.width() / total, track.height()), 3.5, 3.5);
		}
	}

	std::shared_ptr<const SearchTreeModel> model;
};

//every legal move at the root in the order the engine goes through them: searched ones filled with their score,
//the one being searched pulses, the best one is ringed; the bar underneath is how much was searched below it
class RootMovesStrip : public QWidget {
public:
	explicit RootMovesStrip(QWidget* parent) : QWidget(parent) {
		setFixedHeight(48);
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
	static constexpr qreal MARGIN = 8;

	QRectF getChipRect(std::size_t index) const {
		const auto count = static_cast<qreal>(model->getRootMoves().size());
		const qreal space = width() - 2 * MARGIN;
		const qreal step = std::min(84.0, space / count);
		const qreal chipWidth = std::max(6.0, step - 5);
		return {MARGIN + static_cast<qreal>(index) * step, 6, chipWidth, 28};
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
		const QString evalText = tr("Black ↔ White better");
		painter.drawText(QPointF(x, y + metrics.ascent() / 2 - 1), evalText);
		x += metrics.horizontalAdvance(evalText) + 22;

		const auto item = [&](const QPen& pen, const QString& label) {
			if (x + 25 + metrics.horizontalAdvance(label) > width() - 4) {
				return;	//no room left, the tooltips explain it too
			}
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
		item(QPen(CUTOFF_COLOR, 1.8, Qt::DashLine), tr("cut short (✂ skipped)"));
		item(QPen(BEST_COLOR, 1.8), tr("raised alpha"));
		item(QPen(withAlpha(text, 90), 1), tr("not better"));
		item(QPen(SEARCHING_COLOR, 2.4), tr("searching"));
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

	//left: root moves, tree, best line; right: progress and numbers
	auto* outer = new QHBoxLayout(this);
	outer->setContentsMargins(6, 4, 2, 6);
	outer->setSpacing(4);
	auto* layout = new QVBoxLayout;
	layout->setSpacing(2);
	outer->addLayout(layout, 1);
	stats = new SearchStatsBar(this);
	outer->addWidget(stats);

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
	connect(treeView, &SearchTreeView::nodeActivated, this, [this](int node) {
		if (!model) {
			return;
		}
		std::vector<Move> moves;
		for (int current = node; current > 0; current = model->getNode(current).parent) {
			moves.push_back(model->getNode(current).move);
		}
		std::reverse(moves.begin(), moves.end());
		emit lineChosen(model->getSnapshot().origin, moves);
	});

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
	emit shownChanged(true);
}

void SearchWindow::hideEvent(QHideEvent* event) {
	QWidget::hideEvent(event);
	pollTimer->stop();	//nobody asks for snapshots anymore, the search runs undisturbed
	QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(GEOMETRY_KEY, saveGeometry());
	emit shownChanged(false);
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
	expandLabel->setText(expand == 1 ? tr("1 ply · %1 lines").arg(treeView->getLineCount())
		: tr("%1 plies · %2 lines").arg(expand).arg(treeView->getLineCount()));
	expandLabel->setToolTip(tr("1 ply: only the root moves with their best lines. 2 plies: also their other replies, as far as %1 lines allow; the rest is gathered in groups.")
		.arg(SearchTreeView::MAX_LINES));
	followButton->setChecked(treeView->isFollowing());
}
