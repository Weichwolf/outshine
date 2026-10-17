#include "PreparedPlantBuildFixture.h"
#include "BuildPreparedPlant.h"
#include "PreparedPlant.h"
#include "Check.h"
#include "math/Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

bool SameAxis(const PreparedPlant &whole, const PreparedPlant &part, uint64_t id) {
  const auto first = std::ranges::find(whole.Axes, id, &PreparedPlantAxis::Id);
  const auto second = std::ranges::find(part.Axes, id, &PreparedPlantAxis::Id);
  if (first == whole.Axes.end() || second == part.Axes.end() ||
      first->IndexCount != second->IndexCount) {
    return false;
  }
  for (size_t at = 0; at < first->IndexCount; ++at) {
    const size_t a = whole.Prototypes.trianglesOf(0)[first->FirstIndex + at];
    const size_t b = part.Prototypes.trianglesOf(0)[second->FirstIndex + at];
    for (size_t component = 0; component < 3; ++component) {
      if (whole.Prototypes.positionsOf(0)[a * 3 + component] !=
              part.Prototypes.positionsOf(0)[b * 3 + component] ||
          whole.Prototypes.normalsOf(0)[a * 3 + component] !=
              part.Prototypes.normalsOf(0)[b * 3 + component]) {
        return false;
      }
    }
  }
  return true;
}
}

int main() {
  auto plan = PlantBuildPlan();
  CHECK(plan, "branching plan with a breadth-first sibling is canonical");
  if (!plan) { return Report(); }
  auto blades = PlantBuildBlades();
  const auto materials = PlantBuildMaterials();
  const auto demand = PlantBuildDemand();
  const auto counts = MeasurePreparedPlant(*plan, blades, demand);
  CHECK(counts && counts->Vertices == kProductVertices && counts->Triangles == kProductTriangles &&
            counts->LeafInstances == 3 && counts->TriangleReferences == kDrawTriangles,
        "unique prototypes and instanced draw references have independent exact inventories");
  std::array<uint8_t, 32> identity{};
  identity[0] = 1;
  const auto whole = BuildPreparedPlant(*plan, blades, identity, materials, demand);
  CHECK(whole && PreparedPlantIsValid(*whole), "whole plant assembles at its exact budget");
  if (!whole) { return Report(); }
  CHECK(whole->PlanIdentity == identity && whole->Prototypes.parts() == 3 &&
            whole->Axes.size() == 4 && whole->Leaves.size() == 2 && whole->Leaves[0].Variant == 0 &&
            whole->Leaves[1].Variant == 2 && whole->Leaves[1].Ids.size() == 2,
        "only used sparse variants produce meshes and repeated leaves share a prototype");
  CHECK(whole->Prototypes.images() == 1 &&
            whole->Prototypes.surfaceAt(MaterialInstance(1)).BaseColourMap.Image == 0 &&
            materials.parts() == 0 && materials.surfaces() == 2,
        "native material images survive the owned clone without consuming the source palette");
  auto selected = demand;
  selected.Root = kLeftAxis;
  const auto partialCounts = MeasurePreparedPlant(*plan, blades, selected);
  constexpr size_t kBranchVertices = 52;
  constexpr size_t kBranchTriangles = 52;
  constexpr size_t kBranchReferences = 56;
  CHECK(partialCounts && partialCounts->Vertices == kBranchVertices &&
            partialCounts->Triangles == kBranchTriangles && partialCounts->LeafInstances == 2 &&
            partialCounts->TriangleReferences == kBranchReferences,
        "partial demand counts only the left branch and its one blade prototype");
  const auto partial = BuildPreparedPlant(*plan, blades, identity, materials, selected);
  CHECK(partial && PreparedPlantIsValid(*partial),
        "subtree forms a complete standalone prepared product");
  if (!partial) { return Report(); }
  CHECK(partial->Axes.size() == 2 && partial->Axes[0].Id == kLeftAxis &&
            partial->Axes[1].Id == kTwigAxis && partial->Prototypes.parts() == 2 &&
            partial->Leaves.size() == 1 && partial->Leaves[0].Variant == 2 &&
            partial->Leaves[0].Ids[0] == kLeftLeaf && partial->Leaves[0].Ids[1] == kTwigLeaf,
        "partial product excludes the unrelated sibling, root and unused variants");
  CHECK(SameAxis(*whole, *partial, kLeftAxis) && SameAxis(*whole, *partial, kTwigAxis),
        "requesting a branch does not resample its source geometry");
  const Vec3 point = partial->Leaves[0].Transforms[0].TransformPoint({{1, 0, 0}});
  CHECK(point[0] == 2 && point[1] == 5 && point[2] == -4,
        "column-major leaf placement applies the canonical frame and Z-up to Y-up conversion once");
  selected.Root = kRightAxis;
  CHECK(BuildPreparedPlant(*plan, blades, identity, materials, selected),
        "other branch can be requested independently");
  blades[0] = {};
  selected.Root = kLeftAxis;
  const auto repeated = BuildPreparedPlant(*plan, blades, identity, materials, selected);
  CHECK(repeated && repeated->Leaves[0].Ids == partial->Leaves[0].Ids &&
            SameAxis(*partial, *repeated, kTwigAxis),
        "request order and invalid unused blade recipes cannot change selected source products");
  return Report();
}
