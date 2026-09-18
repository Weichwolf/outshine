#include "Check.h"
#include "TerrainRefinement.h"

#include <array>
#include <cstddef>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;

  constexpr TerrainPageLayout layout{.Side = 33, .Halo = 1};
  constexpr Data::TileId tile{.Zoom = 10, .X = 512, .Y = 512};
  const Sheet page{.Tile = tile, .Nodes = {}, .Side = layout.Side, .Postings = 33};
  Ground::TerrainField flat(65, 65);
  const std::array flatSource{TerrainRefinementSource{.Page = &page, .Heights = &flat}};
  const TerrainRefinementDetail detail{.OrthographicPxPerM = 1.0, .ErrorPx = 0.1};
  const auto coarse = RefineTerrain(flatSource, TangentFrame::At({}), layout, detail, 8);
  CHECK(coarse && coarse->size() == 1 && coarse->front().Tile == tile && !coarse->front().Virtual &&
            coarse->front().SourceZoom == tile.Zoom,
        "a flat high-resolution source remains one native patch");

  Ground::TerrainField spike(65, 65);
  spike.SetM(32, 32, 100.0f);
  const std::array spikeSource{TerrainRefinementSource{.Page = &page, .Heights = &spike}};
  const auto refined = RefineTerrain(spikeSource, TangentFrame::At({}), layout, detail, 4);
  CHECK(refined && refined->size() == 4,
        "an error above the declared pixel bound selects four complete child patches");
  if (refined) {
    for (size_t child = 0; child < refined->size(); ++child) {
      const Sheet &selected = (*refined)[child];
      CHECK(selected.Tile.Zoom == tile.Zoom + 1 && selected.Tile.X == tile.X * 2 + child % 2 &&
                selected.Tile.Y == tile.Y * 2 + child / 2 && selected.Virtual &&
                selected.SourceZoom == tile.Zoom,
            "child identity and source ownership remain deterministic");
    }
  }
  const auto overBudget = RefineTerrain(spikeSource, TangentFrame::At({}), layout, detail, 3);
  CHECK(!overBudget && overBudget.error().find("needs 4 height patches") != std::string::npos,
        "a complete over-budget result is rejected instead of truncated");

  const Sheet virtualPage{.Tile = {.Zoom = 11, .X = 1024, .Y = 1024},
                          .Nodes = {},
                          .Side = layout.Side,
                          .Postings = 33,
                          .Virtual = true,
                          .SourceZoom = 10};
  const std::array virtualSource{TerrainRefinementSource{.Page = &virtualPage, .Heights = nullptr}};
  const auto retained = RefineTerrain(virtualSource, TangentFrame::At({}), layout, detail, 1);
  CHECK(retained && retained->size() == 1 && retained->front().Tile == virtualPage.Tile &&
            retained->front().Virtual,
        "an already virtual patch passes through without a terrain provider");
  return Report();
}
