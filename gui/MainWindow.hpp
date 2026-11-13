#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>

class Doorman;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(Doorman& doorman, QWidget* parent = nullptr);
};

#endif
