#include "PlantCrownPlan.h"
#include "PlantGrowth.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ratio>
#include <utility>
#include <vector>

namespace {
void WriteLeaves(const std::vector<outshine::Generators::PlantCrownLeaf> &group) {
  for (const auto &leaf : group) {
    std::cout << "L " << leaf.Id << ' ' << leaf.OwnerNode << ' '
              << static_cast<unsigned>(leaf.Variant);
    for (const double value : leaf.Pose.OriginM) { std::cout << ' ' << value; }
    for (const double value : leaf.Pose.NodeM) { std::cout << ' ' << value; }
    for (const double value : leaf.Pose.Frame) { std::cout << ' ' << value; }
    std::cout << ' ' << leaf.Pose.StemParameter << '\n';
  }
}
}

int main() {
  using namespace outshine::Generators;
  size_t nodes = 0;
  size_t variants = 0;
  size_t requests = 0;
  constexpr size_t kMaximumLeaves = 100000;
  PlantCrownRecipe recipe;
  if (!(std::cin >> nodes >> variants >> requests >> recipe.Seed >> recipe.LengthM >>
        recipe.BendRad >> recipe.PetioleM >> recipe.FoliageDepthM) ||
      nodes > 65536 || variants > kMaximumLeaves || requests > 65536) {
    return 2;
  }
  PlantGrowthPlan growth;
  growth.PositionsM.resize(nodes);
  growth.Parents.resize(nodes);
  std::vector<uint8_t> sourceVariants(variants);
  for (size_t row = 0; row < nodes; ++row) {
    if (!(std::cin >> growth.Parents[row] >> growth.PositionsM[row][0] >>
          growth.PositionsM[row][1] >> growth.PositionsM[row][2])) {
      return 2;
    }
  }
  for (auto &variant : sourceVariants) {
    unsigned value = 0;
    if (!(std::cin >> value) || value >= 64) { return 2; }
    variant = static_cast<uint8_t>(value);
  }
  const auto start = std::chrono::steady_clock::now();
  auto crown = PlantCrownPlan::Create(
      std::move(growth), std::move(sourceVariants), recipe, size_t{1024} * 1024);
  if (!crown) {
    std::cerr << crown.error() << '\n';
    return 1;
  }
  const double indexMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  const auto &summary = crown->Summarize();
  std::cout << std::setprecision(std::numeric_limits<double>::max_digits10) << "S " << summary.Nodes
            << ' ' << summary.Tips << ' ' << summary.Shoots << ' ' << summary.Leaves << ' '
            << summary.IndexBuildBytes << ' ' << summary.IndexResidentBytes << '\n';
  std::cout << 'V';
  for (const size_t count : summary.Variants) { std::cout << ' ' << count; }
  std::cout << '\n';
  for (const uint32_t host : crown->Hosts()) {
    const auto count = crown->MeasureLeaves({.Node = host, .Maximum = variants});
    if (!count) { return 3; }
    std::cout << "H " << host << ' ' << *count << '\n';
  }
  double poseMs = 0;
  size_t poses = 0;
  size_t largest = 0;
  for (size_t request = 0; request < requests; ++request) {
    PlantCrownPlan::LeafDemand demand;
    if (!(std::cin >> demand.Node >> demand.Maximum)) { return 2; }
    const auto begin = std::chrono::steady_clock::now();
    const auto group = crown->Leaves(demand);
    poseMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    if (!group) {
      std::cout << "E " << demand.Node << '\n';
      continue;
    }
    poses += group->size();
    largest = std::max(largest, group->size());
    std::cout << "G " << demand.Node << ' ' << group->size() << '\n';
    WriteLeaves(*group);
  }
  std::cout << "T " << indexMs << ' ' << poseMs << ' ' << poses << ' '
            << largest * sizeof(PlantCrownLeaf) << '\n';
}
