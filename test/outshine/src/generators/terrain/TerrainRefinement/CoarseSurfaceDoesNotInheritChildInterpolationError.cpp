#include "Check.h"
#include "TerrainRefinement.h"

#include <algorithm>
#include <array>
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr TerrainPageLayout layout{.Side = 33, .Halo = 1};
  const Data::TileId tile{.Zoom = 10, .X = 512, .Y = 512};
  const Sheet page{.Tile = tile, .Nodes = {}, .Side = layout.Side, .Postings = 33};
  Ground::TerrainField relief(129, 129);
  double maximumFromPlaneM = 0.0;
  for (uint32_t row = 0; row < 129; ++row) {
    for (uint32_t column = 0; column < 129; ++column) {
      float heightM = -0.75f;
      if (row % 2 == 0 && column % 2 == 0) { heightM = 0.75f; }
      if (row % 4 == 0 && column % 4 == 0) { heightM = 0.0f; }
      relief.SetM(row, column, heightM);
      maximumFromPlaneM = std::max(maximumFromPlaneM, std::abs(static_cast<double>(heightM)));
    }
  }
  CHECK(maximumFromPlaneM == 0.75, "every reference vertex is within 0.75 m of the coarse plane");
  const std::array sources{TerrainRefinementSource{.Page = &page, .Heights = &relief}};
  const TerrainRefinementDetail coarse{.OrthographicPxPerM = 1.0, .ErrorPx = 1.0};
  const auto kept = RefineTerrain(sources, TangentFrame::At({}), layout, coarse, 1);
  CHECK(kept && kept->size() == 1 && kept->front().Tile == tile && !kept->front().Virtual,
        "a valid coarse plane is not rejected because a different child surface has larger error");
  const TerrainRefinementDetail detailed{.OrthographicPxPerM = 1.0, .ErrorPx = 0.5};
  const auto refined = RefineTerrain(sources, TangentFrame::At({}), layout, detailed, 16);
  CHECK(refined && refined->size() == 16,
        "a stricter bound still selects the sixteen patches that retain every source posting");
  if (refined) {
    for (const Sheet &patch : *refined) {
      CHECK(patch.Tile.Zoom == tile.Zoom + 2 && patch.Virtual && patch.SourceZoom == tile.Zoom,
            "the detailed result retains its source and finest required grid");
    }
  }
  return Report();
}
