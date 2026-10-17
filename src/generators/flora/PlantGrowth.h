#ifndef OUTSHINE_GENERATORS_FLORA_PLANTGROWTH_H
#define OUTSHINE_GENERATORS_FLORA_PLANTGROWTH_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {

using PlantGrowthPoint = std::array<double, 3>;

struct PlantGrowthDemand {
  size_t Nodes = 16384, Iterations = 128, WorkingBytes = size_t{64} * 1024 * 1024;
  double StepM = 0, InfluenceM = 0, KillM = 0;
};

enum class PlantGrowthStop : uint8_t {
  IterationBudget,
  NodeBudget,
  ConsumedAttractors,
  UnreachableAttractors,
  CancelledDirections,
  OccupiedEndpoints
};

struct PlantGrowthStats {
  size_t Iterations = 0, Consumed = 0, RejectedEndpoints = 0, WorkingBytes = 0;
  uint64_t NearestQueries = 0, RadiusQueries = 0, DistanceChecks = 0, PartitionComparisons = 0;
};

struct PlantGrowthPlan {
  std::vector<PlantGrowthPoint> PositionsM;
  std::vector<int32_t> Parents;
  std::vector<uint32_t> Unreached;
  PlantGrowthStats Stats;
  PlantGrowthStop Stop = PlantGrowthStop::IterationBudget;
};

[[nodiscard]] std::expected<size_t, std::string_view>
MeasurePlantGrowth(std::span<const PlantGrowthPoint> attractors,
                   std::span<const PlantGrowthPoint> seedPositions,
                   std::span<const int32_t> seedParents,
                   PlantGrowthDemand demand);

[[nodiscard]] std::expected<PlantGrowthPlan, std::string_view>
GrowPlant(std::span<const PlantGrowthPoint> attractors,
          std::span<const PlantGrowthPoint> seedPositions,
          std::span<const int32_t> seedParents,
          PlantGrowthDemand demand);

}
#endif
