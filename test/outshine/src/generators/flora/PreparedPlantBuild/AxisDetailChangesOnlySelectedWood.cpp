#include "PreparedPlantBuildFixture.h"
#include "BuildPreparedPlant.h"
#include "PlantAxisDetail.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto original = PlantBuildPlan();
  CHECK(original, "canonical fixture is available");
  if (!original) { return Report(); }
  std::vector<PlantAxis> axes(original->Axes().begin(), original->Axes().end());
  constexpr double kBendRad = 0.3;
  constexpr double kTwigRadiusM = 0.0001;
  axes.front().Bend = kBendRad;
  axes.back().RadiiM = {kTwigRadiusM, kTwigRadiusM};
  std::vector<PlantLeaf> leaves;
  for (const auto &axis : original->Axes()) {
    const auto group = original->Leaves({.Axis = axis.Id, .Maximum = 3});
    if (group) { leaves.append_range(*group); }
  }
  auto plan = PlantPlan::Create(std::move(axes), std::move(leaves), 3);
  CHECK(plan, "curved trunk and thin twig preserve canonical source ownership");
  if (!plan) { return Report(); }
  auto demand = PlantBuildDemand();
  constexpr size_t kBudget = 1000;
  constexpr double kErrorM = 0.005;
  demand.Maximum = {
      .Vertices = kBudget, .Triangles = kBudget, .LeafInstances = 3, .TriangleReferences = kBudget};
  demand.AxisSegments = demand.AxisSides = 0;
  demand.AxisParametricErrorM = kErrorM;
  const auto blades = PlantBuildBlades();
  const auto palette = PlantBuildMaterials();
  const auto counts = MeasurePreparedPlant(*plan, blades, demand);
  const std::array<uint8_t, 32> identity{};
  const auto adaptive = BuildPreparedPlant(*plan, blades, identity, palette, demand);
  CHECK(counts && adaptive && PreparedPlantIsValid(*adaptive),
        "error-selected detail produces a complete bounded product");
  if (!counts || !adaptive) { return Report(); }
  size_t triangles = 0;
  for (int part = 0; part < adaptive->Prototypes.parts(); ++part) {
    triangles += adaptive->Prototypes.trianglesOf(part).size() / 3;
  }
  CHECK(triangles == counts->Triangles && counts->LeafInstances == 3,
        "preflight and actual mixed axis meshes have identical inventories");
  const auto twig = std::ranges::find(adaptive->Axes, kTwigAxis, &PreparedPlantAxis::Id);
  const auto root = std::ranges::find(adaptive->Axes, kRootAxis, &PreparedPlantAxis::Id);
  CHECK(twig != adaptive->Axes.end() && root != adaptive->Axes.end() &&
            twig->IndexCount < root->IndexCount,
        "thin twig spends fewer triangles while retaining its original owner ID");
  auto fixed = demand;
  fixed.AxisParametricErrorM.reset();
  fixed.AxisSegments = 2;
  fixed.AxisSides = 4;
  const auto control = BuildPreparedPlant(*plan, blades, identity, palette, fixed);
  CHECK(control && control->Leaves.size() == adaptive->Leaves.size(),
        "axis detail does not change the blade inventory");
  if (!control || control->Leaves.size() != adaptive->Leaves.size()) { return Report(); }
  for (size_t group = 0; group < adaptive->Leaves.size(); ++group) {
    const auto &left = adaptive->Leaves[group];
    const auto &right = control->Leaves[group];
    CHECK(left.Ids == right.Ids && left.Axes == right.Axes && left.Variant == right.Variant,
          "leaf identity and ownership remain independent of wood tessellation");
    CHECK(left.Transforms.size() == right.Transforms.size(), "placement counts remain equal");
    if (left.Transforms.size() != right.Transforms.size()) { return Report(); }
    for (size_t at = 0; at < left.Transforms.size(); ++at) {
      CHECK(left.Transforms[at].Column == right.Transforms[at].Column,
            "wood detail does not recompute or move source leaf poses");
    }
  }
  demand.AxisSegments = 2;
  CHECK(!MeasurePreparedPlant(*plan, blades, demand),
        "fixed and error-selected axis modes cannot ambiguously overlap");
  return Report();
}
