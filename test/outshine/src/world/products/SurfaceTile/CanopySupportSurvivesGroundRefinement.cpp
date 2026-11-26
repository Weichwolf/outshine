#include "Check.h"
#include "SurfaceTile.h"
#include "TileId.h"
#include <array>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Data::TileId parent{.Zoom = 2, .X = 1, .Y = 2};
  const Data::TileId site{.Zoom = 4, .X = 4, .Y = 10};
  SurfaceTile coarse;
  coarse.Coverage = parent;
  const auto initial = SurfaceCoverage(std::span(&coarse, 1));
  CHECK(initial && SurfaceCoversTile(*initial, site), "the coarse surface supports the fine site");

  std::array<SurfaceTile, 4> children;
  children[0].Coverage = {.Zoom = 3, .X = 2, .Y = 4};
  children[1].Coverage = {.Zoom = 3, .X = 3, .Y = 4};
  children[2].Coverage = {.Zoom = 3, .X = 3, .Y = 5};
  children[3].Coverage = {.Zoom = 3, .X = 2, .Y = 5};
  const auto refined = SurfaceCoverage(children);
  CHECK(refined == initial && refined && SurfaceCoversTile(*refined, site),
        "the full partition keeps the same site supported");
  const auto partial = SurfaceCoverage(std::span(children).first(3));
  CHECK(partial && !SurfaceCoversTile(*partial, site), "a detached site has no surface support");
  return Report();
}
