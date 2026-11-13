#ifndef SIDEPANELWIDGET_HPP
#define SIDEPANELWIDGET_HPP

#include <QWidget>

class Doorman;
class QToolButton;
class QVBoxLayout;

class SidePanelWidget : public QWidget {
	Q_OBJECT

public:
	explicit SidePanelWidget(Doorman& doorman, QWidget* parent = nullptr);

	void addSection(const QString& title, QWidget* content);
	void setCollapsed(bool collapsed);
	bool isCollapsed() const;

private:
	QToolButton* toggleButton = nullptr;
	QWidget* contentArea = nullptr;
	QVBoxLayout* sectionLayout = nullptr;
	bool collapsed = false;
};

#endif
