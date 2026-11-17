#include "AnalysisSidebar.hpp"

#include <algorithm>
#include <cmath>

#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "EvalCriteriaWidget.hpp"
#include "EvalSquareWidget.hpp"
#include "KeyBindings.hpp"

namespace {

constexpr int SIDEBAR_WIDTH = 316;
constexpr int GAP_TO_BOARD = 12;
constexpr int SLIDE_DURATION = 240;
const char* const SHOWN_KEY = "layout/analysisSidebarShown";

} // !namespace

AnalysisSidebar::AnalysisSidebar(Doorman& doorman, KeyBindings& keyBindings, QWidget* parent)
	: QWidget(parent) {
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

	auto* column = new QWidget;
	column->setAutoFillBackground(false);
	auto* layout = new QVBoxLayout(column);
	layout->setContentsMargins(GAP_TO_BOARD, 0, 0, 0);
	layout->setSpacing(10);
	layout->addWidget(new EvalCriteriaWidget(doorman, column));
	layout->addWidget(new EvalSquareWidget(doorman, column));
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
