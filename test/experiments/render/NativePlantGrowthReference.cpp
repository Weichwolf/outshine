#include "PlantGrowth.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ratio>
#include <vector>

int main() {
  using namespace outshine::Generators;
  PlantGrowthDemand demand;
  size_t seeds = 0;
  size_t sources = 0;
  if (!(std::cin >> seeds >> sources >> demand.Nodes >> demand.Iterations >> demand.StepM >>
        demand.InfluenceM >> demand.KillM) ||
      seeds > 65536 || sources > 65536) {
    return 2;
  }
  std::vector<PlantGrowthPoint> positions(seeds);
  std::vector<PlantGrowthPoint> attractors(sources);
  std::vector<int32_t> parents(seeds);
  for (size_t row = 0; row < seeds; ++row) {
    if (!(std::cin >> parents[row] >> positions[row][0] >> positions[row][1] >>
          positions[row][2])) {
      return 2;
    }
  }
  for (auto &point : attractors) {
    if (!(std::cin >> point[0] >> point[1] >> point[2])) { return 2; }
  }
  const auto start = std::chrono::steady_clock::now();
  const auto plan = GrowPlant(attractors, positions, parents, demand);
  if (!plan) {
    std::cerr << plan.error() << '\n';
    return 1;
  }
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  const auto &stats = plan->Stats;
  std::cout << std::setprecision(std::numeric_limits<double>::max_digits10) << "S "
            << plan->PositionsM.size() << ' ' << plan->Unreached.size() << ' '
            << static_cast<unsigned>(plan->Stop) << ' ' << stats.Consumed << ' ' << stats.Iterations
            << ' ' << stats.NearestQueries << ' ' << stats.RejectedEndpoints << ' '
            << stats.RadiusQueries << ' ' << stats.DistanceChecks << ' '
            << stats.PartitionComparisons << ' ' << stats.WorkingBytes << ' ' << elapsedMs << '\n';
  for (size_t row = 0; row < plan->PositionsM.size(); ++row) {
    const auto &point = plan->PositionsM[row];
    std::cout << "N " << plan->Parents[row] << ' ' << point[0] << ' ' << point[1] << ' ' << point[2]
              << '\n';
  }
  std::cout << 'R';
  for (const uint32_t id : plan->Unreached) { std::cout << ' ' << id; }
  std::cout << '\n';
}
