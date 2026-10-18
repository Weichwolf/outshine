#ifndef OUTSHINE_TEST_PLANTRECIPEFIXTURE_H
#define OUTSHINE_TEST_PLANTRECIPEFIXTURE_H
#include "PlantRecipe.h"
#include "PlantBladeSurface.h"
#include <cstdint>

namespace outshine::Test {
inline Generators::PlantRecipe PlantRecipeFixture() {
  using namespace Generators;
  constexpr uint64_t kSeed = 17;
  constexpr PlantLevel kTrunk{.LengthM = 1,
                              .BaseRadiusM = 0.1,
                              .TipRadiusM = 0.08,
                              .TipScale = 1,
                              .LeafInternodeM = 0.2,
                              .TipAngleRad = std::nullopt};
  constexpr PlantLevel kBranch{.Branches = 3,
                               .LengthM = 0.4,
                               .BaseRadiusM = 0.02,
                               .TipRadiusM = 0.01,
                               .LeafInternodeM = 0.1,
                               .TipAngleRad = std::nullopt};
  constexpr PlantLevel kTwig{.Branches = 2,
                             .LengthM = 0.12,
                             .BaseRadiusM = 0.002,
                             .TipRadiusM = 0.001,
                             .LeafInternodeM = 0.03,
                             .TipAngleRad = std::nullopt};
  constexpr PlantBlade kBlade{.LengthM = 0.1, .WidthM = 0.03, .Widest = 0.6, .BaseFill = 0.1};
  return {.Seed = kSeed, .Levels = {kTrunk, kBranch, kTwig}, .Blades = {kBlade, kBlade}};
}
}
#endif
