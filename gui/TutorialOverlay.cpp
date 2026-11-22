#include "TutorialOverlay.hpp"

#include <algorithm>
#include <utility>

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>

namespace {

constexpr int BOX_WIDTH = 380;
constexpr int BOX_RADIUS = 10;
constexpr int HOLE_RADIUS = 8;
constexpr int GAP_TO_TARGET = 16;	//between the clear area and the text box
constexpr int EDGE_MARGIN = 12;		//the text box keeps this far from the window edges
constexpr int HOLE_DURATION = 320;
constexpr int FADE_DURATION = 200;
const QColor DIM_COLOR(0, 0, 0, 155);

//compared character by character: any change to the file (or to this text in a new version) shows the tutorial again
const char* const MARKER_TEXT = R"rhea(ChessEngineGUI - tutorial marker
================================
This file only tells the app that the tutorial has been shown.
Delete it, or change anything in it, and the tutorial plays again on the next start.
It can also be replayed any time with the "Tutorial" button in the top bar.


                         ___
                       /' o `\___
                      (      ____>     < tutorial done, you're ready! >
                       \    /
                        |  |
                        |  |
                        |  |
                        |  |
                    ___/    \___
              _.--''            ''--._
           .''   ,    ,    ,    ,     ''.
          /   ,    ,    ,    ,    ,     , \
         |  ,    ,    ,    ,    ,    ,    |\\
          \    ,    ,    ,    ,    ,     / \\\
           '._    ,    ,    ,    ,    _.'   ``
              ''--..____________..--''
                     |        |
                     |        |
                     |        |
                     |        |
                    _|\      _|\
                  -' `\`   -' `\`

         this rhea is not stressed at all. (for now.)
)rhea";

QRectF lerp(const QRectF& from, const QRectF& to, qreal t) {
	const auto mix = [t](qreal a, qreal b) { return a + (b - a) * t; };
	return QRectF(QPointF(mix(from.left(), to.left()), mix(from.top(), to.top())),
		QSizeF(mix(from.width(), to.width()), mix(from.height(), to.height())));
}

bool hasArea(const QRectF& rect) {
	return rect.width() >= 1 && rect.height() >= 1;
}

QColor withAlpha(QColor color, int alpha) {
	color.setAlpha(alpha);
	return color;
}

} // !namespace

TutorialOverlay::TutorialOverlay(QWidget& window, std::vector<TutorialLesson> lessons, TargetLookup lookup)
	: QWidget(&window)
	, lessons(std::move(lessons))
	, lookup(std::move(lookup)) {
	hide();

	//text box: lesson text, then "2 / 7 ... Skip  Next"
	box = new QWidget(this);
	box->setFixedWidth(BOX_WIDTH);
	auto* boxLayout = new QVBoxLayout(box);
	boxLayout->setContentsMargins(18, 16, 18, 14);
	boxLayout->setSpacing(12);

	textLabel = new QLabel(box);
	textLabel->setTextFormat(Qt::MarkdownText);
	textLabel->setWordWrap(true);
	textLabel->setOpenExternalLinks(true);
	textLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	boxLayout->addWidget(textLabel);

	auto* buttonRow = new QHBoxLayout;
	buttonRow->setSpacing(8);
	stepLabel = new QLabel(box);
	stepLabel->setForegroundRole(QPalette::PlaceholderText);
	skipButton = new QPushButton(tr("Skip"), box);
	skipButton->setFlat(true);
	skipButton->setCursor(Qt::PointingHandCursor);
	nextButton = new QPushButton(box);
	nextButton->setCursor(Qt::PointingHandCursor);
	buttonRow->addWidget(stepLabel);
	buttonRow->addStretch(1);
	buttonRow->addWidget(skipButton);
	buttonRow->addWidget(nextButton);
	boxLayout->addLayout(buttonRow);

	connect(nextButton, &QPushButton::clicked, this, &TutorialOverlay::next);
	connect(skipButton, &QPushButton::clicked, this, &TutorialOverlay::finish);

	//the clear area glides from one target to the next
	holeAnimation = new QVariantAnimation(this);
	holeAnimation->setStartValue(0.0);
	holeAnimation->setEndValue(1.0);
	holeAnimation->setDuration(HOLE_DURATION);
	holeAnimation->setEasingCurve(QEasingCurve::OutCubic);
	connect(holeAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
		holeProgress = value.toReal();
		refresh();
	});

	//fades in and out as a whole; the effect is off in between so it costs nothing while reading
	fadeEffect = new QGraphicsOpacityEffect(this);
	fadeEffect->setOpacity(0);
	setGraphicsEffect(fadeEffect);
	fade = new QVariantAnimation(this);
	fade->setDuration(FADE_DURATION);
	fade->setEasingCurve(QEasingCurve::InOutQuad);
	connect(fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) { fadeEffect->setOpacity(value.toReal()); });
	connect(fade, &QVariantAnimation::finished, this, [this] {
		if (running) {
			fadeEffect->setEnabled(false);
		} else {
			hide();
		}
	});

	window.installEventFilter(this);	//resizes
}

void TutorialOverlay::start() {
	if (running || lessons.empty()) {
		return;
	}
	running = true;
	setGeometry(parentWidget()->rect());
	hole = QRectF(QRectF(rect()).center(), QSizeF());
	show();
	raise();

	fadeEffect->setEnabled(true);
	fade->stop();
	fade->setStartValue(fadeEffect->opacity());
	fade->setEndValue(1.0);
	fade->start();

	showLesson(0);
}

bool TutorialOverlay::isRunning() const {
	return running;
}

QString TutorialOverlay::getMarkerPath() {
	//the same folder the finished games are saved in, see saveGame() in main.cpp
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/games/tutorial.txt";
}

bool TutorialOverlay::isMarkerCurrent() {
	QFile file(getMarkerPath());
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}
	return QString::fromUtf8(file.readAll()) == QString::fromUtf8(MARKER_TEXT);
}

void TutorialOverlay::writeMarker() {
	const QString path = getMarkerPath();
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		file.write(MARKER_TEXT);
	}
}

bool TutorialOverlay::eventFilter(QObject* watched, QEvent* event) {
	if (watched == parentWidget() && event->type() == QEvent::Resize) {
		setGeometry(parentWidget()->rect());
		refresh();
	} else if (watched == targetWidget.data()) {
		switch (event->type()) {
			case QEvent::Move:
			case QEvent::Resize:
			case QEvent::Show:
			case QEvent::Hide:
				refresh();
				break;
			default:
				break;
		}
	}
	return false;
}

void TutorialOverlay::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	//dimmed everywhere but the hole
	QPainterPath dim;
	dim.addRect(rect());
	const qreal radius = std::min<qreal>(HOLE_RADIUS, std::min(hole.width(), hole.height()) / 2);
	if (hasArea(hole)) {
		QPainterPath cut;
		cut.addRoundedRect(hole, radius, radius);
		dim = dim.subtracted(cut);
	}
	painter.fillPath(dim, DIM_COLOR);
	if (hasArea(hole)) {
		painter.setPen(QPen(withAlpha(palette().color(QPalette::Highlight), 220), 2));
		painter.setBrush(Qt::NoBrush);
		painter.drawRoundedRect(hole.adjusted(-1, -1, 1, 1), radius + 1, radius + 1);
	}

	//the text box's background, the labels and buttons draw on top of it
	const QRectF boxRect = QRectF(box->geometry()).adjusted(0.5, 0.5, -0.5, -0.5);
	painter.setPen(QPen(palette().color(QPalette::Mid), 1));
	painter.setBrush(palette().color(QPalette::Window));
	painter.drawRoundedRect(boxRect, BOX_RADIUS, BOX_RADIUS);
}

//clicks next to the text box do nothing, and don't reach the window underneath
void TutorialOverlay::mousePressEvent(QMouseEvent* event) {
	event->accept();
}

void TutorialOverlay::mouseReleaseEvent(QMouseEvent* event) {
	event->accept();
}

void TutorialOverlay::wheelEvent(QWheelEvent* event) {
	event->accept();
}

void TutorialOverlay::contextMenuEvent(QContextMenuEvent* event) {
	event->accept();
}

void TutorialOverlay::showLesson(int index) {
	current = index;
	const TutorialLesson& lesson = lessons[static_cast<std::size_t>(index)];
	const bool last = index + 1 == static_cast<int>(lessons.size());

	textLabel->setText(lesson.text);
	textLabel->setVisible(!lesson.text.isEmpty());
	stepLabel->setText(QString("%1 / %2").arg(index + 1).arg(lessons.size()));
	nextButton->setText(last ? tr("Finish") : tr("Next"));
	box->adjustSize();

	emit lessonEntered(lesson);	//view switch, drawer, sidebar
	const bool usesWidget = lesson.target != TutorialTarget::NONE && lesson.target != TutorialTarget::CUSTOM;
	watch(usesWidget ? lookup(lesson.target) : nullptr);
	raise();	//over anything the window just showed

	holeFrom = hole;
	holeProgress = 0;
	holeAnimation->stop();
	holeAnimation->start();
	refresh();
	QTimer::singleShot(0, this, &TutorialOverlay::refresh);	//once the layouts have settled
	nextButton->setFocus();
}

void TutorialOverlay::next() {
	if (!running) {
		return;
	}
	if (current + 1 < static_cast<int>(lessons.size())) {
		showLesson(current + 1);
	} else {
		finish();
	}
}

void TutorialOverlay::finish() {
	if (!running) {
		return;
	}
	running = false;
	holeAnimation->stop();
	watch(nullptr);
	emit finished();

	fadeEffect->setEnabled(true);
	fade->stop();
	fade->setStartValue(fadeEffect->opacity());
	fade->setEndValue(0.0);
	fade->start();
}

void TutorialOverlay::refresh() {
	if (!running) {
		return;
	}
	const QRectF target = getTargetRect();
	//nothing to highlight: the hole shrinks into its middle, or grows out of the new target's middle
	const QRectF from = hasArea(holeFrom) ? holeFrom : QRectF(target.center(), QSizeF());
	const QRectF to = hasArea(target) ? target : QRectF(from.center(), QSizeF());
	hole = lerp(from, to, holeProgress);
	placeBox(target);
	update();
}

QRectF TutorialOverlay::getTargetRect() const {
	if (current < 0) {
		return {};
	}
	const TutorialLesson& lesson = lessons[static_cast<std::size_t>(current)];
	QRectF area;
	if (lesson.target == TutorialTarget::CUSTOM) {
		const QRectF& fraction = lesson.customArea;
		area = QRectF(fraction.x() * width(), fraction.y() * height(), fraction.width() * width(), fraction.height() * height());
	} else if (targetWidget != nullptr && targetWidget->isVisible()) {
		area = QRectF(QRect(targetWidget->mapTo(parentWidget(), QPoint(0, 0)), targetWidget->size()));	//the overlay covers the window from (0, 0)
	} else {
		return {};
	}
	area = area.marginsAdded(QMarginsF(lesson.padding)).intersected(QRectF(rect()));
	return hasArea(area) ? area : QRectF();
}

//next to the target where there's room: below, right, above, left; centered when there's no target or no room
void TutorialOverlay::placeBox(const QRectF& target) {
	const QSizeF size = box->size();
	const QRectF bounds = QRectF(rect()).adjusted(EDGE_MARGIN, EDGE_MARGIN, -EDGE_MARGIN, -EDGE_MARGIN);
	QPointF position = bounds.center() - QPointF(size.width() / 2, size.height() / 2);
	if (hasArea(target)) {
		const QPointF center = target.center();
		if (target.bottom() + GAP_TO_TARGET + size.height() <= bounds.bottom()) {
			position = QPointF(center.x() - size.width() / 2, target.bottom() + GAP_TO_TARGET);
		} else if (target.right() + GAP_TO_TARGET + size.width() <= bounds.right()) {
			position = QPointF(target.right() + GAP_TO_TARGET, center.y() - size.height() / 2);
		} else if (target.top() - GAP_TO_TARGET - size.height() >= bounds.top()) {
			position = QPointF(center.x() - size.width() / 2, target.top() - GAP_TO_TARGET - size.height());
		} else if (target.left() - GAP_TO_TARGET - size.width() >= bounds.left()) {
			position = QPointF(target.left() - GAP_TO_TARGET - size.width(), center.y() - size.height() / 2);
		}
	}
	position.setX(std::clamp(position.x(), bounds.left(), std::max(bounds.left(), bounds.right() - size.width())));
	position.setY(std::clamp(position.y(), bounds.top(), std::max(bounds.top(), bounds.bottom() - size.height())));
	box->move(position.toPoint());
}

void TutorialOverlay::watch(QWidget* widget) {
	if (targetWidget != nullptr) {
		targetWidget->removeEventFilter(this);
	}
	targetWidget = widget;
	if (widget != nullptr) {
		widget->installEventFilter(this);
	}
}
