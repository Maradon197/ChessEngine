#ifndef EVALSQUAREWIDGET_HPP
#define EVALSQUAREWIDGET_HPP

#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QWidget>

#include "DataTypes.hpp"
#include "GuiTypes.hpp"

class Doorman;
class QVariantAnimation;

//smooth map of who owns which part of the board: every square spreads its value as a gaussian around its center,
//the summed field is drawn from black (lowest point) to white (highest point)
class EvalSquareWidget : public QWidget {
	Q_OBJECT

public:
	explicit EvalSquareWidget(Doorman& doorman, QWidget* parent = nullptr);

	//balance: who is ahead overall, 0 = even, about +-1 = clearly winning for white / black, larger values are allowed
	void setValues(const SquareValues& values, float balance = 0);
	void setOrientation(Color bottomColor);
	void setPanelColor(const QColor& color);

	bool hasHeightForWidth() const override;
	int heightForWidth(int width) const override;
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	QRectF getMapRect() const;
	void paintGrid(QPainter& painter, const QRectF& mapRect) const;

	QPixmap getScaled(const QImage& image, const QSize& target) const;

	QImage field;	//grayscale, rank 8 at the top, built once per setValues
	QImage previousField;	//faded out while field fades in
	mutable QPixmap scaledField;	//field scaled to the current size and orientation
	mutable QPixmap scaledPrevious;
	QVariantAnimation* fade = nullptr;
	qreal fadeProgress = 1;	//0: previous field, 1: field
	bool hasValues = false;
	Color orientation = WHITE;
	QColor panelColor;
};

#endif
