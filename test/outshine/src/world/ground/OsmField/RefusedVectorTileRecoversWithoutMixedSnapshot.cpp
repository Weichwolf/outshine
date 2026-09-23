#include "OsmField.h"
#include "SourceSet.h"
#include "Check.h"
#include "test/outshine/src/world/data/MvtLayer/WireFixture.h"

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;

class ClockTransport final : public Transport {
public:
  std::atomic<double> Now{0.0};

  Ticket Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}

  double NowMs() override { return Now.load(); }
};

class RecoveringSource final : public Source {
public:
  explicit RecoveringSource(std::vector<uint8_t> bytes) : Bytes(std::move(bytes)) {}

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  Ticket Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Calls.fetch_add(1) == 0 ? Fetched::Meant(Meaning::Refused) : Fetched::Delivered(Bytes);
  }

  mutable std::atomic<int> Calls{0};

private:
  SourceDecl Decl{.Id = "recovering-vector",
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

template <class Ready>
bool BuildUntil(Ground::OsmField &field, Ground::TilePool &pool, Ready ready) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  do {
    if (!field.Build(pool, {.LongitudeDeg = 90.0}, 0, 1)) { return false; }
    if (ready()) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  ClockTransport transport;
  const auto bytes = TileBytes();
  auto source = std::make_unique<RecoveringSource>(bytes);
  const auto *probe = source.get();
  CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted,
        "recovering source registers");
  Ground::TilePool pool({}, sources, transport);
  const std::array<std::string, 1> layers{"x"};
  Ground::OsmField field(1, layers);
  CHECK(field.Accept(0, 1, bytes).has_value(), "old snapshot is published");
  const auto prior = field.Generation();
  const auto *priorPoints = field.Points().data();
  CHECK(BuildUntil(field, pool, [&] { return field.RefusedTiles() == 1; }),
        "the new centre is refused first");
  CHECK(field.Generation() == prior && field.Points().data() == priorPoints &&
            field.Tiles().size() == 1 && field.Tiles()[0].X == 0 && field.OfTile(0).size() == 1,
        "refusal retains the previous complete snapshot");
  transport.Now.store(5000.0);
  CHECK(BuildUntil(field, pool, [&] { return field.Settled(1, 1); }),
        "the new centre recovers after backoff");
  CHECK(probe->Calls.load() == 2 && field.Generation() > prior && field.Tiles().size() == 1 &&
            field.Tiles()[0].X == 1 && field.Tiles()[0].Y == 1 && field.OfTile(0).size() == 1 &&
            field.Tiles()[0].Source.From == Data::TileSourceIdentity::Origin::Provider &&
            field.Tiles()[0].Source.Revision == "r1" && field.PendingTiles() == 0 &&
            field.RefusedTiles() == 0 && field.SettledWithin(0),
        "recovery publishes only the new centre with provider-backed geometry");
  return Report();
}
