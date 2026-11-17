#include "EvalSquareWidget.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include "Doorman.hpp"

namespace {

//the field is sampled at SAMPLES_PER_SQUARE points per square edge and scaled up with bilinear filtering, which keeps
//it smooth at any size; the gaussian is separable, so the 64 kernels are two small matrix products instead of 64 passes
constexpr int BOARD_SIZE = 8;
constexpr int SAMPLES_PER_SQUARE = 16;
constexpr int FIELD_SIZE = BOARD_SIZE * SAMPLES_PER_SQUARE;
constexpr float SIGMA = 0.6f;	//in squares

constexpr qreal PADDING = 12;
constexpr qreal HEADER_HEIGHT = 22;
constexpr qreal LEGEND_HEIGHT = 26;
constexpr qreal CARD_RADIUS = 12;
constexpr qreal MAP_RADIUS = 6;
constexpr int GRID_ALPHA = 70;

const QColor BLACK_SIDE(30, 30, 30);
const QColor WHITE_SIDE(245, 245, 245);

qreal getMapSide(int width) {
	return std::max<qreal>(0, width - 2 * PADDING);
}

} // !namespace

EvalSquareWidget::EvalSquareWidget(Doorman& doorman, QWidget* parent)
	: QWidget(parent) {
	QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	policy.setHeightForWidth(true);
	setSizePolicy(policy);
	setToolTip(tr("Board control: the brighter an area, the more it belongs to white; the darker, the more to black."));
	doorman.attach(*this);
}

void EvalSquareWidget::setValues(const SquareValues& values) {
	//weights[square][sample]: how much a square at index i reaches sample s along one axis
	std::array<std::array<float, FIELD_SIZE>, BOARD_SIZE> weights{};
	for (int square = 0; square < BOARD_SIZE; ++square) {
		for (int sample = 0; sample < FIELD_SIZE; ++sample) {
			const float distance = (sample + 0.5f) / SAMPLES_PER_SQUARE - (square + 0.5f);
			weights[square][sample] = std::exp(-distance * distance / (2 * SIGMA * SIGMA));
		}
	}

	//spread along the files first, then along the ranks
	std::array<std::array<float, FIELD_SIZE>, BOARD_SIZE> rows{};
	for (int rank = 0; rank < BOARD_SIZE; ++rank) {
		for (int file = 0; file < BOARD_SIZE; ++file) {
			const float value = static_cast<float>(values[rank * BOARD_SIZE + file]);
			if (value == 0) {
				continue;
			}
			for (int x = 0; x < FIELD_SIZE; ++x) {
				rows[rank][x] += value * weights[file][x];
			}
		}
	}
	std::vector<float> sums(FIELD_SIZE * FIELD_SIZE, 0.0f);	//y = 0 is rank 1
	for (int y = 0; y < FIELD_SIZE; ++y) {
		float* line = &sums[y * FIELD_SIZE];
		for (int rank = 0; rank < BOARD_SIZE; ++rank) {
			const float weight = weights[rank][y];
			for (int x = 0; x < FIELD_SIZE; ++x) {
				line[x] += weight * rows[rank][x];
			}
		}
	}

	//0 is mid grey; positive values scale up to the highest point (white), negative ones down to the lowest (black)
	const auto [lowest, highest] = std::minmax_element(sums.begin(), sums.end());
	const float whiteScale = std::max(*highest, 0.0f);
	const float blackScale = std::max(-*lowest, 0.0f);
	field = QImage(FIELD_SIZE, FIELD_SIZE, QImage::Format_Grayscale8);
	for (int y = 0; y < FIELD_SIZE; ++y) {
		uchar* pixels = field.scanLine(FIELD_SIZE - 1 - y);	//rank 8 at the top
		for (int x = 0; x < FIELD_SIZE; ++x) {
			const float sum = sums[y * FIELD_SIZE + x];
			const float scale = sum > 0 ? whiteScale : blackScale;
			const float share = scale > 1e-6f ? 0.5f + 0.5f * sum / scale : 0.5f;
			pixels[x] = static_cast<uchar>(std::lround(share * 255));
		}
	}
	hasValues = true;
	scaledField = QPixmap();
	update();
}

void EvalSquareWidget::setOrientation(Color bottomColor) {
	if (orientation != bottomColor) {
		orientation = bottomColor;
		scaledField = QPixmap();
		update();
	}
}

void EvalSquareWidget::setPanelColor(const QColor& color) {
	panelColor = color;
	update();
}

bool EvalSquareWidget::hasHeightForWidth() const {
	return true;
}

int EvalSquareWidget::heightForWidth(int width) const {
	return static_cast<int>(std::ceil(PADDING + HEADER_HEIGHT + getMapSide(width) + LEGEND_HEIGHT + PADDING * 0.5));
}

QSize EvalSquareWidget::sizeHint() const {
	return {280, heightForWidth(280)};
}

QSize EvalSquareWidget::minimumSizeHint() const {
	return {140, heightForWidth(140)};
}

QRectF EvalSquareWidget::getMapRect() const {
	const qreal side = std::min(getMapSide(width()), height() - PADDING * 1.5 - HEADER_HEIGHT - LEGEND_HEIGHT);
	return {(width() - side) / 2, PADDING + HEADER_HEIGHT, side, side};
}

void EvalSquareWidget::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QPalette& colors = palette();

	QColor border = colors.color(QPalette::Mid);
	border.setAlpha(90);
	painter.setPen(QPen(border, 1));
	painter.setBrush(panelColor.isValid() ? panelColor : colors.color(QPalette::Window));
	painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), CARD_RADIUS, CARD_RADIUS);

	QFont headerFont = font();
	headerFont.setBold(true);
	headerFont.setCapitalization(QFont::AllUppercase);
	headerFont.setLetterSpacing(QFont::PercentageSpacing, 108);
	headerFont.setPointSizeF(headerFont.pointSizeF() * 0.85);
	painter.setFont(headerFont);
	QColor headerColor = colors.color(QPalette::Text);
	headerColor.setAlpha(150);
	painter.setPen(headerColor);
	painter.drawText(QRectF(PADDING, PADDING - 2, width() - 2 * PADDING, HEADER_HEIGHT), Qt::AlignLeft | Qt::AlignTop, tr("Board control"));

	const QRectF mapRect = getMapRect();
	if (mapRect.width() < 8) {
		return;
	}
	QPainterPath mapShape;
	mapShape.addRoundedRect(mapRect, MAP_RADIUS, MAP_RADIUS);

	painter.save();
	painter.setClipPath(mapShape);
	if (hasValues) {
		const qreal ratio = devicePixelRatioF();
		const QSize target = (mapRect.size() * ratio).toSize();
		if (scaledField.isNull() || scaledField.size() != target) {
			QImage oriented = orientation == WHITE ? field : field.mirrored(true, true);
			scaledField = QPixmap::fromImage(oriented.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
			scaledField.setDevicePixelRatio(ratio);
		}
		painter.drawPixmap(mapRect.topLeft(), scaledField);
	} else {
		painter.fillRect(mapRect, QColor(128, 128, 128));
	}
	paintGrid(painter, mapRect);
	painter.restore();

	painter.setPen(QPen(colors.color(QPalette::Text), 1));
	painter.setBrush(Qt::NoBrush);
	painter.drawPath(mapShape);

	if (!hasValues) {
		QFont dashFont = font();
		dashFont.setPointSizeF(dashFont.pointSizeF() * 2);
		painter.setFont(dashFont);
		painter.setPen(QColor(255, 255, 255, 170));
		painter.drawText(mapRect, Qt::AlignCenter, QString::fromUtf8("–"));
	}
	paintLegend(painter, mapRect);
}

void EvalSquareWidget::paintGrid(QPainter& painter, const QRectF& mapRect) const {
	painter.setPen(QPen(QColor(128, 128, 128, GRID_ALPHA), 1));
	const qreal step = mapRect.width() / BOARD_SIZE;
	for (int line = 1; line < BOARD_SIZE; ++line) {
		painter.drawLine(QPointF(mapRect.left() + line * step, mapRect.top()), QPointF(mapRect.left() + line * step, mapRect.bottom()));
		painter.drawLine(QPointF(mapRect.left(), mapRect.top() + line * step), QPointF(mapRect.right(), mapRect.top() + line * step));
	}
}

//black on the left, white on the right, like the criterion bars
void EvalSquareWidget::paintLegend(QPainter& painter, const QRectF& mapRect) const {
	const QPalette& colors = palette();
	const qreal stripHeight = 5;
	QFont labelFont = font();
	labelFont.setPointSizeF(labelFont.pointSizeF() * 0.85);
	painter.setFont(labelFont);
	const QFontMetricsF metrics(labelFont);
	const QString blackLabel = tr("Black");
	const QString whiteLabel = tr("White");
	const qreal top = mapRect.bottom() + 9;
	const qreal labelGap = 6;
	const QRectF strip(mapRect.left() + metrics.horizontalAdvance(blackLabel) + labelGap, top + (metrics.height() - stripHeight) / 2,
		mapRect.width() - metrics.horizontalAdvance(blackLabel) - metrics.horizontalAdvance(whiteLabel) - 2 * labelGap, stripHeight);

	QLinearGradient gradient(strip.topLeft(), strip.topRight());
	gradient.setColorAt(0, BLACK_SIDE);
	gradient.setColorAt(1, WHITE_SIDE);
	QColor outline = colors.color(QPalette::Text);
	outline.setAlpha(110);
	painter.setPen(QPen(outline, 0.8));
	painter.setBrush(gradient);
	painter.drawRoundedRect(strip, stripHeight / 2, stripHeight / 2);

	QColor labelColor = colors.color(QPalette::Text);
	labelColor.setAlpha(170);
	painter.setPen(labelColor);
	const QRectF labelRow(mapRect.left(), top, mapRect.width(), metrics.height());
	painter.drawText(labelRow, Qt::AlignLeft | Qt::AlignVCenter, blackLabel);
	painter.drawText(labelRow, Qt::AlignRight | Qt::AlignVCenter, whiteLabel);
}
