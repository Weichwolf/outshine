#include "PrismGrid.h"
#include "Check.h"
#include <array>
#include <cstddef>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
}

int main() {
  const std::array samples{PrismSpan{.Lower = -10, .Upper = -2},
                           PrismSpan{.Lower = -3, .Upper = -4},
                           PrismSpan{.Lower = 4, .Upper = 8},
                           PrismSpan{.Lower = 0, .Upper = 0},
                           PrismSpan{.Lower = 9, .Upper = 8},
                           PrismSpan{.Lower = 1, .Upper = 3}};
  const PrismPatch patch{.Columns = 3, .Rows = 2, .PitchX = 2, .PitchZ = 5, .FillFraction = .5f};
  const auto info = InspectPrismPatch(patch, samples);
  CHECK(info, "a ragged anisotropic patch supports independent negative and positive bases");
  if (!info) { return Report(); }
  CHECK(info->Samples == 6 && info->Occupied == 3, "nonpositive intervals are empty");
  CHECK(info->Width == 6 && info->Depth == 10 && info->Lower == -10 && info->Upper == 8,
        "bounds include complete prisms in the patch's local metre frame");
  for (size_t i = 0; i < samples.size(); ++i) {
    const auto &cell = samples[i];
    if (cell.Upper <= cell.Lower) { continue; }
    const double centreX = (static_cast<double>(i % patch.Columns) + .5) * patch.PitchX;
    const size_t row = i / patch.Columns;
    const double centreZ = (static_cast<double>(row) + .5) * patch.PitchZ;
    for (const double sign : {-1., 1.}) {
      const double x = centreX + sign * patch.PitchX * patch.FillFraction * .5;
      const double z = centreZ + sign * patch.PitchZ * patch.FillFraction * .5;
      CHECK(x >= 0 && x <= info->Width && z >= 0 && z <= info->Depth,
            "outer footprint corners stay in the declared conservative raster rectangle");
    }
    CHECK(cell.Lower >= info->Lower && cell.Upper <= info->Upper,
          "both physical endpoints are retained without a height-zero assumption");
  }
  const std::array empty{PrismSpan{.Lower = 2, .Upper = 2}, PrismSpan{.Lower = 3, .Upper = -5}};
  const auto vacant = InspectPrismPatch({.Columns = 2, .Rows = 1}, empty);
  CHECK(vacant && vacant->Occupied == 0 && vacant->Lower == 0 && vacant->Upper == 0,
        "an explicitly empty patch has finite empty bounds");
  const std::array fractional{PrismSpan{.Lower = 0, .Upper = 1},
                              PrismSpan{.Lower = 0, .Upper = 1},
                              PrismSpan{.Lower = 0, .Upper = 1},
                              PrismSpan{.Lower = 0, .Upper = 1},
                              PrismSpan{.Lower = 0, .Upper = 1}};
  const PrismPatch fractionalPitch{.Columns = 5, .Rows = 1, .PitchX = .1f};
  const auto rounded = InspectPrismPatch(fractionalPitch, fractional);
  CHECK(rounded && static_cast<double>(rounded->Width) >=
                       static_cast<double>(fractionalPitch.Columns) * fractionalPitch.PitchX,
        "narrowing an outward raster bound never trims a complete last-cell footprint");
  return Report();
}
