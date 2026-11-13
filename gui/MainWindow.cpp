#include "MainWindow.hpp"

#include <QHBoxLayout>
#include <QSplitter>
#include <QVBoxLayout>

#include "BoardWidget.hpp"
#include "Doorman.hpp"
#include "EvalCriteriaWidget.hpp"
#include "EvalSquareWidget.hpp"
#include "MoveTreeWidget.hpp"
#include "ReviewWidget.hpp"
#include "SidePanelWidget.hpp"
#include "ToolBarWidget.hpp"

MainWindow::MainWindow(Doorman& doorman, QWidget* parent)
	: QMainWindow(parent) {
	setWindowTitle(tr("Chess Engine GUI"));
	resize(1400, 900);

	addToolBar(Qt::TopToolBarArea, new ToolBarWidget(doorman, this));

	const auto makeSplitter = [](Qt::Orientation orientation, QWidget* parent) {
		auto* splitter = new QSplitter(orientation, parent);
		splitter->setChildrenCollapsible(false);
		splitter->setHandleWidth(7);
		return splitter;
	};

	auto* central = new QWidget(this);
	auto* rootLayout = new QHBoxLayout(central);
	rootLayout->setContentsMargins(8, 8, 0, 8);

	//eval square + criteria
	auto* leftColumn = new QWidget(central);
	leftColumn->setMinimumWidth(140);
	leftColumn->setMaximumWidth(360);
	auto* leftLayout = new QVBoxLayout(leftColumn);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	leftLayout->addWidget(new EvalSquareWidget(doorman, leftColumn));
	leftLayout->addWidget(new EvalCriteriaWidget(doorman, leftColumn), 1);

	//eval | board | review
	auto* analysisSplitter = makeSplitter(Qt::Horizontal, central);
	analysisSplitter->addWidget(leftColumn);
	analysisSplitter->addWidget(new BoardWidget(doorman, analysisSplitter));
	analysisSplitter->addWidget(new ReviewWidget(doorman, analysisSplitter));
	analysisSplitter->setStretchFactor(0, 0);
	analysisSplitter->setStretchFactor(1, 1);
	analysisSplitter->setStretchFactor(2, 0);
	analysisSplitter->setSizes({190, 800, 300});

	//move tree below
	auto* verticalSplitter = makeSplitter(Qt::Vertical, central);
	verticalSplitter->addWidget(analysisSplitter);
	verticalSplitter->addWidget(new MoveTreeWidget(doorman, verticalSplitter));
	verticalSplitter->setStretchFactor(0, 1);
	verticalSplitter->setStretchFactor(1, 0);
	verticalSplitter->setSizes({700, 130});

	//side panel on the right
	auto* rootSplitter = makeSplitter(Qt::Horizontal, central);
	rootSplitter->addWidget(verticalSplitter);
	rootSplitter->addWidget(new SidePanelWidget(doorman, rootSplitter));
	rootSplitter->setStretchFactor(0, 1);
	rootSplitter->setStretchFactor(1, 0);

	rootLayout->addWidget(rootSplitter);
	setCentralWidget(central);
}
