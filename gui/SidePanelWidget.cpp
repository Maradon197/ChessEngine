#include "SidePanelWidget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

#include "Doorman.hpp"

namespace {

constexpr int TOGGLE_WIDTH = 18;
constexpr int CONTENT_MIN_WIDTH = 200;

} // !namespace

SidePanelWidget::SidePanelWidget(Doorman& doorman, QWidget* parent)
	: QWidget(parent) {
	auto* layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	toggleButton = new QToolButton(this);
	toggleButton->setAutoRaise(true);
	toggleButton->setFixedWidth(TOGGLE_WIDTH);
	toggleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	connect(toggleButton, &QToolButton::clicked, this, [this] { setCollapsed(!collapsed); });

	contentArea = new QWidget(this);
	contentArea->setMinimumWidth(CONTENT_MIN_WIDTH);
	sectionLayout = new QVBoxLayout(contentArea);
	sectionLayout->addStretch(1);

	layout->addWidget(toggleButton);
	layout->addWidget(contentArea);

	setCollapsed(true);
	doorman.attach(*this);
}

void SidePanelWidget::addSection(const QString& title, QWidget* content) {
	auto* titleLabel = new QLabel(title, contentArea);
	QFont titleFont = titleLabel->font();
	titleFont.setBold(true);
	titleLabel->setFont(titleFont);

	content->setParent(contentArea);
	sectionLayout->insertWidget(sectionLayout->count() - 1, titleLabel);
	sectionLayout->insertWidget(sectionLayout->count() - 1, content);
}

void SidePanelWidget::setCollapsed(bool newCollapsed) {
	collapsed = newCollapsed;
	contentArea->setVisible(!collapsed);
	if (collapsed) {
		setFixedWidth(TOGGLE_WIDTH);
	} else {
		setMinimumWidth(TOGGLE_WIDTH + CONTENT_MIN_WIDTH);
		setMaximumWidth(QWIDGETSIZE_MAX);
	}
	toggleButton->setArrowType(collapsed ? Qt::LeftArrow : Qt::RightArrow);
	toggleButton->setToolTip(collapsed ? tr("Show analysis overlays") : tr("Hide analysis overlays"));
}

bool SidePanelWidget::isCollapsed() const {
	return collapsed;
}
