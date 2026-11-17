#ifndef EVALCRITERIAWIDGET_HPP
#define EVALCRITERIAWIDGET_HPP

#include <array>
#include <optional>

#include <QWidget>

#include "GuiTypes.hpp"

class BalanceBar;
class Card;
class CriterionBar;
class Doorman;
class QLabel;

//overall evaluation as a number, a sentence and a black/white balance, then one tug-of-war per criterion
class EvalCriteriaWidget : public QWidget {
	Q_OBJECT

public:
	explicit EvalCriteriaWidget(Doorman& doorman, QWidget* parent = nullptr);

	void setOverallCentipawns(int centipawns);
	void setOverallMate(int movesToMate);
	void clearOverall();

	void setCriterion(EvalCriterion criterion, int value);
	std::optional<int> getCriterion(EvalCriterion criterion) const;
	void clearCriteria();

	void setPanelColor(const QColor& color);

private:
	static constexpr std::size_t CRITERION_COUNT = static_cast<std::size_t>(EvalCriterion::COUNT);

	Card* overallCard = nullptr;
	Card* criteriaCard = nullptr;
	QLabel* overallValueLabel = nullptr;
	QLabel* verdictLabel = nullptr;
	BalanceBar* balanceBar = nullptr;
	std::array<CriterionBar*, CRITERION_COUNT> bars{};
};

#endif
