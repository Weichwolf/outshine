#include "Check.h"
#include "tiles/TerrainTiles.h"

#include <fstream>
#include <iterator>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Ground;

class PayloadSource final : public TerrainSource {
public:
  enum class Fault { BrokenPng, TooSmallCrop, ForeignAncestor, HigherZoom };

  PayloadSource(Data::TileId bad, Fault fault) : Bad_(bad), Fault_(fault) {
    std::ifstream input("test/outshine/data/terrain/hockenheim/17164-11206.png", std::ios::binary);
    Png_ = {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  }

  TerrainBytes Take(Data::TileId at) override {
    Data::TileId served = at;
    auto png = Png_;
    if (at == Bad_) {
      switch (Fault_) {
        case Fault::BrokenPng: png = {0, 1, 2, 3}; break;
        case Fault::TooSmallCrop: served = {.Zoom = 0, .X = 0, .Y = 0}; break;
        case Fault::ForeignAncestor: served.X += 1; break;
        case Fault::HigherZoom: served.Zoom += 1; break;
      }
    }
    return TerrainBytes::From(served,
                              std::move(png),
                              {.Tile = served, .SourceId = "fallback-provider", .Revision = "pin"},
                              "actual-source-key");
  }

private:
  Data::TileId Bad_;
  Fault Fault_;
  std::vector<uint8_t> Png_;
};
}

int main() {
  using namespace outshine::Test;
  const Data::TileId centre{.Zoom = 9, .X = 270, .Y = 175};
  for (const auto fault : {PayloadSource::Fault::BrokenPng,
                           PayloadSource::Fault::TooSmallCrop,
                           PayloadSource::Fault::ForeignAncestor,
                           PayloadSource::Fault::HigherZoom}) {
    for (const auto bad : {centre,
                           Data::TileId{.Zoom = 9, .X = 269, .Y = 175},
                           Data::TileId{.Zoom = 9, .X = 271, .Y = 176}}) {
      std::optional<Data::FetchFailure> held;
      {
        PayloadSource source(bad, fault);
        TerrainTiles tiles(source, EnuFrame::At(Geo{}), {.DemCacheBytes = 1024u * 1024u});
        auto grid = tiles.StitchedGrid(centre.Zoom, centre.X, centre.Y);
        CHECK(grid.Where() == TerrainGrid::State::Refused && !grid.TryField() && grid.Failure(),
              "invalid centre, edge or diagonal payload cannot publish a field");
        held = grid.Failure();
      }
      CHECK(held && held->Requested == Data::Address::At(bad) &&
                held->Kind == Data::DataKind::Elevation &&
                held->Reason == Data::FetchFailureReason::CorruptPayload &&
                held->SourceId == "fallback-provider" && held->SourceRevision == "pin" &&
                held->SourceKey == "actual-source-key",
            "corrupt failure owns actual raw provenance beyond decoder/source lifetime");
      Data::TileId served = bad;
      switch (fault) {
        case PayloadSource::Fault::BrokenPng: break;
        case PayloadSource::Fault::TooSmallCrop: served = {.Zoom = 0, .X = 0, .Y = 0}; break;
        case PayloadSource::Fault::ForeignAncestor: served.X += 1; break;
        case PayloadSource::Fault::HigherZoom: served.Zoom += 1; break;
      }
      CHECK(held && held->Served == Data::Address::At(served),
            "reported served tile is the delivered tile even when it violates ancestry");
    }
  }
  return Report();
}
