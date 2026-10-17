#include "PlantGrowth.h"
#include "PlantGrowthIndex.h"
#include "PlantSkeleton.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr double kMaximumCoordinateM = 100000;
constexpr double kDirectionCancellation = 1e-12;
constexpr double kEndpointToleranceScale = 1e-9;

PlantGrowthPoint Difference(PlantGrowthPoint first, PlantGrowthPoint second) {
  for (size_t axis = 0; axis < 3; ++axis) { first[axis] -= second[axis]; }
  return first;
}

double Length(PlantGrowthPoint point) {
  return std::sqrt(point[0] * point[0] + point[1] * point[1] + point[2] * point[2]);
}

struct GrowthWork {
  PlantGrowthPlan Plan;
  std::vector<PlantGrowthPoint> Sums, Proposals;
  std::vector<uint32_t> Sources;
  std::vector<uint8_t> Free;
  PlantGrowthIndex Nodes, Candidates;
  size_t Count;

  GrowthWork(std::span<const PlantGrowthPoint> positions,
             std::span<const int32_t> parents,
             PlantGrowthDemand demand,
             size_t attractors)
      : Sums(demand.Nodes),
        Proposals(demand.Nodes),
        Sources(demand.Nodes),
        Free(demand.Nodes),
        Nodes(demand.Nodes),
        Candidates(demand.Nodes),
        Count(positions.size()) {
    Plan.PositionsM.resize(demand.Nodes);
    Plan.Parents.resize(demand.Nodes);
    Plan.Unreached.resize(attractors);
    std::ranges::copy(positions, Plan.PositionsM.begin());
    std::ranges::copy(parents, Plan.Parents.begin());
    std::ranges::iota(Plan.Unreached, uint32_t{0});
  }

  bool Accumulate(std::span<const PlantGrowthPoint> attractors, PlantGrowthDemand demand) {
    Nodes.Build(std::span(Plan.PositionsM).first(Count), Plan.Stats);
    std::fill_n(Sums.begin(), Count, PlantGrowthPoint{});
    size_t kept = 0;
    bool accepted = false;
    for (const uint32_t id : Plan.Unreached) {
      const auto hit = Nodes.Nearest(attractors[id], demand.InfluenceM, Plan.Stats);
      const double distance = std::sqrt(hit.SquaredM);
      if (hit.Found() && distance <= demand.KillM) {
        ++Plan.Stats.Consumed;
        continue;
      }
      Plan.Unreached[kept++] = id;
      if (!hit.Found()) { continue; }
      accepted = true;
      const auto direction = Difference(attractors[id], Plan.PositionsM[hit.Node]);
      for (size_t axis = 0; axis < 3; ++axis) {
        Sums[hit.Node][axis] += direction[axis] / distance;
      }
    }
    Plan.Unreached.resize(kept);
    return accepted;
  }

  size_t Propose(PlantGrowthDemand demand) {
    size_t count = 0;
    for (size_t source = 0; source < Count; ++source) {
      const double length = Length(Sums[source]);
      if (length <= kDirectionCancellation) { continue; }
      Sources[count] = static_cast<uint32_t>(source);
      for (size_t axis = 0; axis < 3; ++axis) {
        Proposals[count][axis] =
            Plan.PositionsM[source][axis] + Sums[source][axis] / length * demand.StepM;
      }
      ++count;
    }
    return count;
  }

  size_t Append(size_t proposals, PlantGrowthDemand demand) {
    const double toleranceM = demand.StepM * kEndpointToleranceScale;
    size_t freeCount = 0;
    for (size_t row = 0; row < proposals; ++row) {
      Free[row] =
          static_cast<uint8_t>(!Nodes.Nearest(Proposals[row], toleranceM, Plan.Stats).Found());
      freeCount += Free[row];
    }
    if (freeCount > 1) {
      Candidates.Build(std::span(Proposals).first(proposals), Plan.Stats);
      for (size_t row = 0; row < proposals; ++row) {
        if (Free[row] != 0) {
          Candidates.DiscardFollowing(static_cast<uint32_t>(row), toleranceM, Free, Plan.Stats);
        }
      }
    }
    size_t added = 0;
    for (size_t row = 0; row < proposals; ++row) {
      if (Free[row] == 0) {
        ++Plan.Stats.RejectedEndpoints;
      } else if (Count + added < demand.Nodes) {
        Plan.PositionsM[Count + added] = Proposals[row];
        Plan.Parents[Count + added] = static_cast<int32_t>(Sources[row]);
        ++added;
      }
    }
    Count += added;
    return added;
  }

  void ConsumeFinal(std::span<const PlantGrowthPoint> attractors, double killM) {
    if (Plan.Unreached.empty()) { return; }
    Nodes.Build(std::span(Plan.PositionsM).first(Count), Plan.Stats);
    size_t kept = 0;
    for (const uint32_t id : Plan.Unreached) {
      const auto hit = Nodes.Nearest(attractors[id], killM, Plan.Stats);
      if (hit.Found() && std::sqrt(hit.SquaredM) <= killM) {
        ++Plan.Stats.Consumed;
      } else {
        Plan.Unreached[kept++] = id;
      }
    }
    Plan.Unreached.resize(kept);
  }
};
}

std::expected<size_t, std::string_view>
MeasurePlantGrowth(std::span<const PlantGrowthPoint> attractors,
                   std::span<const PlantGrowthPoint> seedPositions,
                   std::span<const int32_t> seedParents,
                   PlantGrowthDemand demand) {
  if (demand.Nodes < 2 || demand.Nodes > static_cast<size_t>(std::numeric_limits<int32_t>::max()) ||
      demand.Iterations == 0 || seedPositions.size() < 2 || seedPositions.size() > demand.Nodes ||
      !ValidPlantSkeleton(seedPositions, seedParents) ||
      attractors.size() > std::numeric_limits<uint32_t>::max() ||
      !std::ranges::all_of(std::array{demand.StepM, demand.InfluenceM, demand.KillM},
                           [](double value) {
                             return std::isfinite(value) && value > 0 &&
                                    value <= kMaximumCoordinateM;
                           }) ||
      demand.StepM > demand.InfluenceM || demand.KillM > demand.InfluenceM ||
      demand.StepM * kEndpointToleranceScale == 0 ||
      !std::ranges::all_of(attractors, ValidPlantGrowthPoint)) {
    return std::unexpected("growth requires finite bounded points, dimensions and a rooted seed");
  }
  const size_t perNode =
      3 * sizeof(PlantGrowthPoint) + sizeof(int32_t) + 3 * sizeof(uint32_t) + sizeof(uint8_t);
  if (demand.Nodes > demand.WorkingBytes / perNode) {
    return std::unexpected("growth node workspace exceeds demand");
  }
  const size_t nodeBytes = demand.Nodes * perNode;
  if (attractors.size() > (demand.WorkingBytes - nodeBytes) / sizeof(uint32_t)) {
    return std::unexpected("growth attractor workspace exceeds demand");
  }
  return nodeBytes + attractors.size() * sizeof(uint32_t);
}

std::expected<PlantGrowthPlan, std::string_view>
GrowPlant(std::span<const PlantGrowthPoint> attractors,
          std::span<const PlantGrowthPoint> seedPositions,
          std::span<const int32_t> seedParents,
          PlantGrowthDemand demand) {
  const auto bytes = MeasurePlantGrowth(attractors, seedPositions, seedParents, demand);
  if (!bytes) { return std::unexpected(bytes.error()); }
  GrowthWork work(seedPositions, seedParents, demand, attractors.size());
  work.Plan.Stats.WorkingBytes = *bytes;
  for (size_t iteration = 0; iteration < demand.Iterations; ++iteration) {
    ++work.Plan.Stats.Iterations;
    const bool accepted = work.Accumulate(attractors, demand);
    if (work.Plan.Unreached.empty()) { break; }
    if (!accepted) {
      work.Plan.Stop = PlantGrowthStop::UnreachableAttractors;
      break;
    }
    const size_t proposed = work.Propose(demand);
    if (proposed == 0) {
      work.Plan.Stop = PlantGrowthStop::CancelledDirections;
      break;
    }
    if (work.Count == demand.Nodes) {
      work.Plan.Stop = PlantGrowthStop::NodeBudget;
      break;
    }
    if (work.Append(proposed, demand) == 0) {
      work.Plan.Stop = PlantGrowthStop::OccupiedEndpoints;
      break;
    }
  }
  work.ConsumeFinal(attractors, demand.KillM);
  if (work.Plan.Unreached.empty()) { work.Plan.Stop = PlantGrowthStop::ConsumedAttractors; }
  work.Plan.PositionsM.resize(work.Count);
  work.Plan.Parents.resize(work.Count);
  return std::move(work.Plan);
}
}
