#include "Check.h"
#include "UrbanAreaGroups.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
using namespace outshine::Test;
namespace Osm = outshine::Generators::Osm;

void CheckShape(uint32_t columns, uint32_t rows, uint32_t patchSide, uint32_t perSide) {
  Osm::UrbanArea metadata;
  metadata.Columns = columns;
  metadata.Rows = rows;
  metadata.Recipe.PatchSide = patchSide;
  const auto groups =
      Osm::UrbanAreaGroups::Make(metadata, {.PatchesPerSide = perSide, .GroupsMost = 10000});
  CHECK(groups, "intrinsic grouping needs dimensions and recipe, without any cell payload");
  if (!groups) { return; }
  std::vector<uint8_t> visits(static_cast<size_t>(columns) * rows);
  size_t cells = 0;
  for (size_t index = 0; index < groups->Size(); ++index) {
    const auto bounds = groups->At(index);
    const auto patches =
        groups->Patches({.Index = index, .PatchesMost = static_cast<size_t>(perSide) * perSide});
    CHECK(bounds && patches, "each metadata group expands only its bounded local patch ranges");
    if (!bounds || !patches) { return; }
    for (const auto &patch : *patches) {
      CHECK(patch.Columns > 0 && patch.Columns <= patchSide && patch.Rows > 0 &&
                patch.Rows <= patchSide && patch.Column >= bounds->Column &&
                patch.Row >= bounds->Row && patch.Column + patch.Columns <= columns &&
                patch.Row + patch.Rows <= rows && patch.Column % patchSide == 0 &&
                patch.Row % patchSide == 0,
            "groups preserve original patch boundaries, including ragged field edges");
      for (uint32_t row = patch.Row; row < patch.Row + patch.Rows; ++row) {
        for (uint32_t column = patch.Column; column < patch.Column + patch.Columns; ++column) {
          ++visits[static_cast<size_t>(row) * columns + column];
          ++cells;
        }
      }
    }
  }
  CHECK(cells == visits.size() && std::ranges::all_of(visits, [](uint8_t n) { return n == 1; }),
        "every field cell belongs to exactly one group and patch, with no holes or overlap");
  CHECK(!groups->At(groups->Size()) &&
            !groups->Patches({.Index = groups->Size(), .PatchesMost = 100}),
        "out of range metadata never addresses an unrelated cached group");
  CHECK(!Osm::UrbanAreaGroups::Make(metadata,
                                    {.PatchesPerSide = perSide, .GroupsMost = groups->Size() - 1}),
        "an insufficient group budget refuses before allocating group metadata");
  CHECK(!groups->Patches({}), "selected patch expansion has its own explicit budget");
}
}

int main() {
  CheckShape(616, 624, 8, 8);
  CheckShape(17, 19, 8, 2);
  CheckShape(1, 1, 1, 1);
  CheckShape(31, 67, 3, 3);
  Osm::UrbanArea empty;
  const auto groups = Osm::UrbanAreaGroups::Make(empty, {.PatchesPerSide = 8});
  CHECK(groups && groups->Size() == 0 && !groups->At(0),
        "an empty source has no groups and needs no phantom origin product");
  CHECK(!Osm::UrbanAreaGroups::Make(empty, {.PatchesPerSide = 0, .GroupsMost = 1}) &&
            !Osm::UrbanAreaGroups::Make(
                empty, {.PatchesPerSide = std::numeric_limits<uint32_t>::max(), .GroupsMost = 1}),
        "zero and overflowing group dimensions are invalid even without payloads");
  empty.Columns = 1;
  CHECK(!Osm::UrbanAreaGroups::Make(empty, {.GroupsMost = 1}),
        "incomplete field dimensions are invalid");
  empty.Columns = empty.Rows = std::numeric_limits<uint32_t>::max();
  empty.Recipe.PatchSide = 1;
  const auto extreme = Osm::UrbanAreaGroups::Make(
      empty, {.PatchesPerSide = std::numeric_limits<uint32_t>::max(), .GroupsMost = 1});
  CHECK(extreme && extreme->Size() == 1 && !extreme->Patches({.PatchesMost = 64}),
        "huge implicit grids stay bounded without iterating, reading or allocating child data");
  return Report();
}
