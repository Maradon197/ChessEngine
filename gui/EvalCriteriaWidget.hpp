#ifndef EVALCRITERIAWIDGET_HPP
#define EVALCRITERIAWIDGET_HPP

#include <array>
#include <optional>

#include <QWidget>

#include "GuiTypes.hpp"

class CriterionBar;
class Doorman;
class QLabel;

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

	QLabel* overallValueLabel = nullptr;
	std::array<CriterionBar*, CRITERION_COUNT> bars{};
};

#endif
