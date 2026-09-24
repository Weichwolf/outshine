#include "Check.h"
#include "TerrainRefinement.h"
#include "TileGeodesy.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;

  constexpr TerrainPageLayout layout{.Side = 33, .Halo = 1};
  constexpr Data::TileId tile{.Zoom = 14, .X = 8500, .Y = 5600};
  const Ground::GeoBounds bounds = Ground::TileBounds(tile);
  const auto point = [&](double fraction) {
    return LongitudeLatitude{
        .LongitudeDeg = bounds.MinLonDeg + fraction * (bounds.MaxLonDeg - bounds.MinLonDeg),
        .LatitudeDeg = bounds.MinLatDeg + fraction * (bounds.MaxLatDeg - bounds.MinLatDeg)};
  };
  const TangentFrame frame = TangentFrame::At(point(0.5));
  const auto eastNorth = [&](double fraction) {
    const EastNorthUp local = frame.ToLocalPosition({.LongitudeDeg = point(fraction).LongitudeDeg,
                                                     .LatitudeDeg = point(fraction).LatitudeDeg,
                                                     .HeightM = 0.0});
    return EastNorth{.EastM = local.EastM, .NorthM = local.NorthM};
  };
  const Sheet page{.Tile = tile, .Side = layout.Side, .Postings = layout.Side};
  Ground::TerrainField flat(129, 129);
  const std::array sources{TerrainRefinementSource{.Page = &page, .Heights = &flat}};
  const TerrainRefinementDetail detail{.OrthographicPxPerM = 1.0, .ErrorPx = 0.1};
  const TerrainRefinementCorridor road{
      .Start = eastNorth(0.15), .End = eastNorth(0.35), .HalfWidthM = 1.0, .MaximumPostingM = 4.0};
  const std::array corridors{road};

  const auto withoutRoad = RefineTerrain(sources, frame, layout, detail, 32);
  CHECK(withoutRoad && withoutRoad->size() == 1,
        "flat terrain remains one source patch without a road corridor");
  const auto refined = RefineTerrain(sources, frame, layout, detail, 32, corridors);
  CHECK(refined && refined->size() > 4 && refined->size() < 16,
        "road corridor refines local ground without refining its full bounding tile");
  if (refined) {
    size_t finest = 0;
    size_t coarse = 0;
    for (const Sheet &sheet : *refined) {
      finest += sheet.Tile.Zoom == tile.Zoom + 2;
      coarse += sheet.Tile.Zoom == tile.Zoom + 1;
    }
    CHECK(finest > 0 && coarse > 0,
          "selected ground has fine road patches and coarse distant patches");
  }
  const auto overBudget = RefineTerrain(sources, frame, layout, detail, 4, corridors);
  CHECK(!overBudget && overBudget.error().find("height patches") != std::string::npos,
        "a corridor that exceeds page capacity fails explicitly");

  TerrainRefinementCorridor invalid = road;
  invalid.MaximumPostingM = std::numeric_limits<double>::quiet_NaN();
  const std::array invalidCorridors{invalid};
  const auto rejected = RefineTerrain(sources, frame, layout, detail, 32, invalidCorridors);
  CHECK(!rejected && rejected.error().find("invalid corridor") != std::string::npos,
        "non-finite corridor input fails before refinement");

  TerrainRefinementJob paced(sources, frame, layout, detail, 32, corridors);
  const auto advanced = paced.Advance(1);
  CHECK(advanced && *advanced && refined && std::move(paced).Take() == *refined,
        "paced and direct corridor refinement select identical patches");
  return Report();
}
