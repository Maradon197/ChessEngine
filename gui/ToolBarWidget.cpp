#include "ToolBarWidget.hpp"

#include <QAction>
#include <QColorDialog>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QToolButton>

#include "Doorman.hpp"

namespace {

const char* const SETTINGS_ORGANIZATION = "ChessEngineGUI";
const char* const SETTINGS_APPLICATION = "ChessEngineGUI";
const char* const THEME_MODE_KEY = "appearance/themeMode";

QString getColorKey(ColorRole role) {
	return QString("appearance/colors/%1").arg(static_cast<int>(role));
}

QString getRoleName(ColorRole role) {
	switch (role) {
		case ColorRole::LIGHT_SQUARE: return QObject::tr("Light squares");
		case ColorRole::DARK_SQUARE: return QObject::tr("Dark squares");
		case ColorRole::LAST_MOVE: return QObject::tr("Last move");
		case ColorRole::SELECTION: return QObject::tr("Selected square");
		case ColorRole::LEGAL_MOVE: return QObject::tr("Legal move hints");
		case ColorRole::USER_HIGHLIGHT: return QObject::tr("My highlights");
		case ColorRole::USER_ARROW: return QObject::tr("My arrows");
		case ColorRole::ENGINE_HIGHLIGHT: return QObject::tr("Engine highlights");
		case ColorRole::ENGINE_ARROW: return QObject::tr("Engine arrows");
		case ColorRole::EVAL_PANEL: return QObject::tr("Evaluation panel");
		case ColorRole::MOVES_PANEL: return QObject::tr("Moves panel");
		case ColorRole::PRINCIPLES_PANEL: return QObject::tr("Principles drawer");
		default: return {};
	}
}

QIcon makeSwatch(const QColor& color) {
	QPixmap pixmap(14, 14);
	pixmap.fill(Qt::white);	//keeps transparent colors visible
	QPainter painter(&pixmap);
	painter.fillRect(pixmap.rect(), color);
	painter.setPen(QColor(90, 90, 90));
	painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));
	return QIcon(pixmap);
}

} // !namespace

ToolBarWidget::ToolBarWidget(Doorman& doorman, QWidget* parent)
	: QToolBar(tr("Appearance"), parent)
	, doorman(doorman) {
	setMovable(false);

	addWidget(new QLabel(tr(" Theme: "), this));
	themeBox = new QComboBox(this);
	themeBox->setFocusPolicy(Qt::ClickFocus);	//no implicit keyboard focus at startup
	themeBox->addItem(tr("System"), static_cast<int>(ThemeMode::SYSTEM));
	themeBox->addItem(tr("Light"), static_cast<int>(ThemeMode::LIGHT));
	themeBox->addItem(tr("Dark"), static_cast<int>(ThemeMode::DARK));
	addWidget(themeBox);

	auto* colorButton = new QToolButton(this);
	colorButton->setText(tr("Colors"));
	colorButton->setPopupMode(QToolButton::InstantPopup);
	auto* colorMenu = new QMenu(colorButton);
	for (std::size_t index = 0; index < COLOR_ROLE_COUNT; ++index) {
		const auto role = static_cast<ColorRole>(index);
		if (role == ColorRole::EVAL_PANEL) {
			colorMenu->addSeparator();
		}
		colorActions[index] = colorMenu->addAction(getRoleName(role) + QString::fromUtf8("…"));
		connect(colorActions[index], &QAction::triggered, this, [this, role] { chooseColor(role); });
	}
	colorMenu->addSeparator();
	connect(colorMenu->addAction(tr("Reset to defaults")), &QAction::triggered, this, &ToolBarWidget::resetColors);
	colorButton->setMenu(colorMenu);
	addWidget(colorButton);

	loadSettings();

	connect(themeBox, &QComboBox::currentIndexChanged, this, [this] {
		const int mode = themeBox->currentData().toInt();
		QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(THEME_MODE_KEY, mode);
		this->doorman.setThemeMode(static_cast<ThemeMode>(mode));
		updateSwatches();
	});
}

void ToolBarWidget::loadSettings() {
	const QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);

	const int modeIndex = themeBox->findData(settings.value(THEME_MODE_KEY, static_cast<int>(ThemeMode::SYSTEM)).toInt());
	themeBox->setCurrentIndex(modeIndex < 0 ? 0 : modeIndex);
	doorman.setThemeMode(static_cast<ThemeMode>(themeBox->currentData().toInt()));

	for (std::size_t index = 0; index < COLOR_ROLE_COUNT; ++index) {
		const auto role = static_cast<ColorRole>(index);
		const QVariant stored = settings.value(getColorKey(role));
		if (stored.isValid()) {
			doorman.setColor(role, stored.toUInt());
		}
	}
	updateSwatches();
}

void ToolBarWidget::chooseColor(ColorRole role) {
	const QColor current = QColor::fromRgba(doorman.getColor(role));
	const QColor chosen = QColorDialog::getColor(current, this, getRoleName(role), QColorDialog::ShowAlphaChannel);
	if (!chosen.isValid()) {
		return;
	}
	QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION).setValue(getColorKey(role), chosen.rgba());
	doorman.setColor(role, chosen.rgba());
	updateSwatches();
}

void ToolBarWidget::resetColors() {
	QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
	for (std::size_t index = 0; index < COLOR_ROLE_COUNT; ++index) {
		settings.remove(getColorKey(static_cast<ColorRole>(index)));
	}
	doorman.resetColors();
	updateSwatches();
}

void ToolBarWidget::updateSwatches() {
	for (std::size_t index = 0; index < COLOR_ROLE_COUNT; ++index) {
		colorActions[index]->setIcon(makeSwatch(QColor::fromRgba(doorman.getColor(static_cast<ColorRole>(index)))));
	}
}
