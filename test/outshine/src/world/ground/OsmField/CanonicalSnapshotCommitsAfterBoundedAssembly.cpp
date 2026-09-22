#include "OsmField.h"
#include "SourceSet.h"
#include "Check.h"
#include "test/outshine/src/world/ground/OsmVector/WireFixture.h"

#include <algorithm>
#include <array>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;

class NoTransport final : public Transport {
public:
  Ticket Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};

class VectorSource final : public Source {
public:
  explicit VectorSource(std::vector<uint8_t> bytes) : Bytes(std::move(bytes)) {}

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  Ticket Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Delivered(Bytes);
  }

private:
  SourceDecl Decl{.Id = "vector-assembly",
                  .Revision = "r1",
                  .Kind = DataKind::VectorMap,
                  .Wire = WireFormat::MapboxVectorTile,
                  .Keeps = Cacheability::Never};
  std::vector<uint8_t> Bytes;
};

std::vector<uint8_t> TileBytes() {
  using outshine::Test::Mvt::Append;
  using outshine::Test::Mvt::Bytes;
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  Append(layer, 0x12, Bytes{0x18, 2, 0x22, 6, 9, 0, 0, 10, 2, 2});
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  const auto bytes = TileBytes();
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<VectorSource>(bytes)) == SourceSet::Registration::Accepted,
        "resident vector source registers");
  NoTransport transport;
  Ground::TilePool pool({.ByteBudget = 1u << 20u}, sources, transport);
  for (uint32_t y = 7; y <= 9; ++y) {
    for (uint32_t x = 7; x <= 9; ++x) {
      Ground::TilePool::Landing landed;
      const Fetch request(DataKind::VectorMap, Address::At({.Zoom = 4, .X = x, .Y = y}));
      CHECK(pool.BytesBlocking(request, &landed) == Ground::TilePool::Reply::Ready,
            "all requested source tiles are resident before assembly");
    }
  }
  const std::array<std::string, 1> layers{"x"};
  Ground::OsmField staged(4, layers);
  Ground::OsmField direct(4, layers);
  for (int y = 9; y >= 7; --y) {
    for (int x = 9; x >= 7; --x) {
      CHECK(direct.Accept(x, y, bytes).has_value(), "direct oracle accepts reverse tile order");
    }
  }
  CHECK(staged.Build(pool, {}, 1, 9).has_value() && staged.Generation() == 1 &&
            staged.Tiles().size() == 1 && !staged.SettledWithin(1),
        "contact publishes first while the full ring remains unavailable");
  const auto *contactPoints = staged.Points().data();
  for (int slice = 0; slice < 2; ++slice) {
    CHECK(staged.Build(pool, {}, 1, 9).has_value() && staged.PendingTiles() == 1 &&
              staged.Generation() == 1 && staged.Points().data() == contactPoints &&
              staged.Tiles().size() == 1 && !staged.SettledWithin(1),
          "each partial assembly slice preserves the published contact snapshot");
  }
  CHECK(staged.Build(pool, {}, 1, 9).has_value() && staged.PendingTiles() == 0 &&
            staged.Generation() == 2 && staged.Tiles().size() == 9 && staged.SettledWithin(1),
        "the complete canonical snapshot commits on the final slice");
  CHECK(staged.Features().size() == direct.Features().size() &&
            staged.Rings().size() == direct.Rings().size() &&
            std::ranges::equal(staged.Points(), direct.Points()),
        "staged and one-shot paths retain the same geometry sizes and coordinates");
  bool sameIndices = staged.Tiles().size() == direct.Tiles().size();
  for (size_t at = 0; at < staged.Tiles().size() && sameIndices; ++at) {
    const auto &left = staged.Tiles()[at];
    const auto &right = direct.Tiles()[at];
    sameIndices = left.X == right.X && left.Y == right.Y &&
                  left.FirstFeature == right.FirstFeature &&
                  left.FeatureCount == right.FeatureCount;
  }
  CHECK(sameIndices, "tile keys and feature offsets match the one-shot oracle");
  return Report();
}
