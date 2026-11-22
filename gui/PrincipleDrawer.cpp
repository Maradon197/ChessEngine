#include "PrincipleDrawer.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <utility>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStringList>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTextBrowser>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "Doorman.hpp"
#include "KeyBindings.hpp"

namespace {

constexpr qreal CARD_WIDTH = 268;
constexpr qreal CARD_HEIGHT = 292;
constexpr qreal CARD_GAP = 24;
constexpr qreal CARD_PADDING = 12;
constexpr qreal CARD_RADIUS = 12;
constexpr qreal IMAGE_HEIGHT = 118;
constexpr qreal GRID_MARGIN = 22;	//room for shadows
constexpr qreal SHADOW_MARGIN = 26;
constexpr qreal HOVER_LIFT = 3;
constexpr qreal USEFUL_DOT_RADIUS = 9;
constexpr qreal DISABLED_OPACITY = 0.4;
constexpr qreal CARD_ANIMATION_TIME = 90;	//ms

constexpr qreal DRAWER_SHARE = 0.66;
constexpr int DRAWER_MIN_HEIGHT = 420;
constexpr qreal DRAWER_RADIUS = 14;
constexpr int SLIDE_DURATION = 320;
constexpr int DETAILS_WIDTH = 360;
constexpr int DETAILS_IMAGE_HEIGHT = 190;

constexpr int ELO_MAX = 3000;
constexpr int ELO_STEP = 25;

const QColor NOT_USEFUL_COLOR(240, 140, 30);

QString getDifficultyName(PrincipleDifficulty difficulty) {
	switch (difficulty) {
		case PrincipleDifficulty::BEGINNER: return PrincipleDrawer::tr("Beginner");
		case PrincipleDifficulty::INTERMEDIATE: return PrincipleDrawer::tr("Intermediate");
		case PrincipleDifficulty::ADVANCED: return PrincipleDrawer::tr("Advanced");
		case PrincipleDifficulty::EXPERT: return PrincipleDrawer::tr("Expert");
		default: return {};
	}
}

QString getGamePartName(GamePart part) {
	switch (part) {
		case GamePart::OPENING: return PrincipleDrawer::tr("Opening");
		case GamePart::MIDGAME: return PrincipleDrawer::tr("Midgame");
		case GamePart::ATTACKING_MIDGAME: return PrincipleDrawer::tr("Attacking midgame");
		case GamePart::ENDGAME: return PrincipleDrawer::tr("Endgame");
		default: return {};
	}
}

QString getFocusName(PrincipleFocus focus) {
	switch (focus) {
		case PrincipleFocus::PAWN: return PrincipleDrawer::tr("Pawn");
		case PrincipleFocus::KNIGHT: return PrincipleDrawer::tr("Knight");
		case PrincipleFocus::BISHOP: return PrincipleDrawer::tr("Bishop");
		case PrincipleFocus::ROOK: return PrincipleDrawer::tr("Rook");
		case PrincipleFocus::QUEEN: return PrincipleDrawer::tr("Queen");
		case PrincipleFocus::KING: return PrincipleDrawer::tr("King");
		case PrincipleFocus::SPACE: return PrincipleDrawer::tr("Space");
		case PrincipleFocus::STRATEGY: return PrincipleDrawer::tr("Strategy");
		case PrincipleFocus::COORDINATION: return PrincipleDrawer::tr("Coordination");
		default: return {};
	}
}

//placeholder for principles without an image
QString getFocusGlyph(PrincipleFocus focus) {
	switch (focus) {
		case PrincipleFocus::PAWN: return QString::fromUtf8("♙");
		case PrincipleFocus::KNIGHT: return QString::fromUtf8("♘");
		case PrincipleFocus::BISHOP: return QString::fromUtf8("♗");
		case PrincipleFocus::ROOK: return QString::fromUtf8("♖");
		case PrincipleFocus::QUEEN: return QString::fromUtf8("♕");
		case PrincipleFocus::KING: return QString::fromUtf8("♔");
		case PrincipleFocus::SPACE: return QString::fromUtf8("▦");
		case PrincipleFocus::STRATEGY: return QString::fromUtf8("◎");
		case PrincipleFocus::COORDINATION: return QString::fromUtf8("⇄");
		default: return {};
	}
}

//all game parts of a principle in one chip text, e.g. "Opening · Midgame"
QString getGamePartNames(GamePartFlags parts) {
	QStringList names;
	for (int index = 0; index < static_cast<int>(GamePart::COUNT); ++index) {
		if (parts.has(static_cast<GamePart>(index))) {
			names.append(getGamePartName(static_cast<GamePart>(index)));
		}
	}
	return names.join(QString::fromUtf8(" | "));
}

//the placeholder image has room for one glyph only: the first focus in enum order
PrincipleFocus getFirstFocus(PrincipleFocusFlags focuses) {
	for (int index = 0; index < static_cast<int>(PrincipleFocus::COUNT); ++index) {
		if (focuses.has(static_cast<PrincipleFocus>(index))) {
			return static_cast<PrincipleFocus>(index);
		}
	}
	return PrincipleFocus::COUNT;
}

//same for focus
QString getFocusNames(PrincipleFocusFlags focuses) {
	QStringList names;
	for (int index = 0; index < static_cast<int>(PrincipleFocus::COUNT); ++index) {
		if (focuses.has(static_cast<PrincipleFocus>(index))) {
			names.append(getFocusName(static_cast<PrincipleFocus>(index)));
		}
	}
	return names.join(QString::fromUtf8(" | "));
}

//muted traffic light for the difficulty levels, one color each for the other two categories
QColor getDifficultyColor(PrincipleDifficulty difficulty) {
	switch (difficulty) {
		case PrincipleDifficulty::BEGINNER: return {92, 132, 186};
		case PrincipleDifficulty::INTERMEDIATE: return {96, 158, 104};
		case PrincipleDifficulty::ADVANCED: return {204, 168, 64};
		default: return {190, 88, 82};
	}
}

const QColor GAME_PART_COLOR(70, 140, 132);
const QColor FOCUS_COLOR(132, 104, 176);

struct Chip {
	QString text;
	QColor color;
};

std::array<Chip, 3> getChips(const Principle& principle) {
	const PrincipleDifficulty difficulty = getPrincipleDifficulty(principle.spotElo);
	return {{
		{getDifficultyName(difficulty), getDifficultyColor(difficulty)},
		{getGamePartNames(principle.gamePart), GAME_PART_COLOR},
		{getFocusNames(principle.focus), FOCUS_COLOR},
	}};
}

bool isDark(const QPalette& colors) {
	return colors.color(QPalette::Window).lightness() < 128;
}

QColor getChipTextColor(const QColor& color, bool dark) {
	return dark ? color.lighter(150) : color.darker(140);
}

QColor mix(const QColor& from, const QColor& to, qreal amount) {
	return QColor::fromRgbF(
		static_cast<float>(from.redF() + (to.redF() - from.redF()) * amount),
		static_cast<float>(from.greenF() + (to.greenF() - from.greenF()) * amount),
		static_cast<float>(from.blueF() + (to.blueF() - from.blueF()) * amount),
		static_cast<float>(from.alphaF() + (to.alphaF() - from.alphaF()) * amount));
}

QColor getOuterColor(const QColor& panel) {
	return panel.lightness() < 128 ? panel.darker(135) : panel.darker(109);
}

QString getNotUsefulText() {
	return PrincipleDrawer::tr("The engine can't use this principle yet");
}

//lays the chips out left to right with wrapping; only measures when paint is false; returns the height used
qreal layoutChips(QPainter& painter, const QRectF& area, const std::array<Chip, 3>& chips, bool dark, bool paint) {
	const QFontMetricsF metrics(painter.font());
	const qreal height = metrics.height() + 5;
	qreal x = area.left();
	qreal y = area.top();
	for (const Chip& chip : chips) {
		const qreal width = std::min(metrics.horizontalAdvance(chip.text) + 16, area.width());
		if (x > area.left() && x + width > area.right()) {
			x = area.left();
			y += height + 5;
		}
		if (paint) {
			const QRectF rect(x, y, width, height);
			QColor fill = chip.color;
			fill.setAlpha(dark ? 70 : 40);
			QColor border = chip.color;
			border.setAlpha(140);
			painter.setPen(QPen(border, 1));
			painter.setBrush(fill);
			painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), height / 2, height / 2);
			painter.setPen(getChipTextColor(chip.color, dark));
			painter.drawText(rect, Qt::AlignCenter, metrics.elidedText(chip.text, Qt::ElideRight, width - 10));
		}
		x += width + 5;
	}
	return y + height - area.top();
}

//word-wrapped text that ends with an ellipsis when it runs out of lines; returns the height used
qreal paintWrappedText(QPainter& painter, const QRectF& area, QString text, int maxLines) {
	text.replace('\n', ' ');
	const QFontMetricsF metrics(painter.font());
	const qreal lineHeight = metrics.lineSpacing();
	const int allowedLines = std::min(maxLines > 0 ? maxLines : INT_MAX, static_cast<int>(area.height() / lineHeight));
	if (allowedLines <= 0) {
		return 0;
	}

	QTextLayout layout(text, painter.font());
	QTextOption option;
	option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	layout.setTextOption(option);
	layout.beginLayout();
	qreal y = area.top();
	for (int lineCount = 0; lineCount < allowedLines; ++lineCount) {
		QTextLine line = layout.createLine();
		if (!line.isValid()) {
			break;
		}
		line.setLineWidth(area.width());
		const bool isLast = lineCount == allowedLines - 1;
		const QString lineText = isLast ? metrics.elidedText(text.mid(line.textStart()), Qt::ElideRight, area.width())
			: text.mid(line.textStart(), line.textLength());
		painter.drawText(QPointF(area.left(), y + metrics.ascent()), lineText);
		y += lineHeight;
	}
	layout.endLayout();
	return y - area.top();
}

//fills the rect like CSS "cover", or draws a gradient with a glyph when there is no image
void paintPrincipleImage(QPainter& painter, const QRectF& rect, const Principle& principle, const QPixmap& image, qreal radius) {
	painter.save();
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	QPainterPath clip;
	clip.addRoundedRect(rect, radius, radius);
	painter.setClipPath(clip);

	if (!image.isNull()) {
		const QSizeF scaled = QSizeF(image.size()).scaled(rect.size(), Qt::KeepAspectRatioByExpanding);
		const QRectF target(rect.center().x() - scaled.width() / 2, rect.center().y() - scaled.height() / 2, scaled.width(), scaled.height());
		painter.drawPixmap(target, image, QRectF(image.rect()));
	} else {
		QColor from = FOCUS_COLOR;
		from.setAlpha(170);
		QColor to = getDifficultyColor(getPrincipleDifficulty(principle.spotElo));
		to.setAlpha(130);
		QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
		gradient.setColorAt(0, from);
		gradient.setColorAt(1, to);
		painter.fillRect(rect, gradient);

		QFont glyphFont = painter.font();
		glyphFont.setPixelSize(static_cast<int>(rect.height() * 0.55));
		painter.setFont(glyphFont);
		painter.setPen(QColor(255, 255, 255, 215));
		painter.drawText(rect, Qt::AlignCenter, getFocusGlyph(getFirstFocus(principle.focus)));
	}
	painter.restore();
}

//soft drop shadow from stacked translucent rounded rects, rendered once and reused
QPixmap makeShadow(bool raised, qreal ratio) {
	const QSizeF size(CARD_WIDTH + 2 * SHADOW_MARGIN, CARD_HEIGHT + 2 * SHADOW_MARGIN);
	QPixmap shadow((size * ratio).toSize());
	shadow.setDevicePixelRatio(ratio);
	shadow.fill(Qt::transparent);

	QPainter painter(&shadow);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0, 0, 0, raised ? 7 : 5));
	const QRectF card(SHADOW_MARGIN, SHADOW_MARGIN, CARD_WIDTH, CARD_HEIGHT);
	const int layers = 14;
	const qreal offset = raised ? 9 : 5;
	const qreal step = raised ? 1.5 : 1.1;
	for (int layer = layers; layer >= 1; --layer) {
		const qreal spread = layer * step;
		painter.drawRoundedRect(card.translated(0, offset).adjusted(-spread, -spread * 0.5, spread, spread), CARD_RADIUS + spread, CARD_RADIUS + spread);
	}
	return shadow;
}

void paintEyeIcon(QPainter& painter, const QRectF& rect, const QColor& color) {
	painter.save();
	painter.setPen(QPen(color, 1.3));
	painter.setBrush(Qt::NoBrush);
	const QPointF left(rect.left(), rect.center().y());
	const QPointF right(rect.right(), rect.center().y());
	QPainterPath eye(left);
	eye.quadTo(rect.center().x(), rect.top() - rect.height() * 0.3, right.x(), right.y());
	eye.quadTo(rect.center().x(), rect.bottom() + rect.height() * 0.3, left.x(), left.y());
	painter.drawPath(eye);
	painter.setPen(Qt::NoPen);
	painter.setBrush(color);
	painter.drawEllipse(rect.center(), rect.height() * 0.24, rect.height() * 0.24);
	painter.restore();
}

//dark pill with an eye and the Elo from which the principle gets spotted
void paintEloBadge(QPainter& painter, const QPointF& bottomLeft, int spotElo, const QFont& font) {
	QFont badgeFont = font;
	badgeFont.setBold(true);
	const QFontMetricsF metrics(badgeFont);
	const QString text = QString::number(spotElo);
	const qreal height = metrics.height() + 4;
	const qreal iconWidth = height * 0.85;
	const QRectF badge(bottomLeft.x(), bottomLeft.y() - height, 7 + iconWidth + 5 + metrics.horizontalAdvance(text) + 8, height);

	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0, 0, 0, 135));
	painter.drawRoundedRect(badge, height / 2, height / 2);
	paintEyeIcon(painter, QRectF(badge.left() + 7, badge.center().y() - iconWidth * 0.3, iconWidth, iconWidth * 0.6), Qt::white);
	painter.setFont(badgeFont);
	painter.setPen(Qt::white);
	painter.drawText(badge.adjusted(7 + iconWidth + 5, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, text);
}

void paintUsefulDot(QPainter& painter, const QPointF& center, const QFont& font) {
	painter.setPen(QPen(Qt::white, 1.5));
	painter.setBrush(NOT_USEFUL_COLOR);
	painter.drawEllipse(center, USEFUL_DOT_RADIUS, USEFUL_DOT_RADIUS);
	QFont dotFont = font;
	dotFont.setBold(true);
	painter.setFont(dotFont);
	painter.setPen(Qt::white);
	painter.drawText(QRectF(center.x() - USEFUL_DOT_RADIUS, center.y() - USEFUL_DOT_RADIUS, 2 * USEFUL_DOT_RADIUS, 2 * USEFUL_DOT_RADIUS), Qt::AlignCenter, "!");
}

QString makeChipHtml(const Chip& chip, bool dark) {
	QColor fill = chip.color;
	fill.setAlpha(dark ? 70 : 40);
	return QString("<span style=\"background-color:rgba(%1,%2,%3,%4); color:%5;\">&nbsp;%6&nbsp;</span>")
		.arg(fill.red()).arg(fill.green()).arg(fill.blue()).arg(fill.alpha())
		.arg(getChipTextColor(chip.color, dark).name()).arg(chip.text.toHtmlEscaped());
}

} // !namespace

//wide slider for the target Elo: difficulty bands on the track, a dot per principle, the part above the handle is switched off
class EloSlider : public QWidget {
public:
	explicit EloSlider(QWidget* parent)
		: QWidget(parent) {
		setFixedHeight(76);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		setFocusPolicy(Qt::StrongFocus);
		setMouseTracking(true);
		setProperty("ownsArrowKeys", true);	//key bindings leave ←/→ alone while this has focus
		setToolTip(PrincipleDrawer::tr("Principles spotted at or below the target Elo are switched on, all above it off"));
	}

	std::function<void(int)> onValueChanged;

	int getValue() const {
		return value;
	}

	void setMarkers(std::vector<std::pair<int, bool>> newMarkers) {
		markers = std::move(newMarkers);
		update();
	}

protected:
	void paintEvent(QPaintEvent* event) override {
		Q_UNUSED(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		const QPalette& colors = palette();
		const QRectF track = getTrackRect();
		const qreal handleX = toX(value);

		//difficulty bands with their names below; the part above the target is greyed out (switched off)
		const std::array<int, 5> bounds = {0, 600, 1200, 1800, ELO_MAX};
		QPainterPath trackShape;
		trackShape.addRoundedRect(track, track.height() / 2, track.height() / 2);
		QFont labelFont = font();
		labelFont.setPointSizeF(labelFont.pointSizeF() * 0.88);
		painter.setFont(labelFont);
		for (std::size_t band = 0; band < 4; ++band) {
			const QRectF part(toX(bounds[band]), track.top(), toX(bounds[band + 1]) - toX(bounds[band]), track.height());
			const QColor color = getDifficultyColor(static_cast<PrincipleDifficulty>(band));
			painter.setPen(getChipTextColor(color, isDark(colors)));
			painter.drawText(QRectF(part.left(), track.bottom() + 9, part.width(), 18), Qt::AlignCenter, getDifficultyName(static_cast<PrincipleDifficulty>(band)));

			painter.save();
			painter.setClipPath(trackShape);
			painter.fillRect(part, mix(color, colors.color(QPalette::Window), 0.35));
			painter.restore();
		}
		QColor off = colors.color(QPalette::Mid);
		off.setAlpha(190);
		painter.save();
		painter.setClipPath(trackShape);
		painter.fillRect(QRectF(handleX, track.top(), track.right() - handleX, track.height()), off);
		painter.restore();
		painter.setBrush(Qt::NoBrush);
		painter.setPen(QPen(colors.color(QPalette::Mid), 1));
		painter.drawPath(trackShape);

		//one dot per principle
		for (const auto& [elo, enabled] : markers) {
			const QPointF center(toX(elo), track.top() - 9);
			painter.setPen(QPen(colors.color(QPalette::Base), 1));
			painter.setBrush(enabled ? colors.color(QPalette::Text) : colors.color(QPalette::Mid));
			painter.drawEllipse(center, 3.5, 3.5);
		}

		//handle with the value bubble above it
		const qreal radius = dragging ? 14 : hoveringHandle ? 13 : 12;
		const QPointF handle(handleX, track.center().y());
		painter.setPen(QPen(colors.color(QPalette::Highlight), 3));
		painter.setBrush(colors.color(QPalette::Base));
		painter.drawEllipse(handle, radius, radius);

		QFont bubbleFont = font();
		bubbleFont.setBold(true);
		painter.setFont(bubbleFont);
		const QString text = QString::number(value);
		const qreal bubbleWidth = QFontMetricsF(bubbleFont).horizontalAdvance(text) + 16;
		const qreal bubbleX = std::clamp(handleX - bubbleWidth / 2, 0.0, width() - bubbleWidth);
		const QRectF bubble(bubbleX, 0, bubbleWidth, 20);
		painter.setPen(Qt::NoPen);
		painter.setBrush(colors.color(QPalette::Highlight));
		painter.drawRoundedRect(bubble, 6, 6);
		painter.setPen(colors.color(QPalette::HighlightedText));
		painter.drawText(bubble, Qt::AlignCenter, text);

		if (hasFocus()) {
			painter.setPen(QPen(colors.color(QPalette::Highlight), 1, Qt::DotLine));
			painter.setBrush(Qt::NoBrush);
			painter.drawEllipse(handle, radius + 4, radius + 4);
		}
	}

	void mousePressEvent(QMouseEvent* event) override {
		if (event->button() == Qt::LeftButton) {
			dragging = true;
			setValue(fromX(event->position().x()));
			update();
		}
	}

	void mouseMoveEvent(QMouseEvent* event) override {
		if (dragging) {
			setValue(fromX(event->position().x()));
			return;
		}
		const bool onHandle = std::abs(event->position().x() - toX(value)) < 16 && std::abs(event->position().y() - getTrackRect().center().y()) < 16;
		if (onHandle != hoveringHandle) {
			hoveringHandle = onHandle;
			update();
		}
	}

	void mouseReleaseEvent(QMouseEvent* event) override {
		if (event->button() == Qt::LeftButton) {
			dragging = false;
			update();
		}
	}

	void leaveEvent(QEvent* event) override {
		Q_UNUSED(event);
		hoveringHandle = false;
		update();
	}

	void wheelEvent(QWheelEvent* event) override {
		const int notches = event->angleDelta().y() / 120;
		if (notches != 0) {
			setValue(value + notches * 4 * ELO_STEP);
		}
	}

	void keyPressEvent(QKeyEvent* event) override {
		switch (event->key()) {
			case Qt::Key_Left: setValue(value - ELO_STEP); break;
			case Qt::Key_Right: setValue(value + ELO_STEP); break;
			case Qt::Key_PageDown: setValue(value - 4 * ELO_STEP); break;
			case Qt::Key_PageUp: setValue(value + 4 * ELO_STEP); break;
			case Qt::Key_Home: setValue(0); break;
			case Qt::Key_End: setValue(ELO_MAX); break;
			default: QWidget::keyPressEvent(event);
		}
	}

private:
	QRectF getTrackRect() const {
		constexpr qreal side = 18;
		return {side, 38, width() - 2 * side, 10};
	}

	qreal toX(int elo) const {
		const QRectF track = getTrackRect();
		return track.left() + track.width() * elo / ELO_MAX;
	}

	int fromX(qreal x) const {
		const QRectF track = getTrackRect();
		const qreal elo = (x - track.left()) / track.width() * ELO_MAX;
		return static_cast<int>(std::lround(elo / ELO_STEP)) * ELO_STEP;
	}

	void setValue(int newValue) {
		newValue = std::clamp(newValue, 0, ELO_MAX);
		if (newValue == value) {
			return;
		}
		value = newValue;
		update();
		if (onValueChanged) {
			onValueChanged(value);
		}
	}

	int value = 600;
	bool dragging = false;
	bool hoveringHandle = false;
	std::vector<std::pair<int, bool>> markers;
};

//minimal rounded frame in the panel color behind the cards
class PrincipleFrame : public QWidget {
public:
	explicit PrincipleFrame(QWidget* parent)
		: QWidget(parent) {}

	void setColor(const QColor& newColor) {
		color = newColor;
		update();
	}

protected:
	void paintEvent(QPaintEvent* event) override {
		Q_UNUSED(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		QColor border = palette().color(QPalette::Mid);
		border.setAlpha(90);
		painter.setPen(QPen(border, 1));
		painter.setBrush(color.isValid() ? color : palette().color(QPalette::Window));
		painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 14, 14);
	}

private:
	QColor color;
};

class PrincipleGrid : public QWidget {
public:
	explicit PrincipleGrid(PrincipleDrawer& drawer)
		: drawer(drawer) {
		setMouseTracking(true);
		animationTimer = new QTimer(this);
		animationTimer->setInterval(16);
		connect(animationTimer, &QTimer::timeout, this, [this] { stepAnimations(); });
	}

	void refresh() {
		hoveredSlot = -1;
		hoveringReadMore = false;
		updateHeight();
		startAnimations();
	}

	void startAnimations() {
		if (!animationTimer->isActive()) {
			frameClock.start();
			animationTimer->start();
		}
		update();
	}

protected:
	bool event(QEvent* event) override {
		if (event->type() == QEvent::ToolTip) {
			auto* help = static_cast<QHelpEvent*>(event);
			const int slot = getSlotAt(help->pos());
			if (slot >= 0 && !drawer.principles[drawer.shown[slot]].useful && getUsefulDotRect(getCardRect(slot)).contains(help->pos())) {
				QToolTip::showText(help->globalPos(), getNotUsefulText(), this);
			} else {
				QToolTip::hideText();
			}
			return true;
		}
		return QWidget::event(event);
	}

	void resizeEvent(QResizeEvent* event) override {
		QWidget::resizeEvent(event);
		updateHeight();
	}

	void paintEvent(QPaintEvent* event) override {
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		if (drawer.shown.empty()) {
			painter.setPen(palette().color(QPalette::PlaceholderText));
			painter.drawText(rect(), Qt::AlignCenter, drawer.principles.empty()
				? PrincipleDrawer::tr("No principles registered.")
				: PrincipleDrawer::tr("No principles match the filters."));
			return;
		}
		updateShadows();
		const QRectF dirty = QRectF(event->rect()).adjusted(-SHADOW_MARGIN, -SHADOW_MARGIN, SHADOW_MARGIN, SHADOW_MARGIN);
		for (int slot = 0; slot < static_cast<int>(drawer.shown.size()); ++slot) {
			const QRectF card = getCardRect(slot);
			if (card.intersects(dirty)) {
				paintCard(painter, card, slot);
			}
		}
	}

	void mouseMoveEvent(QMouseEvent* event) override {
		const int slot = getSlotAt(event->position());
		const bool onReadMore = slot >= 0 && getReadMoreRect(getCardRect(slot)).contains(event->position());
		if (slot != hoveredSlot || onReadMore != hoveringReadMore) {
			hoveredSlot = slot;
			hoveringReadMore = onReadMore;
			setCursor(slot >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
			startAnimations();
		}
	}

	void leaveEvent(QEvent* event) override {
		Q_UNUSED(event);
		hoveredSlot = -1;
		hoveringReadMore = false;
		startAnimations();
	}

	//the whole card toggles the principle, "Read more" opens the details
	void mousePressEvent(QMouseEvent* event) override {
		if (event->button() != Qt::LeftButton) {
			return;
		}
		const int slot = getSlotAt(event->position());
		if (slot < 0) {
			return;
		}
		const int index = drawer.shown[slot];
		if (getReadMoreRect(getCardRect(slot)).contains(event->position())) {
			drawer.showDetails(index);
		} else {
			drawer.changeEnabled(index, !drawer.principles[index].enabled);
		}
	}

private:
	struct CardState {
		qreal hover = 0;
		qreal selected = 0;
		qreal enabled = 1;
	};

	CardState getTarget(int slot) const {
		const int index = drawer.shown[slot];
		return {slot == hoveredSlot ? 1.0 : 0.0, index == drawer.detailIndex ? 1.0 : 0.0, drawer.principles[index].enabled ? 1.0 : 0.0};
	}

	//current animated state; principles seen for the first time start at their target
	CardState& getState(int slot) const {
		const std::string& title = drawer.principles[drawer.shown[slot]].title;
		const auto found = states.find(title);
		if (found != states.end()) {
			return found->second;
		}
		return states.emplace(title, getTarget(slot)).first->second;
	}

	void stepAnimations() {
		const qreal elapsed = std::max<qint64>(frameClock.restart(), 1);
		const qreal blend = 1 - std::exp(-elapsed / CARD_ANIMATION_TIME);
		bool moving = false;
		const auto approach = [&](qreal& current, qreal target) {
			current += (target - current) * blend;
			if (std::abs(target - current) < 0.003) {
				current = target;
			} else {
				moving = true;
			}
		};
		for (int slot = 0; slot < static_cast<int>(drawer.shown.size()); ++slot) {
			CardState& state = getState(slot);
			const CardState target = getTarget(slot);
			approach(state.hover, target.hover);
			approach(state.selected, target.selected);
			approach(state.enabled, target.enabled);
		}
		if (!moving) {
			animationTimer->stop();
		}
		update();
	}

	void updateShadows() {
		const qreal ratio = devicePixelRatioF();
		if (restingShadow.isNull() || shadowRatio != ratio) {
			restingShadow = makeShadow(false, ratio);
			raisedShadow = makeShadow(true, ratio);
			shadowRatio = ratio;
		}
	}

	int getColumnCount() const {
		return std::max(1, static_cast<int>((width() - 2 * GRID_MARGIN + CARD_GAP) / (CARD_WIDTH + CARD_GAP)));
	}

	QRectF getCardRect(int slot) const {
		const int columns = getColumnCount();
		const qreal used = columns * CARD_WIDTH + (columns - 1) * CARD_GAP;
		const qreal left = std::max(GRID_MARGIN, (width() - used) / 2);
		const int row = slot / columns;
		const int column = slot % columns;
		return {left + column * (CARD_WIDTH + CARD_GAP), GRID_MARGIN + row * (CARD_HEIGHT + CARD_GAP), CARD_WIDTH, CARD_HEIGHT};
	}

	static QRectF getContentRect(const QRectF& card) {
		return card.adjusted(CARD_PADDING, CARD_PADDING, -CARD_PADDING, -CARD_PADDING);
	}

	static QRectF getUsefulDotRect(const QRectF& card) {
		const QPointF center(card.left() + CARD_PADDING + 8 + USEFUL_DOT_RADIUS, card.top() + CARD_PADDING + 8 + USEFUL_DOT_RADIUS);
		return {center.x() - USEFUL_DOT_RADIUS, center.y() - USEFUL_DOT_RADIUS, 2 * USEFUL_DOT_RADIUS, 2 * USEFUL_DOT_RADIUS};
	}

	QFont getSmallFont() const {
		QFont small = font();
		small.setPointSizeF(small.pointSizeF() * 0.9);
		return small;
	}

	static QString getReadMoreText() {
		return PrincipleDrawer::tr("Read more") + QString::fromUtf8(" ›");
	}

	QRectF getReadMoreRect(const QRectF& card) const {
		const QFontMetricsF metrics(getSmallFont());
		const QRectF content = getContentRect(card);
		const qreal height = metrics.height() + 2;
		return {content.left(), content.bottom() - height, metrics.horizontalAdvance(getReadMoreText()) + 4, height};
	}

	int getSlotAt(const QPointF& point) const {
		for (int slot = 0; slot < static_cast<int>(drawer.shown.size()); ++slot) {
			if (getCardRect(slot).contains(point)) {
				return slot;
			}
		}
		return -1;
	}

	void updateHeight() {
		const int columns = getColumnCount();
		const int rows = (static_cast<int>(drawer.shown.size()) + columns - 1) / columns;
		const qreal height = 2 * GRID_MARGIN + rows * CARD_HEIGHT + std::max(0, rows - 1) * CARD_GAP;
		setMinimumHeight(static_cast<int>(std::ceil(height)));
	}

	void paintCard(QPainter& painter, QRectF card, int slot) const {
		const int index = drawer.shown[slot];
		const Principle& principle = drawer.principles[index];
		const CardState& state = getState(slot);
		const QPalette& colors = palette();
		const bool dark = isDark(colors);

		card.translate(0, -HOVER_LIFT * state.hover);
		const QPointF shadowOrigin = card.topLeft() - QPointF(SHADOW_MARGIN, SHADOW_MARGIN);
		painter.setOpacity(1 - state.hover);
		painter.drawPixmap(shadowOrigin, restingShadow);
		painter.setOpacity(state.hover);
		painter.drawPixmap(shadowOrigin, raisedShadow);

		painter.setOpacity(1);
		const qreal emphasis = std::max(state.selected, state.hover * 0.45);
		painter.setPen(QPen(mix(colors.color(QPalette::Mid), colors.color(QPalette::Highlight), emphasis), 0.8 + 1.7 * state.selected));
		painter.setBrush(colors.color(QPalette::Base));
		painter.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), CARD_RADIUS, CARD_RADIUS);

		painter.setOpacity(DISABLED_OPACITY + (1 - DISABLED_OPACITY) * state.enabled);
		const QRectF content = getContentRect(card);
		const QRectF imageRect(content.left(), content.top(), content.width(), IMAGE_HEIGHT);
		paintPrincipleImage(painter, imageRect, principle, drawer.images[index], 8);
		paintEloBadge(painter, {imageRect.left() + 7, imageRect.bottom() - 7}, principle.spotElo, getSmallFont());
		qreal y = imageRect.bottom() + 10;

		QFont titleFont = font();
		titleFont.setBold(true);
		titleFont.setPointSizeF(titleFont.pointSizeF() * 1.18);
		painter.setFont(titleFont);
		painter.setPen(colors.color(QPalette::Text));
		y += paintWrappedText(painter, {content.left(), y, content.width(), content.bottom() - y}, QString::fromStdString(principle.title), 2) + 4;

		//bottom up: read more, tags, then the description fills what is left
		const QRectF readMore = getReadMoreRect(card);
		QFont smallFont = getSmallFont();
		painter.setFont(smallFont);
		const std::array<Chip, 3> chips = getChips(principle);
		const qreal chipsHeight = layoutChips(painter, {content.left(), 0, content.width(), 1000}, chips, dark, false);
		const qreal chipsTop = readMore.top() - 8 - chipsHeight;

		painter.setFont(font());
		painter.setPen(colors.color(QPalette::Text));
		paintWrappedText(painter, {content.left(), y, content.width(), chipsTop - 8 - y}, QString::fromStdString(principle.description), 0);

		painter.setFont(smallFont);
		layoutChips(painter, {content.left(), chipsTop, content.width(), chipsHeight}, chips, dark, true);

		smallFont.setUnderline(slot == hoveredSlot && hoveringReadMore);
		painter.setFont(smallFont);
		painter.setPen(colors.color(QPalette::Link));
		painter.drawText(readMore, Qt::AlignLeft | Qt::AlignVCenter, getReadMoreText());

		painter.setOpacity(1);
		if (!principle.useful) {
			paintUsefulDot(painter, getUsefulDotRect(card).center(), font());
		}
	}

	PrincipleDrawer& drawer;
	int hoveredSlot = -1;
	bool hoveringReadMore = false;

	QTimer* animationTimer = nullptr;
	QElapsedTimer frameClock;
	mutable std::unordered_map<std::string, CardState> states;	//by title, survives refreshes and filtering

	QPixmap restingShadow;
	QPixmap raisedShadow;
	qreal shadowRatio = 0;
};

PrincipleDrawer::PrincipleDrawer(Doorman& doorman, KeyBindings& keyBindings, QWidget& window)
	: QWidget(&window) {
	setFocusPolicy(Qt::StrongFocus);	//takes focus when opened so typing doesn't land in the search field
	hide();

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(18, 24, 18, 12);
	layout->setSpacing(10);
	layout->addWidget(createHeader());

	grid = new PrincipleGrid(*this);
	scrollArea = new QScrollArea;
	scrollArea->setWidget(grid);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->viewport()->setAutoFillBackground(false);	//shows the drawer's own background
	grid->setAutoFillBackground(false);	//setWidget() turns it on

	frame = new PrincipleFrame(this);
	auto* frameLayout = new QVBoxLayout(frame);
	frameLayout->setContentsMargins(4, 4, 4, 4);
	frameLayout->addWidget(scrollArea);

	auto* body = new QHBoxLayout;
	body->setSpacing(14);
	body->addWidget(frame, 1);
	body->addWidget(createDetails());
	layout->addLayout(body, 1);

	slide = new QVariantAnimation(this);
	connect(slide, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
		progress = value.toReal();
		updatePlacement();
	});
	connect(slide, &QVariantAnimation::finished, this, [this] {
		if (!isOpen) {
			hide();
		}
	});

	keyBindings.bindVisibility(KeyAction::PRINCIPLE_DRAWER, *this, [this] { return isOpen; }, [this](bool open) { setOpen(open); });
	connect(&keyBindings, &KeyBindings::pressed, this, [this](KeyAction action) {
		if (action == KeyAction::CLOSE_DRAWER) {
			closeStep();
		}
	});
	const auto updateCloseTip = [this, &keyBindings] {
		if (auto* closeButton = findChild<QToolButton*>("detailsCloseButton")) {
			const QKeyCombination key = keyBindings.getKey(KeyAction::CLOSE_DRAWER);
			closeButton->setToolTip(KeyBindings::isBound(key) ? tr("Close (%1)").arg(KeyBindings::getKeyName(key)) : tr("Close"));
		}
	};
	updateCloseTip();
	connect(&keyBindings, &KeyBindings::changed, this, updateCloseTip);

	window.installEventFilter(this);	//resizes
	qApp->installEventFilter(this);	//clicks above the drawer

	applyFilters();
	doorman.attach(*this);
}

QWidget* PrincipleDrawer::createHeader() {
	auto* header = new QWidget(this);
	auto* rows = new QVBoxLayout(header);
	rows->setContentsMargins(0, 0, 0, 0);
	rows->setSpacing(10);

	//the target Elo, the main control
	auto* eloRow = new QHBoxLayout;
	eloRow->setSpacing(18);
	auto* eloCaption = new QLabel(tr("Target Elo"), header);
	QFont captionFont = eloCaption->font();
	captionFont.setBold(true);
	captionFont.setPointSizeF(captionFont.pointSizeF() * 1.15);
	eloCaption->setFont(captionFont);
	eloRow->addWidget(eloCaption);
	eloSlider = new EloSlider(header);
	eloSlider->onValueChanged = [this](int value) { applyTargetElo(value); };
	eloRow->addWidget(eloSlider, 1);
	rows->addLayout(eloRow);

	//search, filters, bulk switches, file
	auto* bottom = new QHBoxLayout;
	bottom->setSpacing(8);
	searchEdit = new QLineEdit(header);
	searchEdit->setPlaceholderText(tr("Search…"));
	searchEdit->setClearButtonEnabled(true);
	searchEdit->setFixedWidth(200);
	bottom->addWidget(searchEdit);
	connect(searchEdit, &QLineEdit::textChanged, this, &PrincipleDrawer::applyFilters);

	const auto makeFilter = [this, header, bottom](const QString& anyText) {
		auto* box = new QComboBox(header);
		box->addItem(anyText, -1);
		bottom->addWidget(box);
		connect(box, &QComboBox::currentIndexChanged, this, &PrincipleDrawer::applyFilters);
		return box;
	};
	difficultyFilter = makeFilter(tr("Any difficulty"));
	for (int index = 0; index < static_cast<int>(PrincipleDifficulty::COUNT); ++index) {
		difficultyFilter->addItem(getDifficultyName(static_cast<PrincipleDifficulty>(index)), index);
	}
	gamePartFilter = makeFilter(tr("Any game part"));
	for (int index = 0; index < static_cast<int>(GamePart::COUNT); ++index) {
		gamePartFilter->addItem(getGamePartName(static_cast<GamePart>(index)), index);
	}
	focusFilter = makeFilter(tr("Any focus"));
	for (int index = 0; index < static_cast<int>(PrincipleFocus::COUNT); ++index) {
		focusFilter->addItem(getFocusName(static_cast<PrincipleFocus>(index)), index);
	}
	stateFilter = makeFilter(tr("Enabled and disabled"));
	stateFilter->addItem(tr("Enabled only"), 1);
	stateFilter->addItem(tr("Disabled only"), 0);
	bottom->addStretch(1);

	auto* enableButton = new QPushButton(tr("Enable shown"), header);
	auto* disableButton = new QPushButton(tr("Disable shown"), header);
	connect(enableButton, &QPushButton::clicked, this, [this] { setShownEnabled(true); });
	connect(disableButton, &QPushButton::clicked, this, [this] { setShownEnabled(false); });
	bottom->addWidget(enableButton);
	bottom->addWidget(disableButton);

	rows->addLayout(bottom);
	return header;
}

QWidget* PrincipleDrawer::createDetails() {
	detailsPanel = new QWidget(this);
	detailsPanel->setFixedWidth(DETAILS_WIDTH);
	detailsPanel->hide();
	auto* layout = new QVBoxLayout(detailsPanel);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(8);

	auto* titleRow = new QHBoxLayout;
	detailTitle = new QLabel(detailsPanel);
	detailTitle->setWordWrap(true);
	QFont titleFont = detailTitle->font();
	titleFont.setBold(true);
	titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
	detailTitle->setFont(titleFont);
	auto* closeButton = new QToolButton(detailsPanel);
	closeButton->setText(QString::fromUtf8("✕"));
	closeButton->setAutoRaise(true);
	closeButton->setObjectName("detailsCloseButton");	//tooltip names the current key, see the constructor
	connect(closeButton, &QToolButton::clicked, this, &PrincipleDrawer::hideDetails);
	titleRow->addWidget(detailTitle, 1);
	titleRow->addWidget(closeButton, 0, Qt::AlignTop);
	layout->addLayout(titleRow);

	detailImage = new QLabel(detailsPanel);
	detailImage->setFixedSize(DETAILS_WIDTH, DETAILS_IMAGE_HEIGHT);
	layout->addWidget(detailImage);

	detailMeta = new QLabel(detailsPanel);
	detailMeta->setTextFormat(Qt::RichText);
	detailMeta->setWordWrap(true);
	layout->addWidget(detailMeta);

	detailUseful = new QLabel(QString::fromUtf8("● ") + getNotUsefulText(), detailsPanel);
	detailUseful->setStyleSheet(QString("color: %1;").arg(NOT_USEFUL_COLOR.name()));
	layout->addWidget(detailUseful);

	detailDescription = new QLabel(detailsPanel);
	detailDescription->setWordWrap(true);
	layout->addWidget(detailDescription);

	detailEnabled = new QCheckBox(tr("Considered by the engine"), detailsPanel);
	connect(detailEnabled, &QCheckBox::toggled, this, [this](bool checked) {
		if (detailIndex >= 0) {
			changeEnabled(detailIndex, checked);
		}
	});
	layout->addWidget(detailEnabled);

	detailExplanation = new QTextBrowser(detailsPanel);
	detailExplanation->setOpenExternalLinks(true);
	layout->addWidget(detailExplanation, 1);
	layout->addStretch(0);	//keeps the rest at the top when there is no explanation
	return detailsPanel;
}

void PrincipleDrawer::setPrinciples(const std::vector<Principle>& newPrinciples) {
	const std::string openTitle = detailIndex >= 0 ? principles[detailIndex].title : std::string{};

	principles = newPrinciples;
	std::stable_sort(principles.begin(), principles.end(), [](const Principle& first, const Principle& second) {
		return first.spotElo < second.spotElo;
	});

	const int targetElo = eloSlider->getValue();
	for (Principle& principle : principles) {
		if (knownTitles.insert(principle.title).second) {	//first time this title shows up: the target Elo decides
			const bool enabled = principle.spotElo <= targetElo;
			if (principle.enabled != enabled) {
				principle.enabled = enabled;
				emit principleEnabledChanged(principle.title, enabled);
			}
		}
	}

	images.clear();
	for (const Principle& principle : principles) {
		QPixmap image;
		if (!principle.imagePath.empty() && !image.load(QString::fromStdString(principle.imagePath))) {
			qWarning("PrincipleDrawer: could not load image \"%s\"", principle.imagePath.c_str());
		}
		images.push_back(image);
	}

	const int reopened = findIndex(openTitle);	//keep the details open across a refresh
	if (reopened >= 0) {
		showDetails(reopened);
	} else {
		hideDetails();
	}
	applyFilters();
}

void PrincipleDrawer::setPrincipleEnabled(const std::string& title, bool enabled) {
	const int index = findIndex(title);
	if (index < 0 || principles[index].enabled == enabled) {
		return;
	}
	principles[index].enabled = enabled;
	if (index == detailIndex) {
		updateDetails();
	}
	applyFilters();
}

void PrincipleDrawer::setPrincipleUseful(const std::string& title, bool useful) {
	const int index = findIndex(title);
	if (index < 0 || principles[index].useful == useful) {
		return;
	}
	principles[index].useful = useful;
	if (index == detailIndex) {
		updateDetails();
	}
	grid->update();
}

void PrincipleDrawer::setPanelColor(const QColor& color) {
	panelColor = color;
	frame->setColor(color);
	update();
}

bool PrincipleDrawer::eventFilter(QObject* watched, QEvent* event) {
	if (watched == parentWidget() && event->type() == QEvent::Resize) {
		updatePlacement();
		return false;
	}
	if (!isOpen || event->type() != QEvent::MouseButtonPress) {
		return false;
	}
	auto* receiver = qobject_cast<QWidget*>(watched);
	if (receiver == nullptr || receiver->window() != window() || receiver == this || isAncestorOf(receiver)) {
		return false;	//popups, dialogs and the drawer itself handle their own input
	}

	for (QWidget* widget = receiver; widget != nullptr; widget = widget->parentWidget()) {
		if (widget->inherits("MoveTreeOverlay") || widget->inherits("TutorialOverlay")) {
			return false;	//those overlays lie above the drawer and handle their own clicks
		}
	}
	const QPoint point = mapFromGlobal(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
	if (point.y() < 0) {
		setOpen(false);	//clicked above the drawer: close it and swallow the click
		return true;
	}
	return false;
}

void PrincipleDrawer::closeStep() {
	if (!isOpen) {
		return;
	}
	if (detailIndex >= 0) {
		hideDetails();
	} else {
		setOpen(false);
	}
}

void PrincipleDrawer::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const QColor background = getOuterColor(panelColor.isValid() ? panelColor : palette().color(QPalette::Window));
	QPainterPath shape;
	shape.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, DRAWER_RADIUS), DRAWER_RADIUS, DRAWER_RADIUS);	//bottom corners hang below the window
	painter.setPen(QPen(palette().color(QPalette::Mid), 1));
	painter.setBrush(background);
	painter.drawPath(shape);

	painter.setPen(Qt::NoPen);
	painter.setBrush(palette().color(QPalette::Mid));
	painter.drawRoundedRect(QRectF(width() / 2.0 - 22, 8, 44, 5), 2.5, 2.5);	//handle
}

void PrincipleDrawer::mousePressEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton && event->position().y() < 20) {
		setOpen(false);	//clicked the handle
	}
}

void PrincipleDrawer::setOpen(bool open) {
	if (isOpen == open) {
		return;
	}
	isOpen = open;
	if (open) {
		updatePlacement();
		show();
		setFocus();
	} else if (QWidget* focused = QApplication::focusWidget(); focused != nullptr && (focused == this || isAncestorOf(focused))) {
		focused->clearFocus();
	}

	const qreal target = open ? 1 : 0;
	slide->stop();
	slide->setEasingCurve(open ? QEasingCurve::OutQuart : QEasingCurve::InOutCubic);
	slide->setStartValue(progress);
	slide->setEndValue(target);
	slide->setDuration(std::max(1, static_cast<int>(SLIDE_DURATION * std::abs(target - progress))));
	slide->start();
}

void PrincipleDrawer::updatePlacement() {
	const QRect area = parentWidget()->rect();
	const int height = std::clamp(static_cast<int>(area.height() * DRAWER_SHARE), std::min(DRAWER_MIN_HEIGHT, area.height()), area.height());
	setGeometry(0, area.height() - static_cast<int>(std::lround(height * progress)), area.width(), height);
}

void PrincipleDrawer::applyFilters() {
	const QString needle = searchEdit->text().trimmed();
	const int difficulty = difficultyFilter->currentData().toInt();
	const int gamePart = gamePartFilter->currentData().toInt();
	const int focus = focusFilter->currentData().toInt();
	const int state = stateFilter->currentData().toInt();
	const auto contains = [&needle](const std::string& text) {
		return QString::fromStdString(text).contains(needle, Qt::CaseInsensitive);
	};

	shown.clear();
	for (int index = 0; index < static_cast<int>(principles.size()); ++index) {
		const Principle& principle = principles[index];
		if ((difficulty >= 0 && static_cast<int>(getPrincipleDifficulty(principle.spotElo)) != difficulty)
			|| (gamePart >= 0 && !principle.gamePart.has(static_cast<GamePart>(gamePart)))
			|| (focus >= 0 && !principle.focus.has(static_cast<PrincipleFocus>(focus)))
			|| (state >= 0 && principle.enabled != (state == 1))) {
			continue;
		}
		if (!needle.isEmpty() && !contains(principle.title) && !contains(principle.description) && !contains(principle.explanation)) {
			continue;
		}
		shown.push_back(index);
	}
	grid->refresh();
	updateMarkers();
}

void PrincipleDrawer::updateMarkers() {
	std::vector<std::pair<int, bool>> markers;
	for (const Principle& principle : principles) {
		markers.emplace_back(principle.spotElo, principle.enabled);
	}
	eloSlider->setMarkers(std::move(markers));
}

void PrincipleDrawer::showDetails(int index) {
	detailIndex = index;
	updateDetails();
	detailsPanel->show();
	grid->startAnimations();
}

void PrincipleDrawer::hideDetails() {
	detailIndex = -1;
	detailsPanel->hide();
	grid->startAnimations();
}

void PrincipleDrawer::updateDetails() {
	if (detailIndex < 0) {
		return;
	}
	const Principle& principle = principles[detailIndex];
	const bool dark = isDark(palette());
	detailTitle->setText(QString::fromStdString(principle.title));

	const qreal ratio = devicePixelRatioF();
	QPixmap canvas(QSize(DETAILS_WIDTH, DETAILS_IMAGE_HEIGHT) * ratio);
	canvas.setDevicePixelRatio(ratio);
	canvas.fill(Qt::transparent);
	{
		QPainter painter(&canvas);
		painter.setRenderHint(QPainter::Antialiasing);
		paintPrincipleImage(painter, QRectF(0, 0, DETAILS_WIDTH, DETAILS_IMAGE_HEIGHT), principle, images[detailIndex], 10);
	}
	detailImage->setPixmap(canvas);

	QString meta;
	for (const Chip& chip : getChips(principle)) {
		meta += makeChipHtml(chip, dark) + "&nbsp; ";
	}
	meta += "<br>" + tr("Spotted from about %1 Elo (chess.com rapid)").arg(principle.spotElo);
	detailMeta->setText(meta);
	detailUseful->setVisible(!principle.useful);
	detailDescription->setText(QString::fromStdString(principle.description));

	const QSignalBlocker blocker(detailEnabled);
	detailEnabled->setChecked(principle.enabled);

	detailExplanation->setMarkdown(QString::fromStdString(principle.explanation));
	detailExplanation->setVisible(!principle.explanation.empty());
}

void PrincipleDrawer::changeEnabled(int index, bool enabled) {
	if (principles[index].enabled == enabled) {
		return;
	}
	principles[index].enabled = enabled;
	emit principleEnabledChanged(principles[index].title, enabled);
	if (index == detailIndex) {
		updateDetails();
	}
	applyFilters();
}

void PrincipleDrawer::setShownEnabled(bool enabled) {
	const std::vector<int> targets = shown;	//applyFilters() rebuilds shown on every change
	for (int index : targets) {
		changeEnabled(index, enabled);
	}
}

//everything spotted at or below the target is switched on, everything above off
void PrincipleDrawer::applyTargetElo(int targetElo) {
	for (Principle& principle : principles) {
		const bool enabled = principle.spotElo <= targetElo;
		if (principle.enabled != enabled) {
			principle.enabled = enabled;
			emit principleEnabledChanged(principle.title, enabled);
		}
	}
	updateDetails();
	applyFilters();
}

int PrincipleDrawer::findIndex(const std::string& title) const {
	for (int index = 0; index < static_cast<int>(principles.size()); ++index) {
		if (principles[index].title == title) {
			return index;
		}
	}
	return -1;
}
