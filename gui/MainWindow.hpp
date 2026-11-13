#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>

class Doorman;

//Arranges the widgets. Holds no state; all communication runs through the Doorman.
//
//All borders between widgets can be dragged.
//
//	+---------------------------------------------------------+
//	| toolbar                                                 |
//	+------------+--------------------------+-----------+-----+
//	| eval square|                          |           |  s  |
//	| eval       |        board             |  review   |  i  |
//	| criteria   |                          |           |  d  |
//	+------------+--------------------------+-----------+  e  |
//	|                 move tree                         |     |
//	+---------------------------------------------------+-----+
class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(Doorman& doorman, QWidget* parent = nullptr);
};

#endif
