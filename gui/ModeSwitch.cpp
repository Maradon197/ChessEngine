#include "ModeSwitch.hpp"

#include <algorithm>
#include <cmath>

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace {

constexpr qreal INSET = 3;	//gap between the track and the highlight
constexpr int SEGMENT_PADDING = 16;
constexpr int VERTICAL_PADDING = 7;
constexpr int SLIDE_DURATION = 220;

QColor withAlpha(QColor color, int alpha) {
	color.setAlpha(alpha);
	return color;
}

QColor mix(const QColor& from, const QColor& to, qreal share) {
	const auto channel = [share](float a, float b) { return a + (b - a) * static_cast<float>(share); };
	return QColor::fromRgbF(channel(from.redF(), to.redF()), channel(from.greenF(), to.greenF()),
		channel(from.blueF(), to.blueF()), channel(from.alphaF(), to.alphaF()));
}

bool isDarkPalette(const QPalette& colors) {
	return colors.color(QPalette::Window).lightness() < 128;
}

} // !namespace

ModeSwitch::ModeSwitch(QWidget* parent)
	: QWidget(parent)
	, labels{tr("Analysis"), tr("Play")} {
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	setFocusPolicy(Qt::NoFocus);	//switching goes through the key bindings
	setCursor(Qt::PointingHandCursor);
	setMouseTracking(true);
	setAttribute(Qt::WA_Hover);

	slide = new QVariantAnimation(this);
	slide->setEasingCurve(QEasingCurve::OutCubic);
	connect(slide, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
		position = value.toReal();
		update();
	});
}

void ModeSwitch::setMode(ViewMode newMode) {
	if (mode == newMode) {
		return;
	}
	mode = newMode;
	const qreal target = mode == ViewMode::PLAY ? 1 : 0;
	slide->stop();
	slide->setStartValue(position);
	slide->setEndValue(target);
	slide->setDuration(std::max(1, static_cast<int>(SLIDE_DURATION * std::abs(target - position))));
	slide->start();
}

ViewMode ModeSwitch::getMode() const {
	return mode;
}

QSize ModeSwitch::sizeHint() const {
	const QFontMetrics metrics(getLabelFont());
	int segmentWidth = 0;
	for (const QString& label : labels) {
		segmentWidth = std::max(segmentWidth, metrics.horizontalAdvance(label));
	}
	segmentWidth += 2 * SEGMENT_PADDING;
	return {static_cast<int>(2 * segmentWidth + 2 * INSET), metrics.height() + 2 * VERTICAL_PADDING};
}

QSize ModeSwitch::minimumSizeHint() const {
	return sizeHint();
}

void ModeSwitch::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QPalette& colors = palette();
	const bool dark = isDarkPalette(colors);

	//track: a little sunken against the toolbar
	const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
	painter.setPen(QPen(withAlpha(colors.color(QPalette::Mid), 90), 1));
	painter.setBrush(colors.color(QPalette::Window).darker(dark ? 125 : 108));
	painter.drawRoundedRect(track, track.height() / 2, track.height() / 2);

	//highlight: raised, with a soft shadow below it
	const QRectF first = getSegmentRect(0);
	const QRectF thumb = first.translated(position * first.width(), 0);
	const qreal radius = thumb.height() / 2;
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0, 0, 0, dark ? 70 : 28));
	painter.drawRoundedRect(thumb.translated(0, 1), radius, radius);
	painter.setPen(QPen(withAlpha(colors.color(QPalette::Mid), dark ? 140 : 110), 1));
	painter.setBrush(colors.color(QPalette::Light));
	painter.drawRoundedRect(thumb, radius, radius);

	//labels fade between muted and full as the highlight passes under them
	painter.setFont(getLabelFont());
	const QColor text = colors.color(QPalette::Text);
	for (int index = 0; index < 2; ++index) {
		const qreal active = index == 0 ? 1 - position : position;
		const int restingAlpha = hovered == index ? 210 : 140;
		painter.setPen(mix(withAlpha(text, restingAlpha), text, active));
		painter.drawText(getSegmentRect(index), Qt::AlignCenter, labels[index]);
	}
}

void ModeSwitch::mousePressEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		pressed = getSegmentAt(event->position());
	}
}

void ModeSwitch::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() != Qt::LeftButton) {
		return;
	}
	const int released = getSegmentAt(event->position());
	if (released >= 0 && released == pressed) {
		emit modeRequested(released == 0 ? ViewMode::ANALYSIS : ViewMode::PLAY);
	}
	pressed = -1;
}

void ModeSwitch::mouseMoveEvent(QMouseEvent* event) {
	const int segment = getSegmentAt(event->position());
	if (segment != hovered) {
		hovered = segment;
		update();
	}
}

void ModeSwitch::leaveEvent(QEvent* event) {
	QWidget::leaveEvent(event);
	hovered = -1;
	update();
}

void ModeSwitch::changeEvent(QEvent* event) {
	QWidget::changeEvent(event);
	if (event->type() == QEvent::FontChange) {
		updateGeometry();
	}
}

QFont ModeSwitch::getLabelFont() const {
	QFont labelFont = font();
	labelFont.setWeight(QFont::DemiBold);
	return labelFont;
}

QRectF ModeSwitch::getSegmentRect(int index) const {
	const qreal segmentWidth = (width() - 2 * INSET) / 2;
	return {INSET + index * segmentWidth, INSET, segmentWidth, height() - 2 * INSET};
}

int ModeSwitch::getSegmentAt(const QPointF& point) const {
	if (!rect().contains(point.toPoint())) {
		return -1;
	}
	return point.x() < width() / 2.0 ? 0 : 1;
}
