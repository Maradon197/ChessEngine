#ifndef EVALSQUAREWIDGET_HPP
#define EVALSQUAREWIDGET_HPP

#include <QFrame>

class Doorman;

//Placeholder for the "eval square". Intentionally empty until its purpose is decided;
//add setters here and forward them through the Doorman like the other widgets.
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
