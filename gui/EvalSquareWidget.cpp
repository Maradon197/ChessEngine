#include "EvalSquareWidget.hpp"

#include "Doorman.hpp"

EvalSquareWidget::EvalSquareWidget(Doorman& doorman, QWidget* parent)
	: QFrame(parent) {
	setFrameShape(QFrame::StyledPanel);
	setAutoFillBackground(true);
	QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	policy.setHeightForWidth(true);
	setSizePolicy(policy);
	doorman.attach(*this);
}

void EvalSquareWidget::setPanelColor(const QColor& color) {
	QPalette colors = palette();
	colors.setColor(QPalette::Window, color);
	setPalette(colors);
}

bool EvalSquareWidget::hasHeightForWidth() const {
	return true;
}

int EvalSquareWidget::heightForWidth(int width) const {
	return width;
}

QSize EvalSquareWidget::sizeHint() const {
	return {160, 160};
}
