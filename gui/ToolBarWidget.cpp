#include "ToolBarWidget.hpp"

#include <functional>
#include <optional>

#include <QAction>
#include <QButtonGroup>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

#include "Doorman.hpp"
#include "KeyBindings.hpp"

namespace {

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

//takes the next key combination pressed while it has focus, Esc and Enter included
class KeyCaptureEdit : public QLineEdit {
public:
	explicit KeyCaptureEdit(QKeyCombination key, QWidget* parent)
		: QLineEdit(parent) {
		setReadOnly(true);
		setAlignment(Qt::AlignCenter);
		setKey(key);
	}

	void setKey(QKeyCombination newKey) {
		key = newKey;
		setText(KeyBindings::getKeyName(key));
		if (onChanged) onChanged();
	}

	QKeyCombination getKey() const {
		return key;
	}

	std::function<void()> onChanged;

protected:
	bool event(QEvent* event) override {
		if (event->type() == QEvent::KeyPress) {	//before Tab moves the focus
			const auto* keyEvent = static_cast<QKeyEvent*>(event);
			const int code = keyEvent->key();
			const bool modifierOnly = code == Qt::Key_Shift || code == Qt::Key_Control || code == Qt::Key_Alt
				|| code == Qt::Key_Meta || code == Qt::Key_AltGr || code == 0 || code == Qt::Key_unknown;
			if (!modifierOnly && !keyEvent->isAutoRepeat()) {
				setKey(QKeyCombination(keyEvent->modifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier), static_cast<Qt::Key>(code)));
			}
			return true;
		}
		if (event->type() == QEvent::ShortcutOverride) {
			event->accept();
			return true;
		}
		return QLineEdit::event(event);
	}

private:
	QKeyCombination key;
};

} // !namespace

ToolBarWidget::ToolBarWidget(Doorman& doorman, KeyBindings& keyBindings, QWidget* parent)
	: QToolBar(tr("Appearance"), parent)
	, doorman(doorman)
	, keyBindings(keyBindings) {
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

	addSeparator();
	auto* keyButton = new QToolButton(this);
	keyButton->setText(tr("Keys"));
	keyButton->setPopupMode(QToolButton::InstantPopup);
	auto* keyMenu = new QMenu(keyButton);
	keyMenu->setToolTipsVisible(true);
	for (std::size_t index = 0; index < KEY_ACTION_COUNT; ++index) {
		const auto action = static_cast<KeyAction>(index);
		keyActions[index] = keyMenu->addAction(QString());
		connect(keyActions[index], &QAction::triggered, this, [this, action] { editKey(action); });
	}
	keyMenu->addSeparator();
	connect(keyMenu->addAction(tr("Reset to defaults")), &QAction::triggered, this, [this] { this->keyBindings.resetToDefaults(); });
	keyButton->setMenu(keyMenu);
	addWidget(keyButton);
	connect(&keyBindings, &KeyBindings::changed, this, &ToolBarWidget::updateKeyTexts);
	updateKeyTexts();

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

void ToolBarWidget::editKey(KeyAction action) {
	QDialog dialog(this);
	dialog.setWindowTitle(KeyBindings::getActionName(action));
	auto* layout = new QVBoxLayout(&dialog);
	layout->setSpacing(10);

	layout->addWidget(new QLabel(tr("Click the field and press the new key:"), &dialog));
	auto* keyRow = new QHBoxLayout;
	auto* capture = new KeyCaptureEdit(keyBindings.getKey(action), &dialog);
	capture->setMinimumWidth(180);
	auto* clearButton = new QPushButton(tr("Unbind"), &dialog);
	clearButton->setAutoDefault(false);
	connect(clearButton, &QPushButton::clicked, &dialog, [capture] { capture->setKey(QKeyCombination()); });
	keyRow->addWidget(capture, 1);
	keyRow->addWidget(clearButton);
	layout->addLayout(keyRow);

	auto* conflictLabel = new QLabel(&dialog);
	conflictLabel->setWordWrap(true);
	conflictLabel->setStyleSheet("color: rgb(225, 130, 30);");
	layout->addWidget(conflictLabel);
	capture->onChanged = [this, capture, conflictLabel, action] {
		const std::optional<KeyAction> owner = keyBindings.findAction(capture->getKey());
		conflictLabel->setVisible(owner && *owner != action);
		if (owner && *owner != action) {
			conflictLabel->setText(tr("Already used by \"%1\", it will lose this key.").arg(KeyBindings::getActionName(*owner)));
		}
	};
	capture->onChanged();

	QRadioButton* holdButton = nullptr;
	QRadioButton* toggleButton = nullptr;
	if (KeyBindings::hasMode(action)) {
		holdButton = new QRadioButton(tr("Hold: shown while the key is held down"), &dialog);
		toggleButton = new QRadioButton(tr("Toggle: each press shows or hides it"), &dialog);
		auto* group = new QButtonGroup(&dialog);
		group->addButton(holdButton);
		group->addButton(toggleButton);
		(keyBindings.getMode(action) == KeyMode::HOLD ? holdButton : toggleButton)->setChecked(true);
		layout->addSpacing(4);
		layout->addWidget(holdButton);
		layout->addWidget(toggleButton);
	}

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, &dialog);
	for (QAbstractButton* button : buttons->buttons()) {
		if (auto* push = qobject_cast<QPushButton*>(button)) {
			push->setAutoDefault(false);	//Enter is a key to bind, not a confirmation
			push->setDefault(false);
		}
	}
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &dialog, [=] {
		capture->setKey(KeyBindings::getDefaultKey(action));
		if (holdButton) {
			(KeyBindings::getDefaultMode(action) == KeyMode::HOLD ? holdButton : toggleButton)->setChecked(true);
		}
	});
	layout->addWidget(buttons);

	capture->setFocus();
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	if (capture->getKey() != keyBindings.getKey(action)) {
		keyBindings.setKey(action, capture->getKey());
	}
	if (holdButton) {
		const KeyMode mode = holdButton->isChecked() ? KeyMode::HOLD : KeyMode::TOGGLE;
		if (mode != keyBindings.getMode(action)) {
			keyBindings.setMode(action, mode);
		}
	}
}

//"Move tree (hold)        M": the part after the tab is drawn like a shortcut, without becoming one
void ToolBarWidget::updateKeyTexts() {
	for (std::size_t index = 0; index < KEY_ACTION_COUNT; ++index) {
		const auto action = static_cast<KeyAction>(index);
		QString text = KeyBindings::getActionName(action);
		if (KeyBindings::hasMode(action)) {
			text += keyBindings.getMode(action) == KeyMode::HOLD ? tr(" (hold)") : tr(" (toggle)");
		}
		keyActions[index]->setText(text + QString::fromUtf8("…\t") + KeyBindings::getKeyName(keyBindings.getKey(action)));
	}
}
