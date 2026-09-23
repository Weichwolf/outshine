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
  Ground::OsmField bounded(4, layers);
  const auto settledCount = [&bounded] {
    size_t count = 0;
    for (int y = 7; y <= 9; ++y) {
      for (int x = 7; x <= 9; ++x) { count += bounded.Settled(x, y) ? 1u : 0u; }
    }
    return count;
  };
  bool sawContact = false;
  for (int slice = 0; slice < 12 && !bounded.SettledWithin(1); ++slice) {
    const size_t before = settledCount();
    CHECK(bounded.Build(pool, {}, 1, 9, {.TilesMost = 2}).has_value(),
          "bounded parsing accepts the resident source window");
    const size_t after = settledCount();
    CHECK(after >= before && after - before <= 2,
          "each interrupted update parses at most two vector tiles");
    if (bounded.Generation() == 1) {
      sawContact = true;
      CHECK(bounded.Tiles().size() == 1 && !bounded.SettledWithin(1),
            "partial parsing retains only the contact publication");
    }
    CHECK(bounded.Generation() != 2 || after == 9,
          "full geometry cannot publish before the last source tile is parsed");
  }
  CHECK(sawContact && bounded.SettledWithin(1) && bounded.Generation() == 2 &&
            bounded.Tiles().size() == staged.Tiles().size() &&
            bounded.Features().size() == staged.Features().size() &&
            std::ranges::equal(bounded.Points(), staged.Points()),
        "bounded and unbounded parsing publish the same canonical world");
  CHECK(!bounded.Build(pool, {}, 1, 9, {.TilesMost = 0}).has_value() && bounded.Generation() == 2,
        "an empty parse budget cannot mutate the published world");
  const std::vector<double> originalPoints(staged.Points().begin(), staged.Points().end());
  const size_t originalBytes = staged.HeapBytes();
  const auto *originalPublication = staged.Points().data();
  CHECK(staged.Build(pool, {.LongitudeDeg = 90.0}, 1, 9).has_value() &&
            staged.Points().data() == originalPublication,
        "a pending distant contact preserves the previously published snapshot");
  for (uint32_t y = 7; y <= 9; ++y) {
    for (uint32_t x = 11; x <= 13; ++x) {
      Ground::TilePool::Landing landed;
      const Fetch request(DataKind::VectorMap, Address::At({.Zoom = 4, .X = x, .Y = y}));
      CHECK(pool.BytesBlocking(request, &landed) == Ground::TilePool::Reply::Ready,
            "the distant source window becomes resident");
    }
  }
  for (int slice = 0; slice < 4; ++slice) {
    CHECK(staged.Build(pool, {.LongitudeDeg = 90.0}, 1, 9).has_value(),
          "distant window assembles without source errors");
  }
  CHECK(staged.SettledWithin(1) && staged.Tiles().size() == 9 &&
            std::ranges::all_of(staged.Tiles(),
                                [](const auto &tile) { return tile.X >= 11 && tile.X <= 13; }),
        "moving removes the old window from the next canonical publication");
  for (int slice = 0; slice < 4; ++slice) {
    CHECK(staged.Build(pool, {}, 1, 9).has_value(),
          "return window assembles without source errors");
  }
  CHECK(staged.SettledWithin(1) && staged.Tiles().size() == 9 &&
            std::ranges::equal(staged.Points(), originalPoints) &&
            staged.HeapBytes() < originalBytes * 2,
        "return travel reproduces the original geometry without accumulating windows");
  return Report();
}
