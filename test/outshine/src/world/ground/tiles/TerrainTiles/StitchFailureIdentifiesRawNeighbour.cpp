#include "Check.h"
#include "tiles/TerrainTiles.h"

#include <fstream>
#include <iterator>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Ground;

class MissingNeighbour final : public TerrainSource {
public:
  explicit MissingNeighbour(Data::TileId missing) : Missing_(missing) {
    std::ifstream input("test/outshine/data/terrain/hockenheim/17164-11206.png", std::ios::binary);
    Png_ = {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  }

  TerrainBytes Take(Data::TileId at) override {
    if (at == Missing_) {
      return TerrainBytes::Wire(Data::FetchFailure{
          .Kind = Data::DataKind::Elevation,
          .Requested = Data::Address::At(at),
          .Served = Data::Address::At({.Zoom = at.Zoom - 1, .X = at.X >> 1u, .Y = at.Y >> 1u}),
          .SourceId = "fixture-provider",
          .SourceRevision = "pin",
          .SourceKey = "fixture-key",
          .Reason = Data::FetchFailureReason::OfflineMiss});
    }
    return TerrainBytes::From(at,
                              Png_,
                              {.Kind = Data::DataKind::Elevation,
                               .Tile = at,
                               .SourceId = "fixture-provider",
                               .Revision = "pin"});
  }

private:
  Data::TileId Missing_;
  std::vector<uint8_t> Png_;
};
}

int main() {
  using namespace outshine::Test;
  for (const Data::TileId missing :
       {Data::TileId{.Zoom = 5, .X = 16, .Y = 11}, Data::TileId{.Zoom = 5, .X = 18, .Y = 12}}) {
    MissingNeighbour source(missing);
    TerrainTiles tiles(source, EnuFrame::At(Geo{}), {.DemCacheBytes = 1024u * 1024u});
    const auto grid = tiles.StitchedGrid(5, 17, 11);
    CHECK(grid.Where() == TerrainGrid::State::Refused && grid.Failure(),
          "missing edge or diagonal prevents publication and owns the raw error");
    if (!grid.Failure()) { continue; }
    const auto &failure = *grid.Failure();
    CHECK(failure.Requested == Data::Address::At(missing) &&
              failure.SourceId == "fixture-provider" && failure.SourceRevision == "pin" &&
              failure.SourceKey == "fixture-key" &&
              failure.Reason == Data::FetchFailureReason::OfflineMiss,
          "aggregate field address never replaces the offending raw neighbour");
    CHECK(failure.Served ==
              Data::Address::At({.Zoom = 4, .X = missing.X >> 1u, .Y = missing.Y >> 1u}),
          "stitching retains the failed neighbour's served ancestor");
  }
  return Report();
}
