#include "SearchStatusLine.hpp"

#include <algorithm>
#include <memory>

#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include "LegalMoveGenerator.hpp"
#include "SearchRecorder.hpp"
#include "SearchWindow.hpp"

namespace {

const QColor SEARCHING_COLOR(0, 210, 230);
const QColor DONE_COLOR(80, 200, 140);
const QColor FASTER_COLOR(80, 200, 110);
const QColor SLOWER_COLOR(230, 80, 70);
constexpr double TREND_MARGIN = 0.02;	//changes below 2% count as "about the same"

QColor withAlpha(QColor color, int alpha) {
	color.setAlpha(alpha);
	return color;
}

QString formatRate(double rate) {
	if (rate >= 1e6) return QString::number(rate / 1e6, 'f', 2) + "M/s";
	if (rate >= 1e3) return QString::number(rate / 1e3, 'f', 0) + "k/s";
	return QString::number(static_cast<qint64>(rate)) + "/s";
}

} // !namespace

SearchStatusLine::SearchStatusLine(QWidget* parent)
	: QWidget(parent) {
	QFont small = font();
	small.setPointSizeF(small.pointSizeF() * 0.85);
	setFont(small);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	timer = new QTimer(this);
	timer->setInterval(SearchWindow::UPDATE_INTERVAL_MS);
	connect(timer, &QTimer::timeout, this, &SearchStatusLine::poll);
}

void SearchStatusLine::setRecorder(SearchRecorder& newRecorder) {
	recorder = &newRecorder;
	if (isVisible()) {
		poll();
	}
}

QSize SearchStatusLine::sizeHint() const {
	return {180, fontMetrics().height() * 2 + 10};
}

void SearchStatusLine::showEvent(QShowEvent* event) {
	QWidget::showEvent(event);
	poll();
	timer->start();
}

void SearchStatusLine::hideEvent(QHideEvent* event) {
	QWidget::hideEvent(event);
	timer->stop();	//nobody asks for snapshots anymore
}

void SearchStatusLine::poll() {
	if (recorder == nullptr) {
		return;
	}
	if (const std::shared_ptr<const SearchSnapshot> snapshot = recorder->takeSnapshot(generation)) {
		generation = snapshot->generation;
		haveSearch = true;
		finished = snapshot->finished;
		seconds = snapshot->seconds;

		searchedMoves = 0;
		for (const SearchTreeNode& node : snapshot->nodes) {
			searchedMoves += node.parent == 0 && node.finished;
		}
		if (!snapshot->rootMoves.empty()) {
			rootMoves = static_cast<int>(snapshot->rootMoves.size());
		} else {
			if (rootCountSearch != snapshot->searchId) {
				BoardState board = snapshot->root;
				const Position position = board.getPosition();	//copy: the generator makes and unmakes moves on board
				legalRootMoves = static_cast<int>(LegalMoveGenerator{}.generateLegalMovesInPosition(position, board).count);
				rootCountSearch = snapshot->searchId;
			}
			rootMoves = legalRootMoves;
		}

		//speed since the last update, compared to the one before
		const bool sameSearch = snapshot->searchId == searchId;
		const double interval = snapshot->seconds - lastSeconds;
		if (finished) {
			rate = seconds > 0.001 ? snapshot->totalNodes / seconds : 0;
			trend = 0;
		} else if (sameSearch && interval > 0.05) {
			const double current = (snapshot->totalNodes - lastNodes) / interval;
			trend = rate <= 0 ? 0 : current > rate * (1 + TREND_MARGIN) ? 1 : current < rate * (1 - TREND_MARGIN) ? -1 : 0;
			rate = current;
		} else if (!sameSearch) {
			rate = seconds > 0.001 ? snapshot->totalNodes / seconds : 0;
			trend = 0;
		}
		searchId = snapshot->searchId;
		lastNodes = snapshot->totalNodes;
		lastSeconds = snapshot->seconds;
		update();
	}
	recorder->requestSnapshot();	//answered at the engine's next node, if it is searching
}

void SearchStatusLine::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QColor text = palette().color(QPalette::Text);
	const QFontMetricsF metrics(font());
	const qreal lineHeight = metrics.height();
	const QColor accent = !haveSearch ? withAlpha(text, 110) : finished ? DONE_COLOR : SEARCHING_COLOR;

	//line 1: root moves as a progress bar, the count beside it
	const QString count = haveSearch ? QString("%1/%2").arg(searchedMoves).arg(rootMoves) : QString::fromUtf8("–");
	const qreal countWidth = metrics.horizontalAdvance(QStringLiteral("00/00")) + 4;
	const QRectF countRect(width() - countWidth, 2, countWidth, lineHeight);
	painter.setPen(withAlpha(text, 170));
	painter.drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, count);
	const QRectF track(0, countRect.center().y() - 2.5, width() - countWidth - 6, 5);
	painter.setPen(Qt::NoPen);
	painter.setBrush(withAlpha(text, 30));
	painter.drawRoundedRect(track, 2.5, 2.5);
	if (haveSearch && rootMoves > 0) {
		painter.setBrush(accent);
		painter.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * std::min(1.0, static_cast<qreal>(searchedMoves) / rootMoves), track.height()), 2.5, 2.5);
	}

	//line 2: status · time · speed with its triangle
	const qreal y = countRect.bottom() + 4;
	qreal x = 0;
	painter.setBrush(accent);
	painter.drawEllipse(QPointF(x + 3.5, y + lineHeight / 2), 3.5, 3.5);
	x += 11;
	QString status = !haveSearch ? tr("Engine idle") : finished ? tr("Done") : tr("Thinking");
	if (haveSearch) {
		status += QString::fromUtf8(" · %1 s · %2").arg(seconds, 0, 'f', 1).arg(formatRate(rate));
	}
	painter.setPen(withAlpha(text, 190));
	painter.drawText(QRectF(x, y, width() - x, lineHeight), Qt::AlignLeft | Qt::AlignVCenter, status);
	if (haveSearch && trend != 0) {
		const qreal size = lineHeight * 0.32;
		const QPointF center(x + metrics.horizontalAdvance(status) + size + 4, y + lineHeight / 2);
		QPainterPath triangle;
		if (trend > 0) {
			triangle.moveTo(center.x(), center.y() - size);
			triangle.lineTo(center.x() + size, center.y() + size * 0.7);
			triangle.lineTo(center.x() - size, center.y() + size * 0.7);
		} else {
			triangle.moveTo(center.x(), center.y() + size);
			triangle.lineTo(center.x() + size, center.y() - size * 0.7);
			triangle.lineTo(center.x() - size, center.y() - size * 0.7);
		}
		triangle.closeSubpath();
		painter.setPen(Qt::NoPen);
		painter.setBrush(trend > 0 ? FASTER_COLOR : SLOWER_COLOR);
		painter.drawPath(triangle);
	}
}
