#ifndef OUTSHINE_GENERATORS_FLORA_PLANTRECIPE_H
#define OUTSHINE_GENERATORS_FLORA_PLANTRECIPE_H

#include "PlantPlan.h"
#include "PlantBladeSurface.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <vector>

namespace outshine::Generators {
enum class PlantBranchShape : uint8_t { Taper, Rounded };
enum class PlantArrangement : uint8_t { Spiral, Distichous, Opposite, Decussate, Whorled };

namespace PlantLevelDefaults {
inline constexpr double TipScale = 0.55, AzimuthVariationRad = 0.18;
inline constexpr double First = 0.2, Last = 0.92, AngleRad = 1.1, AngleVariationRad = 0.18;
inline constexpr double BendRad = 0.35, BendVariationRad = 0.1;
inline constexpr double LeafFirst = 0.1, LeafLast = 0.96, LeafAngleRad = 1.12,
                        LeafSkyAlignment = 0.65;
}

struct PlantLevel {
  size_t Branches = 1, WhorlSize = 1;
  double LengthM = 0, BaseRadiusM = 0, TipRadiusM = 0, TipScale = PlantLevelDefaults::TipScale;
  PlantBranchShape Shape = PlantBranchShape::Taper;
  double AttachmentJitter = 0, AzimuthVariationRad = PlantLevelDefaults::AzimuthVariationRad,
         UpwardBias = 0;
  double First = PlantLevelDefaults::First, Last = PlantLevelDefaults::Last,
         AngleRad = PlantLevelDefaults::AngleRad,
         AngleVariationRad = PlantLevelDefaults::AngleVariationRad;
  double BendRad = PlantLevelDefaults::BendRad,
         BendVariationRad = PlantLevelDefaults::BendVariationRad;
  std::optional<double> LeafInternodeM, TipAngleRad;
  double LeafFirst = PlantLevelDefaults::LeafFirst, LeafLast = PlantLevelDefaults::LeafLast,
         LeafAngleRad = PlantLevelDefaults::LeafAngleRad,
         LeafSkyAlignment = PlantLevelDefaults::LeafSkyAlignment;
  PlantArrangement Arrangement = PlantArrangement::Spiral;
};

struct PlantRecipe {
  uint64_t Seed = 0;
  std::vector<PlantLevel> Levels;
  std::vector<PlantBlade> Blades;
};

struct PlantRecipeDemand {
  std::array<int16_t, 7> Path{-1, -1, -1, -1, -1, -1, -1};
  size_t MaximumAxes = 0, MaximumLeaves = 0;
};

struct PlantRecipeSize {
  uint64_t Root = 0;
  size_t Axes = 0, Leaves = 0;
};

[[nodiscard]] std::expected<PlantRecipeSize, std::string_view>
MeasurePlantRecipe(const PlantRecipe &recipe, PlantRecipeDemand demand);
[[nodiscard]] std::expected<PlantPlan, std::string_view> BuildPlantPlan(const PlantRecipe &recipe,
                                                                        PlantRecipeDemand demand);
}
#endif
