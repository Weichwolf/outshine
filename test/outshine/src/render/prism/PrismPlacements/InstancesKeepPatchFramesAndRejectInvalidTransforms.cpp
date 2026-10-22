#include "PrismPlacements.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
}

int main() {
  const PrismGrid grid{
      .Samples = std::make_shared<const std::vector<PrismSpan>>(
          std::vector<PrismSpan>{{.Lower = -4, .Upper = 6}, {.Lower = 0, .Upper = 0}}),
      .Patches = {{.Columns = 1, .Rows = 1, .PitchX = 2, .PitchZ = 3},
                  {.FirstSample = 1, .Columns = 1, .Rows = 1}}};
  const auto info = InspectPrismGrid(grid);
  CHECK(info, "a shared source has independent occupied and empty patches");
  if (!info) { return Report(); }
  std::array<PrismInstance, 3> instances{};
  instances[0].Patch = 1;
  instances[1].Current[12] = 4;
  instances[1].Previous[12] = 2;
  instances[2].Current[14] = -6;
  instances[2].Previous[14] = -6;
  const auto records = PackPrismPlacements(grid, *info, instances);
  CHECK(records && records->size() == 2, "empty patches do not create GPU draw instances");
  if (!records) { return Report(); }
  CHECK((*records)[0].Current[12] == 4 && (*records)[0].Previous[12] == 2 &&
            (*records)[1].Current[14] == -6,
        "one source range supports independent current and previous placements");
  CHECK((*records)[0].Shape[3] == -4 && (*records)[0].Bounds[0] == 6,
        "neither source floors nor tops are folded into a geographic transform");
  CHECK((*records)[0].Bounds[1] == 2 && (*records)[0].Bounds[2] == 3,
        "rectangular cells retain independent axis pitches");
  auto invalid = instances;
  invalid[1].Patch = 2;
  CHECK(!PackPrismPlacements(grid, *info, invalid), "an invalid patch rejects the entire batch");
  invalid = instances;
  invalid[1].Previous[12] = std::numeric_limits<float>::infinity();
  CHECK(!PackPrismPlacements(grid, *info, invalid), "previous placement must also remain finite");
  invalid = instances;
  invalid[1].Current[0] = 0;
  CHECK(!PackPrismPlacements(grid, *info, invalid),
        "a singular transform cannot define local rays");
  invalid = instances;
  invalid[1].Current[3] = 1;
  CHECK(!PackPrismPlacements(grid, *info, invalid),
        "a perspective placement is not an affine frame");
  return Report();
}
