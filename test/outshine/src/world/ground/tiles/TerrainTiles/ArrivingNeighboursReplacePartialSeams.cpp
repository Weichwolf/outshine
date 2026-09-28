#include "tiles/TerrainTiles.h"
#include "Check.h"
#include <cstdint>

namespace {
using namespace outshine;
using namespace outshine::Ground;
constexpr uint8_t kPng[]{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x08, 0x02, 0x00, 0x00, 0x00, 0x4b,
    0x6d, 0x29, 0xdc, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x68,
    0x60, 0x60, 0xc0, 0x8a, 0xb0, 0x8b, 0x0e, 0x5a, 0x09, 0x00, 0xa1, 0x7c, 0x20, 0x01, 0x64,
    0xc6, 0x93, 0x18, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

class ArrivingNeighbours final : public TerrainSource {
public:
  bool Ready = false;

  TerrainBytes Take(Data::TileId at) override {
    if (!Ready && at != Data::TileId{.Zoom = 2, .X = 1, .Y = 1}) { return TerrainBytes::Nothing(); }
    return TerrainBytes::From(at,
                              {kPng, kPng + sizeof(kPng)},
                              {.Kind = Data::DataKind::Elevation,
                               .Tile = at,
                               .SourceId = "arriving-dem",
                               .Revision = "r1"});
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  ArrivingNeighbours source;
  TerrainTiles tiles(source,
                     EnuFrame::At(Geo{}),
                     {.DemCacheBytes = 1024u * 1024u, .StitchedFieldBytes = 1024u * 1024u});
  const Data::TileId at{.Zoom = 2, .X = 1, .Y = 1};
  const auto partial = tiles.StitchedField(at.Zoom, at.X, at.Y);
  CHECK(partial && partial->Sources().size() == 1,
        "missing neighbours preserve usable centre heights");
  CHECK(!tiles.HeldStitched(at), "partial seams do not become a permanent stitched-cache hit");
  source.Ready = true;
  const auto complete = tiles.StitchedField(at.Zoom, at.X, at.Y);
  CHECK(complete && complete->Sources().size() == 9,
        "arriving neighbours replace the partial source set");
  CHECK(complete && tiles.HeldStitched(at) == complete,
        "complete boundary products remain reusable");
  CHECK(partial && partial->Sources().size() == 1,
        "an already borrowed partial snapshot stays immutable");
  return Report();
}
