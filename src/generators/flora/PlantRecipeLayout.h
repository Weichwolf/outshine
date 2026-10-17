#ifndef OUTSHINE_GENERATORS_FLORA_PLANTRECIPELAYOUT_H
#define OUTSHINE_GENERATORS_FLORA_PLANTRECIPELAYOUT_H

#include "PlantRecipe.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

namespace outshine::Generators {
inline constexpr size_t kPlantRecipeMostBranches = 64;

struct PlantRecipeLevelLayout {
  uint64_t AxisOffset = 0, AxisCount = 0, LeafOffset = 0;
  std::array<uint64_t, kPlantRecipeMostBranches + 1> LeafPrefix{};
  uint64_t FirstOrdinal = 0, Count = 0;
  size_t FirstRow = 0;
};

struct PlantRecipeLayout {
  std::array<PlantRecipeLevelLayout, 8> Levels{};
  size_t Depth = 0;
  PlantRecipeSize Size;
};

[[nodiscard]] bool ValidPlantRecipe(const PlantRecipe &recipe);

[[nodiscard]] double
PlantLevelFraction(size_t slot, const PlantLevel &level, double single) noexcept;
[[nodiscard]] double PlantLevelLength(const PlantLevel &level, size_t slot) noexcept;
[[nodiscard]] size_t PlantLeavesPerGroup(PlantArrangement arrangement) noexcept;
[[nodiscard]] uint64_t PlantLeafPrefix(const PlantLevel &level,
                                       const PlantRecipeLevelLayout &layout,
                                       uint64_t ordinal) noexcept;
[[nodiscard]] std::expected<PlantRecipeLayout, std::string_view>
LayoutPlantRecipe(const PlantRecipe &recipe, PlantRecipeDemand demand);
}
#endif
