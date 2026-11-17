#include "MainWindow.hpp"

#include <QHBoxLayout>
#include <QSplitter>
#include <QVBoxLayout>

#include "BoardWidget.hpp"
#include "Doorman.hpp"
#include "EvalCriteriaWidget.hpp"
#include "EvalSquareWidget.hpp"
#include "MoveTreeOverlay.hpp"
#include "PrincipleDrawer.hpp"
#include "ToolBarWidget.hpp"

MainWindow::MainWindow(Doorman& doorman, QWidget* parent)
	: QMainWindow(parent) {
	setWindowTitle(tr("Chess Engine GUI"));
	resize(1400, 900);

	addToolBar(Qt::TopToolBarArea, new ToolBarWidget(doorman, this));

	auto* central = new QWidget(this);
	auto* rootLayout = new QHBoxLayout(central);
	rootLayout->setContentsMargins(8, 8, 8, 8);

	//eval square + criteria
	auto* leftColumn = new QWidget(central);
	leftColumn->setMinimumWidth(140);
	leftColumn->setMaximumWidth(360);
	auto* leftLayout = new QVBoxLayout(leftColumn);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	leftLayout->addWidget(new EvalSquareWidget(doorman, leftColumn));
	leftLayout->addWidget(new EvalCriteriaWidget(doorman, leftColumn), 1);

	//eval | board
	auto* analysisSplitter = new QSplitter(Qt::Horizontal, central);
	analysisSplitter->setChildrenCollapsible(false);
	analysisSplitter->setHandleWidth(7);
	analysisSplitter->addWidget(leftColumn);
	analysisSplitter->addWidget(new BoardWidget(doorman, analysisSplitter));
	analysisSplitter->setStretchFactor(0, 0);
	analysisSplitter->setStretchFactor(1, 1);
	analysisSplitter->setSizes({190, 1100});

	rootLayout->addWidget(analysisSplitter);
	setCentralWidget(central);

	//principles, slide up from the bottom on Ctrl+Space
	new PrincipleDrawer(doorman, *this);

	//move tree, shown over everything while M is held
	new MoveTreeOverlay(doorman, *this);
}
