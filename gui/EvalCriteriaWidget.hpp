#ifndef EVALCRITERIAWIDGET_HPP
#define EVALCRITERIAWIDGET_HPP

#include <array>
#include <optional>

#include <QWidget>

#include "GuiTypes.hpp"

class Card;
class CriterionBar;
class Doorman;

//one three-step track per criterion; the overall score sits under the board control map (EvalSquareWidget)
class EvalCriteriaWidget : public QWidget {
	Q_OBJECT

public:
	explicit EvalCriteriaWidget(Doorman& doorman, QWidget* parent = nullptr);

	void setCriterion(EvalCriterion criterion, int value);
	std::optional<int> getCriterion(EvalCriterion criterion) const;
	void clearCriteria();

	void setPanelColor(const QColor& color);

private:
	static constexpr std::size_t CRITERION_COUNT = static_cast<std::size_t>(EvalCriterion::COUNT);

	Card* criteriaCard = nullptr;
	std::array<CriterionBar*, CRITERION_COUNT> bars{};
};

#endif
