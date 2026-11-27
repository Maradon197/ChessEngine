#include "EvalCriteriaWidget.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "Doorman.hpp"

namespace {

const QColor WHITE_SIDE(245, 245, 245);
const QColor BLACK_SIDE(30, 30, 30);

constexpr qreal CARD_RADIUS = 12;
constexpr int CARD_PADDING = 12;
constexpr int HEADER_HEIGHT = 22;
constexpr int ANIMATION_DURATION = 260;

constexpr qreal TRACK_HEIGHT = 20;
constexpr qreal TRACK_GAP = 6;	//between a criterion's name and its track
constexpr qreal SEGMENT_GAP = 3;
constexpr qreal SEGMENT_RADIUS = 4;

const QString MINUS_SIGN = QString::fromUtf8("−");

QString formatSigned(int value) {
	if (value > 0) {
		return QString("+%1").arg(value);
	}
	if (value < 0) {
		return MINUS_SIGN + QString::number(-value);
	}
	return "0";
}

QString getCriterionName(EvalCriterion criterion) {
	switch (criterion) {
		case EvalCriterion::WEIGHTED_MATERIAL: return EvalCriteriaWidget::tr("Weighted material");
		case EvalCriterion::KING_SAFETY: return EvalCriteriaWidget::tr("King safety");
		case EvalCriterion::PIECE_ACTIVITY: return EvalCriteriaWidget::tr("Piece activity");
		case EvalCriterion::SPACE: return EvalCriteriaWidget::tr("Space");
		default: return {};
	}
}

QFont makeHeaderFont(const QFont& base) {
	QFont font = base;
	font.setBold(true);
	font.setCapitalization(QFont::AllUppercase);
	font.setLetterSpacing(QFont::PercentageSpacing, 108);
	font.setPointSizeF(font.pointSizeF() * 0.85);
	return font;
}

QColor withAlpha(QColor color, int alpha) {
	color.setAlpha(alpha);
	return color;
}

QVariantAnimation* makeEasing(QObject* parent) {
	auto* animation = new QVariantAnimation(parent);
	animation->setDuration(ANIMATION_DURATION);
	animation->setEasingCurve(QEasingCurve::OutCubic);
	return animation;
}

void easeTo(QVariantAnimation& animation, qreal from, qreal to) {
	animation.stop();
	animation.setStartValue(from);
	animation.setEndValue(to);
	animation.start();
}

} // !namespace

//rounded panel with a small caps title
class Card : public QWidget {
public:
	Card(const QString& title, QWidget* parent)
		: QWidget(parent)
		, title(title) {
	}

	void setColor(const QColor& color) {
		panelColor = color;
		update();
	}


protected:
	void paintEvent(QPaintEvent* event) override {
		Q_UNUSED(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		const QPalette& colors = palette();
		painter.setPen(QPen(withAlpha(colors.color(QPalette::Mid), 90), 1));
		painter.setBrush(panelColor.isValid() ? panelColor : colors.color(QPalette::Window));
		painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), CARD_RADIUS, CARD_RADIUS);

		painter.setFont(makeHeaderFont(font()));
		painter.setPen(withAlpha(colors.color(QPalette::Text), 150));
		painter.drawText(QRectF(CARD_PADDING, CARD_PADDING - 2, width() - 2 * CARD_PADDING, HEADER_HEIGHT), Qt::AlignLeft | Qt::AlignTop, title);
	}

private:
	QString title;
	QColor panelColor;
};

//one track of three steps: it fills with the color of whoever is ahead on this criterion, empty means equal
class CriterionBar : public QWidget {
public:
	CriterionBar(const QString& name, QWidget* parent)
		: QWidget(parent)
		, name(name) {
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		easing = makeEasing(this);
		connect(easing, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
			shownValue = value.toReal();
			update();
		});
		updateToolTip();
	}

	void setValue(std::optional<int> newValue) {
		if (newValue) {
			easeTo(*easing, value ? shownValue : 0, *newValue);
		} else {
			easing->stop();
			shownValue = 0;
		}
		value = newValue;
		updateToolTip();
		update();
	}

	std::optional<int> getValue() const {
		return value;
	}

	QSize sizeHint() const override {
		return {220, static_cast<int>(std::ceil(fontMetrics().height() + TRACK_GAP + TRACK_HEIGHT))};
	}

	QSize minimumSizeHint() const override {
		return {140, sizeHint().height()};
	}

protected:
	void paintEvent(QPaintEvent* event) override {
		Q_UNUSED(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		const QColor text = palette().color(QPalette::Text);

		const qreal textHeight = fontMetrics().height();
		painter.setPen(value ? text : withAlpha(text, 120));
		painter.drawText(QRectF(0, 0, width(), textHeight), Qt::AlignLeft | Qt::AlignVCenter, name);

		const QRectF track(0.5, textHeight + TRACK_GAP + 0.5, width() - 1.0, TRACK_HEIGHT - 1.0);
		const int segmentCount = std::max(CRITERION_MAX, -CRITERION_MIN);
		const qreal segmentWidth = (track.width() - SEGMENT_GAP * (segmentCount - 1)) / segmentCount;
		const qreal filled = std::abs(shownValue);	//in segments, fractional while easing
		const QColor side = shownValue >= 0 ? WHITE_SIDE : BLACK_SIDE;

		for (int index = 0; index < segmentCount; ++index) {
			const QRectF segment(track.left() + index * (segmentWidth + SEGMENT_GAP), track.top(), segmentWidth, track.height());
			QPainterPath shape;
			shape.addRoundedRect(segment, SEGMENT_RADIUS, SEGMENT_RADIUS);
			painter.fillPath(shape, withAlpha(text, value ? 22 : 12));

			const qreal fill = value ? std::clamp(filled - index, 0.0, 1.0) : 0.0;
			if (fill > 0) {
				painter.save();
				painter.setClipPath(shape);
				painter.fillRect(QRectF(segment.left(), segment.top(), segment.width() * fill, segment.height()), side);
				painter.restore();
			}
			painter.setPen(QPen(withAlpha(text, fill > 0 ? 170 : value ? 70 : 40), 1));
			painter.setBrush(Qt::NoBrush);
			painter.drawPath(shape);
		}
	}

private:
	void updateToolTip() {
		setToolTip(value ? QString("%1: %2").arg(name, formatSigned(*value)) : EvalCriteriaWidget::tr("%1: no evaluation yet").arg(name));
	}

	QString name;
	std::optional<int> value;
	qreal shownValue = 0;
	QVariantAnimation* easing = nullptr;
};

EvalCriteriaWidget::EvalCriteriaWidget(Doorman& doorman, QWidget* parent)
	: QWidget(parent) {
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(10);

	//one three-step track per criterion
	criteriaCard = new Card(tr("Why"), this);
	auto* criteriaLayout = new QVBoxLayout(criteriaCard);
	criteriaLayout->setContentsMargins(CARD_PADDING, CARD_PADDING + HEADER_HEIGHT, CARD_PADDING, CARD_PADDING);
	criteriaLayout->setSpacing(14);
	for (std::size_t index = 0; index < CRITERION_COUNT; ++index) {
		bars[index] = new CriterionBar(getCriterionName(static_cast<EvalCriterion>(index)), criteriaCard);
		criteriaLayout->addWidget(bars[index]);
	}
	layout->addWidget(criteriaCard);

	doorman.attach(*this);
}

void EvalCriteriaWidget::setCriterion(EvalCriterion criterion, int value) {
	const auto index = static_cast<std::size_t>(criterion);
	if (index < CRITERION_COUNT) {
		bars[index]->setValue(std::clamp(value, CRITERION_MIN, CRITERION_MAX));
	}
}

std::optional<int> EvalCriteriaWidget::getCriterion(EvalCriterion criterion) const {
	const auto index = static_cast<std::size_t>(criterion);
	return index < CRITERION_COUNT ? bars[index]->getValue() : std::nullopt;
}

void EvalCriteriaWidget::clearCriteria() {
	for (CriterionBar* bar : bars) {
		bar->setValue(std::nullopt);
	}
}

void EvalCriteriaWidget::setPanelColor(const QColor& color) {
	criteriaCard->setColor(color);
}
