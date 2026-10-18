#include "PlantRecipeFixture.h"
#include "PlantRecipe.h"
#include "PlantPlan.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr size_t kAxes = 10;
  constexpr size_t kLeaves = 32;
  const auto recipe = PlantRecipeFixture();
  PlantRecipeDemand demand{.MaximumAxes = kAxes, .MaximumLeaves = kLeaves};
  const auto size = MeasurePlantRecipe(recipe, demand);
  CHECK(size && size->Root == 0 && size->Axes == kAxes && size->Leaves == kLeaves,
        "complete recipe counts are known before axes or leaf placements are generated");
  const auto full = BuildPlantPlan(recipe, demand);
  CHECK(full, "canonical recipe builds at its exact counts");
  if (!full) { return Report(); }
  demand.Path[0] = 1;
  demand.MaximumAxes = 4;
  demand.MaximumLeaves = 9;
  const auto selected = MeasurePlantRecipe(recipe, demand);
  CHECK(selected && selected->Root == 2 && selected->Axes == 4 && selected->Leaves == 9,
        "branch demand includes one necessary ancestor and excludes its leaves and siblings");
  const auto part = BuildPlantPlan(recipe, demand);
  CHECK(part && part->Axes().size() == 4,
        "bounded branch recipe builds without the rest of the plant");
  if (!part) { return Report(); }
  constexpr std::array<uint64_t, 4> kIds{0, 2, 6, 7};
  for (size_t at = 0; at < kIds.size(); ++at) {
    const auto &axis = part->Axes()[at];
    const auto *original = full->Axis(kIds[at]);
    CHECK(axis.Id == kIds[at] && original && axis.Parent == original->Parent &&
              axis.Path == original->Path && axis.Frame == original->Frame &&
              axis.OriginM == original->OriginM && axis.Bend == original->Bend &&
              axis.LengthM == original->LengthM,
          "subset axes retain global source identity, coordinates and deterministic growth");
    const auto own = part->Leaves({.Axis = axis.Id, .Maximum = kLeaves});
    if (at == 0) {
      CHECK(own && own->empty() && !full->Leaves({.Axis = 0, .Maximum = kLeaves})->empty(),
            "ancestor geometry needs no unrequested ancestor leaf placements");
      continue;
    }
    const auto reference = full->Leaves({.Axis = axis.Id, .Maximum = kLeaves});
    CHECK(own && reference && own->size() == reference->size(),
          "requested branch owns the same source leaf count");
    if (!own || !reference || own->size() != reference->size()) { return Report(); }
    for (size_t leaf = 0; leaf < own->size(); ++leaf) {
      const auto &a = (*own)[leaf];
      const auto &b = (*reference)[leaf];
      CHECK(a.Id == b.Id && a.Axis == b.Axis && a.Variant == b.Variant && a.Frame == b.Frame &&
                a.NodeM == b.NodeM && a.OriginM == b.OriginM && a.StemParameter == b.StemParameter,
            "requesting a branch does not renumber or resample its leaves");
    }
  }
  const auto summary = part->Summarize(selected->Root);
  CHECK(summary && summary->Axes == 3 && summary->Leaves == 9,
        "product-root inventory excludes its extra construction ancestor");
  const auto original = part->Leaves({.Axis = 2, .Maximum = kLeaves});
  CHECK(original && !original->empty() && original->front().Id == 9,
        "global leaf offset includes skipped source rows instead of restarting at zero");
  auto bare = recipe;
  bare.Blades.clear();
  for (auto &level : bare.Levels) { level.LeafInternodeM.reset(); }
  demand = {.MaximumAxes = kAxes, .MaximumLeaves = 0};
  const auto wood = BuildPlantPlan(bare, demand);
  CHECK(wood && wood->Summarize(0)->Variants.empty() && wood->Summarize(0)->Leaves == 0,
        "bladeless recipes retain wood topology without invented blade variants");
  constexpr double kLongBareAxisM = 1e20;
  bare.Levels.front().LengthM = kLongBareAxisM;
  CHECK(MeasurePlantRecipe(bare, demand), "bare axis length does not imply nonexistent leaves");
  return Report();
}
