#include "TerrainSeams.h"
#include "GroundMesher.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Terrain;
  using namespace outshine::Test;
  Patchwork empty;
  const auto absent = StitchTerrainSeams(empty);
  CHECK(absent && absent->Virtual.Edges == 0 && absent->Real.Edges == 0,
        "empty terrain has no seam work");
  constexpr auto pageSide = static_cast<size_t>(kPatchGrid) + 3;
  Sheet page;
  page.Tile = {.Zoom = 5, .X = 3, .Y = 4};
  page.Nodes.assign(pageSide * pageSide, 12.0f);
  Patchwork repeated;
  repeated.Sheets = {page, page};
  const auto invalid = StitchTerrainSeams(repeated);
  CHECK(!invalid && invalid.error() == "ground patchwork repeats tile",
        "duplicate pages fail through the typed error contract");
  CHECK(std::ranges::all_of(repeated.Sheets,
                            [&page](const Sheet &sheet) { return sheet.Nodes == page.Nodes; }),
        "index failure leaves all input samples untouched");
  return Report();
}
