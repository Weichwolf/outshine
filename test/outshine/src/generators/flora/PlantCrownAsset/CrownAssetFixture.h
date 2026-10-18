#ifndef OUTSHINE_TEST_CROWNASSETFIXTURE_H
#define OUTSHINE_TEST_CROWNASSETFIXTURE_H

#include "PlantCrownPlan.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <utility>

namespace outshine::Test {
inline constexpr size_t kCrownAssetBudget = 4096;
inline constexpr size_t kCrownIndexBudget = 60;
inline constexpr uint32_t kUnreachedLast = 19;
inline constexpr Generators::PlantGrowthStats kRecordedGrowth = {.Iterations = 31,
                                                                 .Consumed = 20,
                                                                 .RejectedEndpoints = 9,
                                                                 .WorkingBytes = 5000,
                                                                 .NearestQueries = 300,
                                                                 .RadiusQueries = 50,
                                                                 .DistanceChecks = 900,
                                                                 .PartitionComparisons = 800};

inline std::expected<Generators::PlantCrownPlan, std::string_view> CrownAssetFixture() {
  Generators::PlantGrowthPlan growth{
      .PositionsM = {{0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {1, 0, 2}, {0, 0, 3}},
      .Parents = {-1, 0, 1, 2, 2},
      .Unreached = {3, 7, kUnreachedLast},
      .Stats = kRecordedGrowth,
      .Stop = Generators::PlantGrowthStop::OccupiedEndpoints};
  constexpr double kLengthM = 0.35;
  constexpr double kBendRad = 0.4;
  constexpr double kPetioleM = 0.004;
  const Generators::PlantCrownRecipe recipe{.Seed = 682,
                                            .LengthM = kLengthM,
                                            .BendRad = kBendRad,
                                            .PetioleM = kPetioleM,
                                            .FoliageDepthM = 1};
  return Generators::PlantCrownPlan::Create(
      std::move(growth), {7, 2, 5, 7, 2}, recipe, kCrownIndexBudget);
}
}
#endif
