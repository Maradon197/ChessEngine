#ifndef SEARCHSTATUSLINE_HPP
#define SEARCHSTATUSLINE_HPP

#include <cstdint>

#include <QWidget>

class QTimer;
class SearchRecorder;

//the search window's numbers in two small lines for the analysis sidebar, shown while that window is closed:
//root moves as a progress bar with the count beside it, then status, thinking time and nodes per second with a
//triangle for whether the speed went up or down since the last update
class SearchStatusLine : public QWidget {
public:
	explicit SearchStatusLine(QWidget* parent = nullptr);

	void setRecorder(SearchRecorder& recorder);
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent* event) override;
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;

private:
	void poll();	//asks for snapshots like the search window, only while shown

	SearchRecorder* recorder = nullptr;
	QTimer* timer = nullptr;
	std::uint64_t generation = 0;

	bool haveSearch = false;
	bool finished = false;
	int searchedMoves = 0;
	int rootMoves = 0;
	double seconds = 0;
	double rate = 0;	//nodes per second, since the last update while searching, the average when done
	int trend = 0;	//+1 faster, -1 slower, 0 about the same

	std::uint64_t searchId = 0;
	std::uint64_t lastNodes = 0;
	double lastSeconds = 0;
	std::uint64_t rootCountSearch = 0;	//the search the legal root move count was made for
	int legalRootMoves = 0;
};

#endif
