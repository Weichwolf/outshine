#include "OsmField.h"
#include "SourceSet.h"
#include "Check.h"
#include "test/outshine/src/world/data/MvtLayer/WireFixture.h"

#include <array>
#include <memory>
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
  explicit VectorSource(std::string revision, std::vector<uint8_t> bytes)
      : Decl{.Id = "fixture-vector",
             .Revision = std::move(revision),
             .Kind = DataKind::VectorMap,
             .Wire = WireFormat::MapboxVectorTile,
             .Keeps = Cacheability::Never},
        Bytes(std::move(bytes)) {}

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  Ticket Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Delivered(Bytes);
  }

private:
  SourceDecl Decl;
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

TileSourceIdentity Resident(std::string revision, const std::vector<uint8_t> &bytes) {
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  NoTransport transport;
  const auto registration = sources.Add(std::make_unique<VectorSource>(std::move(revision), bytes));
  if (registration != SourceSet::Registration::Accepted) { return {}; }
  Ground::TilePool pool({}, sources, transport);
  const Fetch request(DataKind::VectorMap, Address::At({.Zoom = 0, .X = 0, .Y = 0}));
  Ground::TilePool::Landing cached;
  if (pool.BytesBlocking(request, &cached) != Ground::TilePool::Reply::Ready) { return {}; }
  const std::array<std::string, 1> layers{"x"};
  Ground::OsmField field(0, layers);
  const auto built = field.Build(pool, {.LongitudeDeg = 0, .LatitudeDeg = 0}, 0, 1);
  if (!built || field.Tiles().size() != 1) { return {}; }
  return field.Tiles().front().Source;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  const auto bytes = TileBytes();
  const TileSourceIdentity first = Resident("r1", bytes);
  const TileSourceIdentity second = Resident("r2", bytes);
  const TileId expected{.Zoom = 0, .X = 0, .Y = 0};
  CHECK(first.From == TileSourceIdentity::Origin::Provider,
        "a provider-backed resident tile keeps its origin");
  CHECK(first.SourceId == "fixture-vector", "a resident tile keeps the provider ID");
  CHECK(first.Revision == "r1", "a resident tile keeps the source revision after cache lookup");
  CHECK(first.Kind == DataKind::VectorMap && first.Tile == expected,
        "resident identity names the canonical vector tile");
  CHECK(second.SourceId == first.SourceId && second.Revision == "r2" && !(first == second),
        "equal vector payloads from different declared revisions remain distinct inputs");
  const std::array<std::string, 1> layers{"x"};
  Ground::OsmField direct(0, layers);
  const auto accepted = direct.Accept(0, 0, bytes);
  CHECK(accepted && direct.Tiles().size() == 1, "direct vector input publishes a native tile");
  if (accepted && direct.Tiles().size() == 1) {
    CHECK(direct.Tiles().front().Source.From == TileSourceIdentity::Origin::Direct &&
              !(direct.Tiles().front().Source == first),
          "a direct input cannot collide with a provider identity");
  }
  return Report();
}
