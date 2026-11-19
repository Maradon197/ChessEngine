#ifndef MODESWITCH_HPP
#define MODESWITCH_HPP

#include <array>

#include <QWidget>

#include "GuiTypes.hpp"

class QVariantAnimation;

//"Analysis | Play" pill in the toolbar; the raised highlight slides over to the current view
class ModeSwitch : public QWidget {
	Q_OBJECT

public:
	explicit ModeSwitch(QWidget* parent = nullptr);

	void setMode(ViewMode mode);	//animated, emits nothing
	ViewMode getMode() const;

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

signals:
	void modeRequested(ViewMode mode);	//a segment was clicked, the window switches and calls setMode()

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void leaveEvent(QEvent* event) override;
	void changeEvent(QEvent* event) override;

private:
	QFont getLabelFont() const;
	QRectF getSegmentRect(int index) const;
	int getSegmentAt(const QPointF& point) const;	//-1 outside

	std::array<QString, 2> labels;
	ViewMode mode = ViewMode::ANALYSIS;
	qreal position = 0;	//0: highlight on Analysis, 1: on Play
	int hovered = -1;
	int pressed = -1;
	QVariantAnimation* slide = nullptr;
};

#endif
