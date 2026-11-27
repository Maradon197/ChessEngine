#include "SearchSettingsWidget.hpp"

#include <algorithm>

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

#include "GuiTypes.hpp"

namespace {

QString getPlyText(int plies) {
	return plies == 1 ? QObject::tr("1 ply") : QObject::tr("%1 plies").arg(plies);
}

QString getQuiescenceText(int plies) {
	return plies == 0 ? QObject::tr("off") : QObject::tr("+%1").arg(getPlyText(plies));
}

} // !namespace

SearchSettingsWidget::SearchSettingsWidget(const QString& depthCaption, const QString& depthTip, const QString& prefix,
	int defaultDepth, QWidget* parent)
	: QWidget(parent)
	, settingsPrefix(prefix) {
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	const QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
	depth = std::clamp(settings.value(settingsPrefix + "/depth", defaultDepth).toInt(), MIN_SEARCH_DEPTH, MAX_SEARCH_DEPTH);
	quiescenceDepth = std::clamp(settings.value(settingsPrefix + "/quiescence", DEFAULT_QUIESCENCE_DEPTH).toInt(),
		MIN_QUIESCENCE_DEPTH, MAX_QUIESCENCE_DEPTH);

	QSlider* depthSlider = addSlider(depthCaption, depthTip, MIN_SEARCH_DEPTH, MAX_SEARCH_DEPTH, depthValue);
	depthSlider->setValue(depth);
	depthValue->setText(getPlyText(depth));
	connect(depthSlider, &QSlider::valueChanged, this, [this](int value) {
		depth = value;
		depthValue->setText(getPlyText(value));
		QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(settingsPrefix + "/depth", value);
		emit depthChanged(value);
	});

	layout->addSpacing(6);
	QSlider* quiescenceSlider = addSlider(tr("Captures after that"),
		tr("Behind the horizon the engine keeps playing out captures for up to this many half-moves, so it doesn't stop "
			"counting in the middle of an exchange (quiescence search). 0 switches it off."),
		MIN_QUIESCENCE_DEPTH, MAX_QUIESCENCE_DEPTH, quiescenceValue);
	quiescenceSlider->setValue(quiescenceDepth);
	quiescenceValue->setText(getQuiescenceText(quiescenceDepth));
	connect(quiescenceSlider, &QSlider::valueChanged, this, [this](int value) {
		quiescenceDepth = value;
		quiescenceValue->setText(getQuiescenceText(value));
		QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(settingsPrefix + "/quiescence", value);
		emit quiescenceDepthChanged(value);
	});
}

//caption and value in one row, the slider below; the value label is handed back
QSlider* SearchSettingsWidget::addSlider(const QString& caption, const QString& tip, int minimum, int maximum, QLabel*& value) {
	auto* layout = static_cast<QVBoxLayout*>(this->layout());
	auto* row = new QHBoxLayout;
	auto* captionLabel = new QLabel(caption, this);
	QFont captionFont = captionLabel->font();
	captionFont.setBold(true);
	captionLabel->setFont(captionFont);
	value = new QLabel(this);
	row->addWidget(captionLabel);
	row->addStretch(1);
	row->addWidget(value);
	layout->addLayout(row);

	auto* slider = new QSlider(Qt::Horizontal, this);
	slider->setRange(minimum, maximum);
	slider->setPageStep(1);
	slider->setTickPosition(QSlider::TicksBelow);
	slider->setTickInterval(1);
	slider->setFocusPolicy(Qt::NoFocus);	//the arrow keys stay with the key bindings
	slider->setCursor(Qt::PointingHandCursor);
	slider->setToolTip(tip);
	captionLabel->setToolTip(tip);
	layout->addWidget(slider);
	return slider;
}
