#include "MainWindow.hpp"

#include <QHBoxLayout>

#include "AnalysisSidebar.hpp"
#include "BoardWidget.hpp"
#include "Doorman.hpp"
#include "KeyBindings.hpp"
#include "MoveTreeOverlay.hpp"
#include "PrincipleDrawer.hpp"
#include "ToolBarWidget.hpp"

MainWindow::MainWindow(Doorman& doorman, QWidget* parent)
	: QMainWindow(parent) {
	setWindowTitle(tr("Chess Engine GUI"));
	resize(1400, 900);

	auto* keyBindings = new KeyBindings(*this);
	addToolBar(Qt::TopToolBarArea, new ToolBarWidget(doorman, *keyBindings, this));

	//board | evaluation, the sidebar slides away on its key and the board takes the room
	auto* central = new QWidget(this);
	auto* rootLayout = new QHBoxLayout(central);
	rootLayout->setContentsMargins(8, 8, 8, 8);
	rootLayout->setSpacing(0);
	rootLayout->addWidget(new BoardWidget(doorman, central), 1);
	rootLayout->addWidget(new AnalysisSidebar(doorman, *keyBindings, central));
	setCentralWidget(central);

	//principles, slide up from the bottom
	new PrincipleDrawer(doorman, *keyBindings, *this);

	//move tree, shown over everything
	new MoveTreeOverlay(doorman, *keyBindings, *this);
}
