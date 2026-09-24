#include "TileWatermark.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <cstdint>

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;

  const std::array<OsmField::Feature, 2> features{{{.Tile = 0}, {.Tile = 1}}};
  const std::array<OsmField::Tile, 2> tiles{{{.Z = 14, .X = 0, .Y = 1}, {.Z = 14, .X = 1, .Y = 1}}};
  TileWatermark source;
  source.Take(0);
  source.Take(1);
  source.Advance(features);
  CHECK(source.Done(features) && source.Takes() == 2,
        "the scan can pass two reserved tiles before either product is accepted");

  TileWatermark snapshot = source;
  const std::array<uint32_t, 1> accepted{0};
  CHECK(snapshot.ReleaseUnaccepted(accepted) == 1 && snapshot.Takes() == 1 &&
            !snapshot.Done(features),
        "the snapshot releases a pending reservation even behind the scan watermark");
  const TileWatermark::Next retry = snapshot.Ask(features,
                                                 tiles,
                                                 {.CentreX = 0, .CentreY = 1, .Rings = kEveryRing},
                                                 [](size_t, size_t) { return true; });
  CHECK(retry.Found && retry.Tile == 1,
        "the snapshot retries only the tile without an accepted product");
  CHECK(source.Done(features) && source.Takes() == 2,
        "the source retains both reservations and its scan position");
  return Report();
}
