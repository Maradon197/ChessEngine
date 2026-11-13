#ifndef EVALSQUAREWIDGET_HPP
#define EVALSQUAREWIDGET_HPP

#include <QFrame>

class Doorman;

class EvalSquareWidget : public QFrame {
	Q_OBJECT

public:
	explicit EvalSquareWidget(Doorman& doorman, QWidget* parent = nullptr);

	void setPanelColor(const QColor& color);

	bool hasHeightForWidth() const override;
	int heightForWidth(int width) const override;
	QSize sizeHint() const override;
};

#endif
