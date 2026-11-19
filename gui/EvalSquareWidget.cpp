#include "EvalSquareWidget.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

#include "Doorman.hpp"

namespace {

//the field is sampled at SAMPLES_PER_SQUARE points per square edge and scaled up with bilinear filtering, which keeps
//it smooth at any size; each gaussian is separable, so a square only costs one row and one column of weights plus
//their outer product, about a million multiply-adds for a full board, and only when new values arrive
constexpr int BOARD_SIZE = 8;
constexpr int SAMPLES_PER_SQUARE = 16;
constexpr int FIELD_SIZE = BOARD_SIZE * SAMPLES_PER_SQUARE;
//bigger values reach further: sigma = BASE_SIGMA * (1 + SPREAD_GROWTH * ln(1 + |value|)), in squares
//the logarithm keeps large scales (e.g. centipawns) from flooding the whole board
constexpr float BASE_SIGMA = 0.4f;
constexpr float SPREAD_GROWTH = 0.65f;
constexpr float MAX_SIGMA = 2.5f;
//the summed field is mapped to grey on a fixed scale, so absolute strength shows: share = 0.5 + 0.5 * s / (|s| + K), s = sum + tilt
//	OWNERSHIP_SCALE	K, the sum (in input units) that is halfway to full colour; the curve saturates slowly, so busy
//					middlegames keep their gradation while sparse endgames still show (middlegame sums reach ~150)
//	TILT_STRENGTH	T, uniform offset for the side that is ahead overall: tilt = T * balance (balance comes from the caller)
//					it mostly shows in quiet areas, strongly owned ones are already close to saturation
//values from several positions (e.g. along the engine's best line) can be blended before calling setValues: everything
//up to the curve is linear, so blending inputs is the same as blending fields
constexpr float OWNERSHIP_SCALE = 40.0f;
constexpr float TILT_STRENGTH = 18.0f;

constexpr qreal PADDING = 12;
constexpr qreal HEADER_HEIGHT = 22;
constexpr qreal LEGEND_HEIGHT = 26;
constexpr qreal CARD_RADIUS = 12;
constexpr qreal MAP_RADIUS = 6;
constexpr int GRID_ALPHA = 70;
constexpr int FADE_DURATION = 500;

const QColor BLACK_SIDE(30, 30, 30);
const QColor WHITE_SIDE(245, 245, 245);

//screen grey values are not perceived linearly: the same step looks bigger in some ranges than in others, which made
//black's areas look stronger than white for the same numbers. The value is mapped to CIE L* lightness
//instead (perceptually even, 0 = black, 100 = white) and converted to an sRGB grey from there.
//Tuning knobs:
//	MID_LIGHTNESS	the average lightness of grey in percent
//	BLACK_BOOST		> 1 darkens weak black areas sooner, < 1 later; the darkest point stays pure black
//	WHITE_BOOST		same for white
constexpr double MID_LIGHTNESS = 46;
constexpr double BLACK_BOOST = 1.0;
constexpr double WHITE_BOOST = 1.0;

uchar toPerceivedGrey(float share) {
	//lookup table over lightness, the expensive part is converting L* to sRGB
	static const std::array<uchar, 1024> table = [] {
		std::array<uchar, 1024> grey{};
		for (std::size_t index = 0; index < grey.size(); ++index) {
			const double lightness = 100.0 * index / (grey.size() - 1);
			const double luminance = lightness > 8 ? std::pow((lightness + 16) / 116, 3) : lightness / 903.3;
			const double encoded = luminance <= 0.0031308 ? 12.92 * luminance : 1.055 * std::pow(luminance, 1 / 2.4) - 0.055;
			grey[index] = static_cast<uchar>(std::lround(std::clamp(encoded, 0.0, 1.0) * 255));
		}
		return grey;
	}();
	const double signedShare = std::clamp(2.0 * share - 1.0, -1.0, 1.0);	//-1 darkest, 0 neutral, 1 brightest
	const double strength = std::abs(signedShare);
	const double lightness = signedShare < 0
		? MID_LIGHTNESS * (1 - std::pow(strength, 1 / BLACK_BOOST))
		: MID_LIGHTNESS + (100 - MID_LIGHTNESS) * std::pow(strength, 1 / WHITE_BOOST);
	const auto index = static_cast<std::size_t>(std::lround(lightness / 100 * (table.size() - 1)));
	return table[std::min(index, table.size() - 1)];
}

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
	fade = new QVariantAnimation(this);
	fade->setStartValue(0.0);
	fade->setEndValue(1.0);
	fade->setDuration(FADE_DURATION);
	fade->setEasingCurve(QEasingCurve::InOutQuad);
	connect(fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
		fadeProgress = value.toReal();
		update();
	});
	doorman.attach(*this);
}

void EvalSquareWidget::setValues(const SquareValues& values, float balance) {
	std::vector<float> sums(FIELD_SIZE * FIELD_SIZE, 0.0f);	//y = 0 is rank 1
	std::array<float, FIELD_SIZE> across{};	//weights along the rank
	std::array<float, FIELD_SIZE> along{};	//weights along the file
	for (int square = 0; square < BOARD_SIZE * BOARD_SIZE; ++square) {
		const float value = static_cast<float>(values[square]);
		if (value == 0) {
			continue;
		}
		const float sigma = std::min(MAX_SIGMA, BASE_SIGMA * (1 + SPREAD_GROWTH * std::log1p(std::abs(value))));
		const float falloff = -1 / (2 * sigma * sigma);
		const float centerX = square % BOARD_SIZE + 0.5f;
		const float centerY = square / BOARD_SIZE + 0.5f;
		for (int sample = 0; sample < FIELD_SIZE; ++sample) {
			const float position = (sample + 0.5f) / SAMPLES_PER_SQUARE;
			across[sample] = value * std::exp((position - centerX) * (position - centerX) * falloff);
			along[sample] = std::exp((position - centerY) * (position - centerY) * falloff);
		}
		for (int y = 0; y < FIELD_SIZE; ++y) {
			if (along[y] < 1e-4f) {
				continue;	//far outside this gaussian
			}
			float* line = &sums[y * FIELD_SIZE];
			for (int x = 0; x < FIELD_SIZE; ++x) {
				line[x] += along[y] * across[x];
			}
		}
	}

	//the old map fades out from what is on screen right now, even if the last fade hasn't finished
	if (hasValues) {
		if (fadeProgress < 1 && !previousField.isNull()) {
			for (int y = 0; y < FIELD_SIZE; ++y) {
				uchar* old = previousField.scanLine(y);
				const uchar* current = field.constScanLine(y);
				for (int x = 0; x < FIELD_SIZE; ++x) {
					old[x] = static_cast<uchar>(std::lround(old[x] + (current[x] - old[x]) * fadeProgress));
				}
			}
		} else {
			previousField = field.copy();
		}
	}

	//0 is mid grey; the overall balance tilts the whole map towards the side that is ahead
	const float tilt = TILT_STRENGTH * balance;
	field = QImage(FIELD_SIZE, FIELD_SIZE, QImage::Format_Grayscale8);
	for (int y = 0; y < FIELD_SIZE; ++y) {
		uchar* pixels = field.scanLine(FIELD_SIZE - 1 - y);	//rank 8 at the top
		for (int x = 0; x < FIELD_SIZE; ++x) {
			const float sum = sums[y * FIELD_SIZE + x] + tilt;
			pixels[x] = toPerceivedGrey(0.5f + 0.5f * sum / (std::abs(sum) + OWNERSHIP_SCALE));
		}
	}
	scaledField = QPixmap();
	scaledPrevious = QPixmap();
	if (hasValues) {
		fadeProgress = 0;
		fade->stop();
		fade->start();
	}
	hasValues = true;
	update();
}

void EvalSquareWidget::setOrientation(Color bottomColor) {
	if (orientation != bottomColor) {
		orientation = bottomColor;
		scaledField = QPixmap();
		scaledPrevious = QPixmap();
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
			scaledField = getScaled(field, target);
		}
		if (fadeProgress < 1) {
			if (scaledPrevious.isNull() || scaledPrevious.size() != target) {
				scaledPrevious = getScaled(previousField, target);
			}
			painter.drawPixmap(mapRect.topLeft(), scaledPrevious);
			painter.setOpacity(fadeProgress);
		}
		painter.drawPixmap(mapRect.topLeft(), scaledField);
		painter.setOpacity(1);
	} else {
		const int middle = toPerceivedGrey(0.5f);
		painter.fillRect(mapRect, QColor(middle, middle, middle));
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
	//paintLegend(painter, mapRect);
}

QPixmap EvalSquareWidget::getScaled(const QImage& image, const QSize& target) const {
	const QImage oriented = orientation == WHITE ? image : image.mirrored(true, true);
	QPixmap scaled = QPixmap::fromImage(oriented.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	scaled.setDevicePixelRatio(devicePixelRatioF());
	return scaled;
}

void EvalSquareWidget::paintGrid(QPainter& painter, const QRectF& mapRect) const {
	painter.setPen(QPen(QColor(128, 128, 128, GRID_ALPHA), 1));
	const qreal step = mapRect.width() / BOARD_SIZE;
	for (int line = 1; line < BOARD_SIZE; ++line) {
		painter.drawLine(QPointF(mapRect.left() + line * step, mapRect.top()), QPointF(mapRect.left() + line * step, mapRect.bottom()));
		painter.drawLine(QPointF(mapRect.left(), mapRect.top() + line * step), QPointF(mapRect.right(), mapRect.top() + line * step));
	}
}
#if 0
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
#endif //!if 0
