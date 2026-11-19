#include "PrincipleRegistry.hpp"

#include <array>

#include "DataTypes.hpp"
#include "EngineTypes.hpp"

std::array<float, size_t(PrincipleId::COUNT)> evaluateAll(const EvalContext& ctx) {
	std::array<float, size_t(PrincipleId::COUNT)> results{};
	
	for (std::size_t i = 0; i < static_cast<std::size_t>(PrincipleId::COUNT); ++i) {
		results[i] = PRINCIPLES[i].calculateScore(ctx);
	}

	return results;
}
