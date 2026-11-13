#include "Theme.hpp"

#include <QApplication>
#include <QPalette>
#include <QStyleHints>

namespace {

QPalette makePalette(bool dark) {
	const QColor window = dark ? QColor(45, 45, 48) : QColor(240, 240, 240);
	const QColor base = dark ? QColor(30, 30, 32) : QColor(255, 255, 255);
	const QColor alternateBase = dark ? QColor(56, 56, 60) : QColor(230, 230, 234);
	const QColor text = dark ? QColor(230, 230, 230) : QColor(20, 20, 20);
	const QColor button = dark ? QColor(60, 60, 64) : QColor(240, 240, 240);
	const QColor mid = dark ? QColor(95, 95, 100) : QColor(160, 160, 160);
	const QColor disabledText = dark ? QColor(120, 120, 120) : QColor(150, 150, 150);

	QPalette palette;
	palette.setColor(QPalette::Window, window);
	palette.setColor(QPalette::WindowText, text);
	palette.setColor(QPalette::Base, base);
	palette.setColor(QPalette::AlternateBase, alternateBase);
	palette.setColor(QPalette::Text, text);
	palette.setColor(QPalette::Button, button);
	palette.setColor(QPalette::ButtonText, text);
	palette.setColor(QPalette::ToolTipBase, dark ? button : QColor(255, 255, 220));
	palette.setColor(QPalette::ToolTipText, text);
	palette.setColor(QPalette::PlaceholderText, disabledText);
	palette.setColor(QPalette::Highlight, dark ? QColor(42, 130, 218) : QColor(48, 140, 198));
	palette.setColor(QPalette::HighlightedText, Qt::white);
	palette.setColor(QPalette::Link, dark ? QColor(90, 160, 240) : QColor(0, 90, 200));
	palette.setColor(QPalette::Light, dark ? QColor(80, 80, 85) : QColor(255, 255, 255));
	palette.setColor(QPalette::Midlight, dark ? QColor(70, 70, 74) : QColor(227, 227, 227));
	palette.setColor(QPalette::Mid, mid);
	palette.setColor(QPalette::Dark, dark ? QColor(25, 25, 27) : QColor(160, 160, 160));
	palette.setColor(QPalette::Shadow, dark ? QColor(15, 15, 15) : QColor(105, 105, 105));
	for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
		palette.setColor(QPalette::Disabled, role, disabledText);
	}
	return palette;
}

} // namespace

bool theme::isDark(ThemeMode mode) {
	switch (mode) {
		case ThemeMode::LIGHT: return false;
		case ThemeMode::DARK: return true;
		default: return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
	}
}

//Fusion is used for all modes because it follows the palette completely on every platform.
void theme::applyMode(ThemeMode mode) {
	static bool styleSet = false;
	if (!styleSet) {
		QApplication::setStyle("Fusion");
		styleSet = true;
	}
	QApplication::setPalette(makePalette(isDark(mode)));
}

ColorValue theme::getDefaultColor(ColorRole role, bool dark) {
	switch (role) {
		case ColorRole::LIGHT_SQUARE: return qRgba(240, 217, 181, 255);
		case ColorRole::DARK_SQUARE: return qRgba(181, 136, 99, 255);
		case ColorRole::LAST_MOVE: return qRgba(155, 199, 0, 105);
		case ColorRole::SELECTION: return qRgba(20, 85, 30, 128);
		case ColorRole::LEGAL_MOVE: return qRgba(20, 85, 30, 110);
		case ColorRole::USER_HIGHLIGHT: return qRgba(235, 97, 80, 170);
		case ColorRole::USER_ARROW: return qRgba(255, 170, 0, 150);
		case ColorRole::ENGINE_HIGHLIGHT: return qRgba(60, 130, 230, 140);
		case ColorRole::ENGINE_ARROW: return qRgba(40, 110, 220, 140);
		case ColorRole::EVAL_PANEL: return dark ? qRgba(38, 45, 58, 255) : qRgba(228, 235, 246, 255);
		case ColorRole::REVIEW_PANEL: return dark ? qRgba(54, 47, 38, 255) : qRgba(247, 240, 226, 255);
		case ColorRole::MOVES_PANEL: return dark ? qRgba(38, 52, 43, 255) : qRgba(229, 242, 232, 255);
		default: return qRgba(0, 0, 0, 255);
	}
}
