#include "EvalSquareWidget.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <QFontMetricsF>
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
constexpr qreal SCORE_HEIGHT = 34;	//the score tab under the map
constexpr qreal SCORE_SPACE = SCORE_HEIGHT + 6;
constexpr qreal SCORE_MIN_WIDTH = 104;
constexpr int SCORE_EASE_DURATION = 260;
//same curve lichess uses for its eval bar: centipawns to white's winning share; capped so full colour means mate
constexpr double WIN_CHANCE_SLOPE = 0.00368208;
constexpr double MIN_SHARE = 0.04;
const QString MINUS_SIGN = QString::fromUtf8("−");
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
	scoreText = QString::fromUtf8("–");
	scoreEase = new QVariantAnimation(this);
	scoreEase->setDuration(SCORE_EASE_DURATION);
	scoreEase->setEasingCurve(QEasingCurve::OutCubic);
	connect(scoreEase, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
		scoreShare = value.toReal();
		update();
	});
	doorman.attach(*this);
}

void EvalSquareWidget::setOverallCentipawns(int centipawns) {
	const QString magnitude = QString::number(std::abs(centipawns) / 100.0, 'f', 2);
	const QString sign = centipawns > 0 ? "+" : centipawns < 0 ? MINUS_SIGN : "";
	const double share = 1.0 / (1.0 + std::exp(-WIN_CHANCE_SLOPE * centipawns));
	setScore(sign + magnitude, std::clamp(share, MIN_SHARE, 1 - MIN_SHARE));
}

void EvalSquareWidget::setOverallMate(int movesToMate) {
	const QString text = movesToMate == 0 ? QStringLiteral("#")
		: "#" + (movesToMate < 0 ? MINUS_SIGN : QString()) + QString::number(std::abs(movesToMate));
	setScore(text, movesToMate == 0 ? 0.5 : movesToMate > 0 ? 1.0 : 0.0);
}

void EvalSquareWidget::clearOverall() {
	scoreEase->stop();
	hasScore = false;
	scoreShare = 0.5;
	scoreText = QString::fromUtf8("–");
	update();
}

void EvalSquareWidget::setScore(const QString& text, qreal share) {
	scoreEase->stop();
	scoreEase->setStartValue(hasScore ? scoreShare : 0.5);
	scoreEase->setEndValue(share);
	scoreEase->start();
	hasScore = true;
	scoreText = text;
	setToolTip(tr("Board control: the brighter an area, the more it belongs to white; the darker, the more to black.\n"
		"Below it the overall score, %1 from white's view, shaded the same way.").arg(text));
	update();
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
	return static_cast<int>(std::ceil(PADDING + HEADER_HEIGHT + getMapSide(width) + SCORE_SPACE + PADDING * 0.5));
}

QSize EvalSquareWidget::sizeHint() const {
	return {280, heightForWidth(280)};
}

QSize EvalSquareWidget::minimumSizeHint() const {
	return {140, heightForWidth(140)};
}

QRectF EvalSquareWidget::getMapRect() const {
	const qreal side = std::min(getMapSide(width()), height() - PADDING * 1.5 - HEADER_HEIGHT - SCORE_SPACE);
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

	//the score tab hangs from the map's bottom edge: square top corners, one outline around both
	const QRectF scoreRect = getScoreRect(mapRect);
	QPainterPath tabShape;
	tabShape.addRoundedRect(scoreRect, MAP_RADIUS, MAP_RADIUS);
	QPainterPath tabTop;
	tabTop.addRect(scoreRect.adjusted(0, -2, 0, -scoreRect.height() / 2));
	tabShape = tabShape.united(tabTop);
	const int scoreGrey = toPerceivedGrey(static_cast<float>(scoreShare));
	const QColor scoreFill = hasScore ? QColor(scoreGrey, scoreGrey, scoreGrey) : colors.color(QPalette::Base);
	painter.setPen(Qt::NoPen);
	painter.setBrush(scoreFill);
	painter.drawPath(tabShape);
	QFont scoreFont = font();
	scoreFont.setBold(true);
	scoreFont.setPointSizeF(scoreFont.pointSizeF() * 1.45);
	painter.setFont(scoreFont);
	painter.setPen(!hasScore ? colors.color(QPalette::Text) : scoreGrey > 140 ? QColor(25, 25, 28) : QColor(238, 238, 240));
	painter.drawText(scoreRect.adjusted(0, 1, 0, 0), Qt::AlignCenter, scoreText);

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
	painter.drawPath(mapShape.united(tabShape));

	if (!hasValues) {
		QFont dashFont = font();
		dashFont.setPointSizeF(dashFont.pointSizeF() * 2);
		painter.setFont(dashFont);
		painter.setPen(QColor(255, 255, 255, 170));
		painter.drawText(mapRect, Qt::AlignCenter, QString::fromUtf8("–"));
	}
}

QPixmap EvalSquareWidget::getScaled(const QImage& image, const QSize& target) const {
	const QImage oriented = orientation == WHITE ? image : image.mirrored(true, true);
	QPixmap scaled = QPixmap::fromImage(oriented.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	scaled.setDevicePixelRatio(devicePixelRatioF());
	return scaled;
}

//centered under the map, as wide as the score needs
QRectF EvalSquareWidget::getScoreRect(const QRectF& mapRect) const {
	QFont scoreFont = font();
	scoreFont.setBold(true);
	scoreFont.setPointSizeF(scoreFont.pointSizeF() * 1.45);
	const qreal width = std::min(mapRect.width(), std::max(SCORE_MIN_WIDTH, QFontMetricsF(scoreFont).horizontalAdvance(scoreText) + 32));
	return {mapRect.center().x() - width / 2, mapRect.bottom() - 0.5, width, SCORE_HEIGHT};
}

void EvalSquareWidget::paintGrid(QPainter& painter, const QRectF& mapRect) const {
	painter.setPen(QPen(QColor(128, 128, 128, GRID_ALPHA), 1));
	const qreal step = mapRect.width() / BOARD_SIZE;
	for (int line = 1; line < BOARD_SIZE; ++line) {
		painter.drawLine(QPointF(mapRect.left() + line * step, mapRect.top()), QPointF(mapRect.left() + line * step, mapRect.bottom()));
		painter.drawLine(QPointF(mapRect.left(), mapRect.top() + line * step), QPointF(mapRect.right(), mapRect.top() + line * step));
	}
}
