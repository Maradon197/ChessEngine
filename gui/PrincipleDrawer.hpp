#ifndef PRINCIPLEDRAWER_HPP
#define PRINCIPLEDRAWER_HPP

#include <string>
#include <unordered_set>
#include <vector>

#include <QColor>
#include <QPixmap>
#include <QWidget>

#include "GuiTypes.hpp"

class Doorman;
class EloSlider;
class PrincipleFrame;
class PrincipleGrid;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QScrollArea;
class QTextBrowser;
class QVariantAnimation;

//slides up from the bottom on Ctrl+Space: browse the principles and choose which ones the engine considers
class PrincipleDrawer : public QWidget {
	Q_OBJECT

public:
	PrincipleDrawer(Doorman& doorman, QWidget& window);

	void setPrinciples(const std::vector<Principle>& principles);
	void setPrincipleEnabled(const std::string& title, bool enabled);
	void setPrincipleUseful(const std::string& title, bool useful);
	void setProblems(const std::vector<std::string>& problems);
	void setPanelColor(const QColor& color);

signals:
	void principleEnabledChanged(const std::string& title, bool enabled);
	void refreshRequested();

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;

private:
	friend class PrincipleGrid;

	QWidget* createHeader();
	QWidget* createDetails();
	void setOpen(bool open);
	void updatePlacement();
	void applyFilters();
	void updateMarkers();
	void showDetails(int index);
	void hideDetails();
	void updateDetails();
	void changeEnabled(int index, bool enabled);	//user action, emits
	void setShownEnabled(bool enabled);
	void applyTargetElo(int targetElo);
	int findIndex(const std::string& title) const;

	std::vector<Principle> principles;	//sorted by spotElo
	std::vector<QPixmap> images;	//null: placeholder is drawn
	std::vector<int> shown;	//indices of the principles that pass the filters
	std::unordered_set<std::string> knownTitles;	//new titles take their state from the target Elo
	int detailIndex = -1;

	QColor panelColor;
	QVariantAnimation* slide = nullptr;
	qreal progress = 0;
	bool isOpen = false;

	QLabel* problemsLabel = nullptr;
	EloSlider* eloSlider = nullptr;
	QLineEdit* searchEdit = nullptr;
	QComboBox* difficultyFilter = nullptr;
	QComboBox* gamePartFilter = nullptr;
	QComboBox* focusFilter = nullptr;
	QComboBox* stateFilter = nullptr;
	PrincipleFrame* frame = nullptr;
	QScrollArea* scrollArea = nullptr;
	PrincipleGrid* grid = nullptr;

	QWidget* detailsPanel = nullptr;
	QLabel* detailTitle = nullptr;
	QLabel* detailImage = nullptr;
	QLabel* detailMeta = nullptr;
	QLabel* detailUseful = nullptr;
	QLabel* detailDescription = nullptr;
	QCheckBox* detailEnabled = nullptr;
	QTextBrowser* detailExplanation = nullptr;
};

#endif
