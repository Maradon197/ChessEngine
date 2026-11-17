#include "EvalCriteriaWidget.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <QEvent>
#include <QLabel>
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

//same curve lichess uses for its eval bar: centipawns to the share of the bar that is white
constexpr double WIN_CHANCE_SLOPE = 0.00368208;
constexpr double MIN_SHARE = 0.04;	//a bar that is completely one color is reserved for mates

const QString MINUS_SIGN = QString::fromUtf8("−");
const QString DASH = QString::fromUtf8("–");

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

//the number in words, for players who don't think in pawns yet
QString getVerdict(int centipawns) {
	const int magnitude = std::abs(centipawns);
	if (magnitude < 25) {
		return EvalCriteriaWidget::tr("The position is balanced");
	}
	const bool white = centipawns > 0;
	if (magnitude < 75) {
		return white ? EvalCriteriaWidget::tr("White is slightly better") : EvalCriteriaWidget::tr("Black is slightly better");
	}
	if (magnitude < 200) {
		return white ? EvalCriteriaWidget::tr("White is better") : EvalCriteriaWidget::tr("Black is better");
	}
	if (magnitude < 500) {
		return white ? EvalCriteriaWidget::tr("White is clearly better") : EvalCriteriaWidget::tr("Black is clearly better");
	}
	return white ? EvalCriteriaWidget::tr("White is winning") : EvalCriteriaWidget::tr("Black is winning");
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

	//muted text, follows the theme
	void setVerdictLabel(QLabel* label) {
		verdict = label;
		updateVerdictColor();
	}

protected:
	void changeEvent(QEvent* event) override {
		QWidget::changeEvent(event);
		if (event->type() == QEvent::PaletteChange) {
			updateVerdictColor();
		}
	}

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
	void updateVerdictColor() {
		if (verdict == nullptr) {
			return;
		}
		QPalette colors = verdict->palette();
		colors.setColor(QPalette::WindowText, withAlpha(palette().color(QPalette::Text), 175));
		verdict->setPalette(colors);
	}

	QString title;
	QColor panelColor;
	QLabel* verdict = nullptr;
};

//one pill, black from the left and white from the right; the split shows who is ahead overall
class BalanceBar : public QWidget {
public:
	explicit BalanceBar(QWidget* parent)
		: QWidget(parent) {
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		easing = makeEasing(this);
		connect(easing, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
			shownShare = value.toReal();
			update();
		});
	}

	//share of the bar that is white, nullopt: no evaluation
	void setShare(std::optional<qreal> share) {
		if (!share) {
			easing->stop();
			hasShare = false;
			shownShare = 0.5;
			update();
			return;
		}
		easeTo(*easing, hasShare ? shownShare : 0.5, std::clamp(*share, 0.0, 1.0));
		hasShare = true;
	}

	QSize sizeHint() const override {
		return {200, 16};
	}

protected:
	void paintEvent(QPaintEvent* event) override {
		Q_UNUSED(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		const QPalette& colors = palette();
		const QRectF pill = QRectF(rect()).adjusted(1, 1, -1, -1);
		const qreal radius = pill.height() / 2;
		QPainterPath shape;
		shape.addRoundedRect(pill, radius, radius);

		if (hasShare) {
			const qreal split = pill.left() + (1 - shownShare) * pill.width();
			painter.save();
			painter.setClipPath(shape);
			painter.fillRect(QRectF(pill.left(), pill.top(), split - pill.left(), pill.height()), BLACK_SIDE);
			painter.fillRect(QRectF(split, pill.top(), pill.right() - split, pill.height()), WHITE_SIDE);
			painter.restore();
		} else {
			painter.fillPath(shape, colors.color(QPalette::Window).darker(115));
		}

		//the middle: equal position
		const qreal middle = pill.center().x();
		painter.setPen(QPen(QColor(128, 128, 128, 200), 1.5));
		painter.drawLine(QPointF(middle, pill.top() + 3), QPointF(middle, pill.bottom() - 3));

		painter.setPen(QPen(withAlpha(colors.color(QPalette::Text), 160), 1));
		painter.setBrush(Qt::NoBrush);
		painter.drawPath(shape);
	}

private:
	QVariantAnimation* easing = nullptr;
	qreal shownShare = 0.5;
	bool hasShare = false;
};

//tug of war: black pulls to the left, white to the right, the knot shows who wins this criterion
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
	}

	void setValue(std::optional<int> newValue) {
		if (newValue) {
			easeTo(*easing, value ? shownValue : 0, *newValue);
		} else {
			easing->stop();
			shownValue = 0;
		}
		value = newValue;
		update();
	}

	std::optional<int> getValue() const {
		return value;
	}

	QSize sizeHint() const override {
		return {220, fontMetrics().height() + 26};
	}

	QSize minimumSizeHint() const override {
		return {140, sizeHint().height()};
	}

protected:
	void paintEvent(QPaintEvent* event) override {
		Q_UNUSED(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		const QPalette& colors = palette();
		const QColor text = colors.color(QPalette::Text);

		//name and number
		const qreal textHeight = fontMetrics().height();
		const QRectF textRow(0, 0, width(), textHeight);
		painter.setPen(text);
		painter.drawText(textRow, Qt::AlignLeft | Qt::AlignVCenter, name);
		QFont valueFont = font();
		valueFont.setBold(true);
		painter.setFont(valueFont);
		painter.setPen(value ? text : withAlpha(text, 120));
		painter.drawText(textRow, Qt::AlignRight | Qt::AlignVCenter, value ? formatSigned(*value) : DASH);

		//rope between the two ends
		const qreal capRadius = 5;
		const qreal knotRadius = 7;
		const qreal ropeY = textHeight + 13;
		const qreal left = capRadius + 1;
		const qreal right = width() - capRadius - 1;
		const qreal inner = knotRadius + 4;	//the knot stops before the end caps
		const auto toX = [&](qreal criterionValue) {
			const qreal share = (criterionValue - CRITERION_MIN) / static_cast<qreal>(CRITERION_MAX - CRITERION_MIN);
			return left + inner + share * (right - left - 2 * inner);
		};

		painter.setPen(Qt::NoPen);
		painter.setBrush(withAlpha(text, value ? 45 : 25));
		painter.drawRoundedRect(QRectF(left, ropeY - 1.5, right - left, 3), 1.5, 1.5);

		//a dot for every step, a line for the middle
		painter.setBrush(withAlpha(text, value ? 90 : 45));
		for (int step = CRITERION_MIN; step <= CRITERION_MAX; ++step) {
			if (step != 0) {
				painter.drawEllipse(QPointF(toX(step), ropeY), 1.6, 1.6);
			}
		}
		painter.setPen(QPen(withAlpha(text, value ? 150 : 70), 1.5));
		painter.drawLine(QPointF(toX(0), ropeY - 6), QPointF(toX(0), ropeY + 6));

		const QPen outline(withAlpha(text, 170), 1);
		painter.setPen(outline);
		painter.setBrush(BLACK_SIDE);
		painter.drawEllipse(QPointF(left, ropeY), capRadius, capRadius);
		painter.setBrush(WHITE_SIDE);
		painter.drawEllipse(QPointF(right, ropeY), capRadius, capRadius);

		if (!value) {
			return;
		}
		//pulled part of the rope, then the knot
		const qreal center = toX(0);
		const qreal knot = toX(shownValue);
		const QColor side = shownValue >= 0 ? WHITE_SIDE : BLACK_SIDE;
		if (std::abs(knot - center) > 0.5) {
			painter.setBrush(side);
			painter.drawRoundedRect(QRectF(std::min(center, knot), ropeY - 3, std::abs(knot - center), 6), 3, 3);
		}
		painter.setBrush(std::abs(shownValue) < 0.05 ? QColor(128, 128, 128) : side);
		painter.setPen(QPen(text, 1.5));
		painter.drawEllipse(QPointF(knot, ropeY), knotRadius, knotRadius);
	}

private:
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

	//overall: number, sentence, balance
	overallCard = new Card(tr("Overall"), this);
	auto* overallLayout = new QVBoxLayout(overallCard);
	overallLayout->setContentsMargins(CARD_PADDING, CARD_PADDING + HEADER_HEIGHT - 4, CARD_PADDING, CARD_PADDING + 2);
	overallLayout->setSpacing(2);

	overallValueLabel = new QLabel(DASH, overallCard);
	QFont valueFont = overallValueLabel->font();
	valueFont.setBold(true);
	valueFont.setPointSizeF(valueFont.pointSizeF() * 2.2);
	overallValueLabel->setFont(valueFont);
	overallLayout->addWidget(overallValueLabel);

	verdictLabel = new QLabel(tr("Waiting for the engine"), overallCard);
	verdictLabel->setWordWrap(true);
	overallLayout->addWidget(verdictLabel);
	overallLayout->addSpacing(8);

	balanceBar = new BalanceBar(overallCard);
	overallCard->setVerdictLabel(verdictLabel);
	balanceBar->setToolTip(tr("Black's share on the left, white's on the right. The line in the middle means equal."));
	overallLayout->addWidget(balanceBar);
	layout->addWidget(overallCard);

	//one tug of war per criterion
	criteriaCard = new Card(tr("Why"), this);
	auto* criteriaLayout = new QVBoxLayout(criteriaCard);
	criteriaLayout->setContentsMargins(CARD_PADDING, CARD_PADDING + HEADER_HEIGHT, CARD_PADDING, CARD_PADDING);
	criteriaLayout->setSpacing(10);
	for (std::size_t index = 0; index < CRITERION_COUNT; ++index) {
		bars[index] = new CriterionBar(getCriterionName(static_cast<EvalCriterion>(index)), criteriaCard);
		criteriaLayout->addWidget(bars[index]);
	}
	layout->addWidget(criteriaCard);

	doorman.attach(*this);
}

void EvalCriteriaWidget::setOverallCentipawns(int centipawns) {
	const QString magnitude = QString::number(std::abs(centipawns) / 100.0, 'f', 2);
	const QString sign = centipawns > 0 ? "+" : centipawns < 0 ? MINUS_SIGN : "";
	overallValueLabel->setText(sign + magnitude);
	verdictLabel->setText(getVerdict(centipawns));
	const double share = 1.0 / (1.0 + std::exp(-WIN_CHANCE_SLOPE * centipawns));
	balanceBar->setShare(std::clamp(share, MIN_SHARE, 1 - MIN_SHARE));
}

void EvalCriteriaWidget::setOverallMate(int movesToMate) {
	overallValueLabel->setText("#" + (movesToMate < 0 ? MINUS_SIGN : QString()) + QString::number(std::abs(movesToMate)));
	const bool white = movesToMate > 0;
	if (movesToMate == 0) {
		verdictLabel->setText(tr("Checkmate"));
	} else {
		const int moves = std::abs(movesToMate);
		if (moves == 1) {
			verdictLabel->setText(white ? tr("White mates in 1 move") : tr("Black mates in 1 move"));
		} else {
			verdictLabel->setText((white ? tr("White mates in %1 moves") : tr("Black mates in %1 moves")).arg(moves));
		}
	}
	balanceBar->setShare(movesToMate == 0 ? 0.5 : white ? 1.0 : 0.0);
}

void EvalCriteriaWidget::clearOverall() {
	overallValueLabel->setText(DASH);
	verdictLabel->setText(tr("Waiting for the engine"));
	balanceBar->setShare(std::nullopt);
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
	overallCard->setColor(color);
	criteriaCard->setColor(color);
}
