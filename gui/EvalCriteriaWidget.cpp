#include "EvalCriteriaWidget.hpp"

#include <algorithm>
#include <cstdlib>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include "Doorman.hpp"

namespace {

const QColor WHITE_ADVANTAGE_COLOR(245, 245, 245);
const QColor BLACK_ADVANTAGE_COLOR(45, 45, 45);

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
		case EvalCriterion::WEIGHTED_MATERIAL: return QObject::tr("Weighted material");
		case EvalCriterion::KING_SAFETY: return QObject::tr("King safety");
		case EvalCriterion::PIECE_ACTIVITY: return QObject::tr("Piece activity");
		case EvalCriterion::SPACE: return QObject::tr("Space");
		default: return {};
	}
}

} // namespace

//Segmented bar: CRITERION_MIN..-1 to the left of the center line, 1..CRITERION_MAX to the right.
class CriterionBar : public QWidget {
public:
	explicit CriterionBar(QWidget* parent)
		: QWidget(parent) {
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	void setValue(std::optional<int> newValue) {
		value = newValue;
		update();
	}

	std::optional<int> getValue() const {
		return value;
	}

	QSize sizeHint() const override {
		return {150, 18};
	}

	QSize minimumSizeHint() const override {
		return {100, 18};
	}

protected:
	void paintEvent(QPaintEvent* event) override {
		Q_UNUSED(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		const QPalette& colors = palette();

		const int textWidth = fontMetrics().horizontalAdvance("+3") + 8;
		const QRectF barRect(0.5, 2.5, width() - textWidth - 1.0, height() - 5.0);
		const int segmentCount = CRITERION_MAX - CRITERION_MIN;	//zero has no segment
		const qreal gap = 2.0;
		const qreal segmentWidth = (barRect.width() - gap * (segmentCount - 1)) / segmentCount;

		for (int index = 0; index < segmentCount; ++index) {
			const int segmentValue = index < -CRITERION_MIN ? index + CRITERION_MIN : index + CRITERION_MIN + 1;
			const bool filled = value
				&& ((segmentValue < 0 && *value <= segmentValue) || (segmentValue > 0 && *value >= segmentValue));
			const QRectF segment(barRect.left() + index * (segmentWidth + gap), barRect.top(), segmentWidth, barRect.height());

			if (filled) {
				painter.setPen(QPen(colors.color(QPalette::Text), 1));
				painter.setBrush(segmentValue > 0 ? WHITE_ADVANTAGE_COLOR : BLACK_ADVANTAGE_COLOR);
			} else {
				painter.setPen(QPen(colors.color(QPalette::Mid), 1));
				painter.setBrush(colors.color(QPalette::Window).darker(112));
			}
			painter.drawRoundedRect(segment, 2, 2);
		}

		const qreal centerX = barRect.left() + -CRITERION_MIN * (segmentWidth + gap) - gap / 2;
		painter.setPen(QPen(colors.color(QPalette::Text), 1.5));
		painter.drawLine(QPointF(centerX, 0), QPointF(centerX, height()));

		painter.setPen(colors.color(QPalette::Text));
		const QRectF textRect(width() - textWidth, 0, textWidth, height());
		painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, value ? formatSigned(*value) : DASH);
	}

private:
	std::optional<int> value;
};

EvalCriteriaWidget::EvalCriteriaWidget(Doorman& doorman, QWidget* parent)
	: QWidget(parent) {
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(4);
	setAutoFillBackground(true);

	auto* overallRow = new QHBoxLayout();
	auto* overallLabel = new QLabel(tr("Overall"), this);
	QFont boldFont = overallLabel->font();
	boldFont.setBold(true);
	overallLabel->setFont(boldFont);

	overallValueLabel = new QLabel(DASH, this);
	QFont valueFont = boldFont;
	valueFont.setPointSizeF(boldFont.pointSizeF() * 1.6);
	overallValueLabel->setFont(valueFont);
	overallValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

	overallRow->addWidget(overallLabel);
	overallRow->addWidget(overallValueLabel, 1);
	layout->addLayout(overallRow);
	layout->addSpacing(6);

	for (std::size_t index = 0; index < CRITERION_COUNT; ++index) {
		layout->addWidget(new QLabel(getCriterionName(static_cast<EvalCriterion>(index)), this));
		bars[index] = new CriterionBar(this);
		layout->addWidget(bars[index]);
	}

	layout->addStretch(1);

	doorman.attach(*this);
}

void EvalCriteriaWidget::setOverallCentipawns(int centipawns) {
	const QString magnitude = QString::number(std::abs(centipawns) / 100.0, 'f', 2);
	const QString sign = centipawns > 0 ? "+" : centipawns < 0 ? MINUS_SIGN : "";
	overallValueLabel->setText(sign + magnitude);
}

void EvalCriteriaWidget::setOverallMate(int movesToMate) {
	overallValueLabel->setText("#" + (movesToMate < 0 ? MINUS_SIGN : QString()) + QString::number(std::abs(movesToMate)));
}

void EvalCriteriaWidget::clearOverall() {
	overallValueLabel->setText(DASH);
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
	QPalette colors = palette();
	colors.setColor(QPalette::Window, color);
	setPalette(colors);
}
