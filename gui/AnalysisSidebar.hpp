#ifndef ANALYSISSIDEBAR_HPP
#define ANALYSISSIDEBAR_HPP

#include <QWidget>

class Doorman;
class KeyBindings;
class QCheckBox;
class QLabel;
class SearchRecorder;
class SearchStatusLine;
class QScrollArea;
class QVariantAnimation;

//evaluation column on the right of the board; slides out of the window and back in on its key
class AnalysisSidebar : public QWidget {
	Q_OBJECT

public:
	AnalysisSidebar(Doorman& doorman, KeyBindings& keyBindings, QWidget* parent = nullptr);

	void setShown(bool shown);
	bool isShown() const;

	int getSearchDepth() const;	//how deep the engine analyses every position, in plies
	bool isEngineArrowShown() const;	//the engine draws its best move

	void setSearchRecorder(SearchRecorder& recorder);
	void setSearchStatusShown(bool shown);	//the small search status, while the search window is closed

signals:
	void searchDepthChanged(int depth);
	void engineArrowShownChanged(bool shown);

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	void applyProgress();

	void setSearchDepth(int depth);	//slider moved: shown, kept in the settings, announced

	QScrollArea* content = nullptr;
	QLabel* depthValue = nullptr;
	QCheckBox* engineArrow = nullptr;
	SearchStatusLine* searchStatus = nullptr;
	int searchDepth = 4;	//keeps its full width while the sidebar narrows, so nothing reflows mid-slide
	QVariantAnimation* slide = nullptr;
	qreal progress = 1;
	bool shown = true;
};

#endif
