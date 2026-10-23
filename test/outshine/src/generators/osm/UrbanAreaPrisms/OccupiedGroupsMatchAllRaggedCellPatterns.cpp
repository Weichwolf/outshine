#include "UrbanAreaGroups.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
using namespace outshine::Generators::Osm;
using namespace outshine::Test;

struct PatternCase {
  uint32_t Columns = 0, Rows = 0, PatchSide = 0, PatchesPerSide = 0;
};

void CheckPatterns(PatternCase input) {
  const auto [columns, rows, patchSide, patchesPerSide] = input;
  UrbanArea area;
  area.Columns = columns;
  area.Rows = rows;
  area.Recipe.PatchSide = patchSide;
  area.Tiers.resize(static_cast<size_t>(columns) * rows);
  const auto groups =
      UrbanAreaGroups::Make(area, {.PatchesPerSide = patchesPerSide, .GroupsMost = 64});
  CHECK(groups, "bounded ragged field has an intrinsic group layout");
  if (!groups) { return; }
  const uint32_t side = patchSide * patchesPerSide;
  const uint32_t across = (columns + side - 1) / side;
  const uint64_t patterns = uint64_t{1} << area.Tiers.size();
  for (uint64_t pattern = 0; pattern < patterns; ++pattern) {
    std::vector<size_t> expected;
    std::vector<bool> occupied(groups->Size());
    for (size_t cell = 0; cell < area.Tiers.size(); ++cell) {
      area.Tiers[cell] = (pattern & (uint64_t{1} << cell)) != 0
                             ? static_cast<uint8_t>(1 + cell % (UrbanArea::HeightsM.size() - 1))
                             : 0;
      if (area.Tiers[cell] != 0) {
        const size_t column = cell % columns;
        const size_t row = cell / columns;
        occupied[(row / side) * across + column / side] = true;
      }
    }
    for (size_t group = 0; group < occupied.size(); ++group) {
      if (occupied[group]) { expected.push_back(group); }
    }
    const auto actual = groups->Occupied(area);
    CHECK(actual && *actual == expected,
          "every cell pattern preserves exact nonempty group identity and row-major order");
  }
}

void CheckMismatches() {
  UrbanArea area;
  const auto empty = UrbanAreaGroups::Make(area, {.GroupsMost = 0});
  CHECK(empty && empty->Occupied(area) && empty->Occupied(area)->empty(),
        "an empty implicit field needs no occupancy entries or payload allocation");
  area.Columns = 3;
  area.Rows = 2;
  area.Recipe.PatchSide = 1;
  area.Tiers = {0, 0, 0, 0, 0, 4};
  const auto groups = UrbanAreaGroups::Make(area, {.PatchesPerSide = 2, .GroupsMost = 2});
  CHECK(groups, "ragged occupancy fixture has two groups");
  if (!groups) { return; }
  const auto original = groups->Occupied(area);
  CHECK(original && *original == std::vector<size_t>{1},
        "the last ragged cell selects its original group without renumbering");
  area.Tiers.pop_back();
  CHECK(!groups->Occupied(area), "an incomplete field is rejected before scanning");
  area.Tiers.push_back(4);
  area.Columns = 2;
  CHECK(!groups->Occupied(area), "an index cannot read another field layout");
  area.Columns = 3;
  area.Recipe.PatchSide = 2;
  CHECK(!groups->Occupied(area), "a changed patch layout requires its own index");
  area.Recipe.PatchSide = 1;
  std::ranges::fill(area.Tiers, 0);
  const auto replaced = groups->Occupied(area);
  CHECK(original && *original == std::vector<size_t>{1} && replaced && replaced->empty(),
        "replacement builds an independent index and cannot mutate the previous one");
}
}

int main() {
  CheckPatterns({.Columns = 3, .Rows = 2, .PatchSide = 1, .PatchesPerSide = 2});
  CheckPatterns({.Columns = 3, .Rows = 5, .PatchSide = 2, .PatchesPerSide = 1});
  CheckMismatches();
  return Report();
}
