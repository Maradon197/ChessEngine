#include "AnalysisSidebar.hpp"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "EvalCriteriaWidget.hpp"
#include "EvalSquareWidget.hpp"
#include "SearchStatusLine.hpp"
#include "KeyBindings.hpp"

namespace {

constexpr int SIDEBAR_WIDTH = 316;
constexpr int GAP_TO_BOARD = 12;
constexpr int SLIDE_DURATION = 240;
const char* const SHOWN_KEY = "layout/analysisSidebarShown";
const char* const DEPTH_KEY = "engine/analysisDepth";
const char* const ARROW_KEY = "engine/showArrow";
constexpr int DEFAULT_ANALYSIS_DEPTH = 4;

QString getDepthText(int depth) {
	return depth == 1 ? QObject::tr("1 ply") : QObject::tr("%1 plies").arg(depth);
}

} // !namespace

AnalysisSidebar::AnalysisSidebar(Doorman& doorman, KeyBindings& keyBindings, QWidget* parent)
	: QWidget(parent) {
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

	auto* column = new QWidget;
	column->setAutoFillBackground(false);
	auto* layout = new QVBoxLayout(column);
	layout->setContentsMargins(GAP_TO_BOARD, 0, 0, 0);
	layout->setSpacing(10);
	//how deep the engine analyses, like the play view's setting but its own
	auto* depthSettings = new QWidget(column);
	auto* depthLayout = new QVBoxLayout(depthSettings);
	depthLayout->setContentsMargins(0, 0, 0, 0);
	depthLayout->setSpacing(4);
	auto* depthRow = new QHBoxLayout;
	auto* depthCaption = new QLabel(tr("Analysis depth"), depthSettings);
	QFont captionFont = depthCaption->font();
	captionFont.setBold(true);
	depthCaption->setFont(captionFont);
	depthValue = new QLabel(depthSettings);
	depthRow->addWidget(depthCaption);
	depthRow->addStretch(1);
	depthRow->addWidget(depthValue);
	depthLayout->addLayout(depthRow);
	auto* depthSlider = new QSlider(Qt::Horizontal, depthSettings);
	depthSlider->setRange(MIN_SEARCH_DEPTH, MAX_SEARCH_DEPTH);
	depthSlider->setPageStep(1);
	depthSlider->setTickPosition(QSlider::TicksBelow);
	depthSlider->setTickInterval(1);
	depthSlider->setFocusPolicy(Qt::NoFocus);	//the arrow keys stay with the key bindings
	depthSlider->setCursor(Qt::PointingHandCursor);
	depthSlider->setToolTip(tr("How many half-moves the engine searches for the score and its arrow. Deeper is more exact, but slower."));
	depthLayout->addWidget(depthSlider);
	const int storedDepth = QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).value(DEPTH_KEY, DEFAULT_ANALYSIS_DEPTH).toInt();
	searchDepth = std::clamp(storedDepth, MIN_SEARCH_DEPTH, MAX_SEARCH_DEPTH);
	depthSlider->setValue(searchDepth);
	depthValue->setText(getDepthText(searchDepth));
	connect(depthSlider, &QSlider::valueChanged, this, &AnalysisSidebar::setSearchDepth);

	engineArrow = new QCheckBox(tr("Show the engine's arrow"), depthSettings);
	engineArrow->setFocusPolicy(Qt::NoFocus);
	engineArrow->setCursor(Qt::PointingHandCursor);
	QFont smallFont = engineArrow->font();
	smallFont.setPointSizeF(smallFont.pointSizeF() * 0.9);
	engineArrow->setFont(smallFont);
	engineArrow->setChecked(QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).value(ARROW_KEY, true).toBool());
	connect(engineArrow, &QCheckBox::toggled, this, [this](bool shown) {
		QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(ARROW_KEY, shown);
		emit engineArrowShownChanged(shown);
	});
	depthLayout->addWidget(engineArrow);

	searchStatus = new SearchStatusLine(depthSettings);
	depthLayout->addSpacing(2);
	depthLayout->addWidget(searchStatus);

	layout->addWidget(new EvalCriteriaWidget(doorman, column));
	layout->addWidget(new EvalSquareWidget(doorman, column));
	layout->addWidget(depthSettings);
	layout->addStretch(1);

	content = new QScrollArea(this);
	content->setFrameShape(QFrame::NoFrame);
	content->setFocusPolicy(Qt::NoFocus);
	content->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	content->setWidgetResizable(true);
	content->viewport()->setAutoFillBackground(false);
	content->setWidget(column);

	slide = new QVariantAnimation(this);
	connect(slide, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
		progress = value.toReal();
		applyProgress();
	});

	//held: hidden until the key goes down; toggled: as the user left it
	const bool startShown = keyBindings.getMode(KeyAction::TOGGLE_SIDEBAR) == KeyMode::TOGGLE
		&& QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).value(SHOWN_KEY, true).toBool();
	shown = startShown;
	progress = startShown ? 1 : 0;
	applyProgress();

	keyBindings.bindVisibility(KeyAction::TOGGLE_SIDEBAR, *this, [this] { return isShown(); },
		[this, &keyBindings](bool show) {
			setShown(show);
			if (keyBindings.getMode(KeyAction::TOGGLE_SIDEBAR) == KeyMode::TOGGLE) {
				QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(SHOWN_KEY, show);
			}
		});
}

void AnalysisSidebar::setShown(bool show) {
	if (shown == show) {
		return;
	}
	shown = show;
	const qreal target = show ? 1 : 0;
	slide->stop();
	slide->setEasingCurve(show ? QEasingCurve::OutCubic : QEasingCurve::InOutCubic);
	slide->setStartValue(progress);
	slide->setEndValue(target);
	slide->setDuration(std::max(1, static_cast<int>(SLIDE_DURATION * std::abs(target - progress))));
	slide->start();
}

bool AnalysisSidebar::isShown() const {
	return shown;
}

int AnalysisSidebar::getSearchDepth() const {
	return searchDepth;
}

bool AnalysisSidebar::isEngineArrowShown() const {
	return engineArrow->isChecked();
}

void AnalysisSidebar::setSearchRecorder(SearchRecorder& recorder) {
	searchStatus->setRecorder(recorder);
}

void AnalysisSidebar::setSearchStatusShown(bool shown) {
	searchStatus->setVisible(shown);
}

void AnalysisSidebar::setSearchDepth(int depth) {
	searchDepth = depth;
	depthValue->setText(getDepthText(depth));
	QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(DEPTH_KEY, depth);
	emit searchDepthChanged(depth);
}

void AnalysisSidebar::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	content->setGeometry(width() - SIDEBAR_WIDTH, 0, SIDEBAR_WIDTH, height());	//anchored right: slides out over the window edge
}

void AnalysisSidebar::applyProgress() {
	const int visibleWidth = static_cast<int>(std::lround(SIDEBAR_WIDTH * progress));
	setFixedWidth(visibleWidth);
	setVisible(visibleWidth > 0);
	content->setGeometry(visibleWidth - SIDEBAR_WIDTH, 0, SIDEBAR_WIDTH, height());
}
