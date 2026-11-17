#ifndef ANALYSISSIDEBAR_HPP
#define ANALYSISSIDEBAR_HPP

#include <QWidget>

class Doorman;
class KeyBindings;
class QScrollArea;
class QVariantAnimation;

//evaluation column on the right of the board; slides out of the window and back in on its key
class AnalysisSidebar : public QWidget {
	Q_OBJECT

public:
	AnalysisSidebar(Doorman& doorman, KeyBindings& keyBindings, QWidget* parent = nullptr);

	void setShown(bool shown);
	bool isShown() const;

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	void applyProgress();

	QScrollArea* content = nullptr;	//keeps its full width while the sidebar narrows, so nothing reflows mid-slide
	QVariantAnimation* slide = nullptr;
	qreal progress = 1;
	bool shown = true;
};

#endif
