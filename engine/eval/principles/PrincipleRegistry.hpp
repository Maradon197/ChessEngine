#ifndef INCLUDED__CHESS_ENGINE__PRINCIPLE_REGISTRY_HPP
#define INCLUDED__CHESS_ENGINE__PRINCIPLE_REGISTRY_HPP

#include <array>
#include <cstddef>

#include "DataTypes.hpp"
#include "EngineTypes.hpp"

#include "PrincipleFunctions.hpp"

std::array<float, size_t(PrincipleId::COUNT)> evaluateAll(const EvalContext& ctx);

inline constexpr std::array PRINCIPLES = {
	PrincipleInfo{
		.id = PrincipleId::THIS_RETURNS_ZERO_POINT_FIVE,
		.title = "Test function: return 0.5f",
		.imagePath = "",
		.spotElo = 600,
		.description = "This is the description",
		.gamePart = GamePart::OPENING,
		.focus = PrincipleFocus::STRATEGY,
		.calculateScore = principles::thisReturnsZeroPointFive,
	},

	//TODO: more principles
};

//make wrong entries a compile error
namespace principle_registry_checks {
	consteval bool idsMatchIndices() {
		for (std::size_t i = 0; i < PRINCIPLES.size(); ++i) {
			if (static_cast<std::size_t>(PRINCIPLES[i].id) != i) return false;
		}
		return true;
	}

	consteval bool allHaveScoreFunction() {
		for (const PrincipleInfo& principle : PRINCIPLES) {
			if (principle.calculateScore == nullptr) return false;
		}
		return true;
	}
}

static_assert(PRINCIPLES.size() == static_cast<std::size_t>(PrincipleId::COUNT), 
	"engine/eval/principles/PrincipleRegistry.hpp:every PrincipleId needs exactly one entry in PRINCIPLES");
static_assert(principle_registry_checks::idsMatchIndices(), 
	"engine/eval/principles/PrincipleRegistry.hpp: PRINCIPLES must be in the same order as PrincipleId");
static_assert(principle_registry_checks::allHaveScoreFunction(), 
	"engine/eval/principles/PrincipleRegistry.hpp: every principle needs a calculateScore function");


#endif //!INCLUDED__CHESS_ENGINE__PRINCIPLE_REGISTRY_HPP
