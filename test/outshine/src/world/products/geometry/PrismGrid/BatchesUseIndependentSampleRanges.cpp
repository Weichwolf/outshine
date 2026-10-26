#include "PrismGrid.h"
#include "Check.h"
#include <limits>
#include <memory>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
}

int main() {
  constexpr float elevatedBase = 100;
  constexpr float elevatedTop = 120;
  constexpr float reusedFill = .25f;
  const auto values = std::make_shared<const std::vector<PrismSpan>>(
      std::vector<PrismSpan>{{.Lower = std::numeric_limits<float>::quiet_NaN(), .Upper = 0},
                             {.Lower = -4, .Upper = -1},
                             {.Lower = 2, .Upper = 8},
                             {.Lower = 6, .Upper = 5},
                             {.Lower = elevatedBase, .Upper = elevatedTop}});
  PrismGrid grid{
      .Samples = values,
      .Patches = {{.FirstSample = 1, .Columns = 3, .Rows = 1, .PitchX = 4},
                  {.FirstSample = 4, .Columns = 1, .Rows = 1, .PitchZ = 7},
                  {.FirstSample = 1, .Columns = 3, .Rows = 1, .FillFraction = reusedFill}}};
  const auto info = InspectPrismGrid(grid);
  CHECK(info && info->Patches.size() == 3,
        "one packed input can supply several independent patches");
  if (!info) { return Report(); }
  CHECK(info->SampleBytes == 40 && info->ReferencedSamples == 7 && info->Occupied == 5,
        "shared storage bytes stay distinct from instanced sample and primitive counts");
  CHECK(info->Patches[0].Lower == -4 && info->Patches[0].Upper == 8 &&
            info->Patches[1].Lower == elevatedBase && info->Patches[1].Upper == elevatedTop,
        "each patch retains its own local vertical interval without reading unreferenced samples");
  CHECK(info->Patches[0].Width == 12 && info->Patches[2].Width == 3 && info->Patches[1].Depth == 7,
        "reused samples can have independent pitch and fill in different patch frames");
  CHECK(InspectPrismGrid({}), "an empty batch needs no sample allocation");
  const PrismGrid missing{.Samples = {}, .Patches = {{.Columns = 1, .Rows = 1}}};
  CHECK(!InspectPrismGrid(missing), "declared patches cannot silently omit their sample ownership");
  grid.Patches[1].FirstSample = 5;
  CHECK(!InspectPrismGrid(grid), "one invalid range rejects the complete registration");
  return Report();
}
