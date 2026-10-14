#include "TerrainLoader.h"
#include "SourceSet.h"
#include "Check.h"
#include <chrono>
#include <memory>

namespace {
class SingleTileSource final : public outshine::Data::Source {
public:
  outshine::Data::SourceDecl Decl{
      .Id = "single-tile", .Revision = "r1", .Keeps = outshine::Data::Cacheability::Never};

  const outshine::Data::SourceDecl &Declaration() const noexcept override { return Decl; }

  outshine::Data::Coverage Covers(const outshine::Data::Fetch &request) const noexcept override {
    const auto tile = request.Where().Tile();
    return tile && tile->Zoom == 6 && tile->X == 32 && tile->Y == 32
               ? outshine::Data::Coverage::Inside
               : outshine::Data::Coverage::Outside;
  }

  outshine::Data::Address Serves(const outshine::Data::Fetch &request) const noexcept override {
    return request.Where();
  }

  outshine::Data::FetchStart Begin(const outshine::Data::Address &,
                                   outshine::Data::Transport &) const override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Fetched Collect(const outshine::Data::Address &,
                                  outshine::Data::Ticket,
                                  outshine::Data::Transport &) const override {
    return outshine::Data::Fetched::Delivered(
        {137, 80,  78,  71,  13,  10, 26,  10,  0,  0,   0,  13,  73,  72, 68, 82, 0,   0,  0,
         4,   0,   0,   0,   4,   8,  2,   0,   0,  0,   38, 147, 9,   41, 0,  0,  0,   16, 73,
         68,  65,  84,  120, 156, 99, 104, 208, 98, 128, 35, 6,   226, 56, 0,  28, 131, 10, 161,
         92,  158, 195, 228, 0,   0,  0,   0,   73, 69,  78, 68,  174, 66, 96, 130});
  }
};

class NoNetwork final : public outshine::Data::Transport {
public:
  outshine::Data::FetchStart Begin(const std::string &) override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Never();
  }

  void Cancel(outshine::Data::Ticket) override {}
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
  Data::SourceSet sources(store);
  NoNetwork transport;
  CHECK(sources.Add(std::make_unique<SingleTileSource>()) ==
            Data::SourceSet::Registration::Accepted,
        "single terrain tile registered");
  Ground::TilePool pool({.PollAttempts = 100}, sources, transport);
  Ground::GroundStream ground(pool, {.Z = 4, .Grid = 4});
  const LongitudeLatitude point{.LongitudeDeg = 2.8, .LatitudeDeg = -2.8};
  auto sample = GroundSample::Waiting();
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    sample = ground.At(point, 6);
    if (sample.AslM()) { break; }
    static_cast<void>(pool.AwaitLanding(0.002));
  }
  CHECK(sample.AslM().has_value(),
        "interior point resolves although neighboring sources are absent");
  const auto fields = pool.Counters().FieldTiles;
  for (int query = 0; query < 4; ++query) {
    const auto again = ground.At(point, 6);
    CHECK(again.AslM().has_value(), "a resolved interior point does not return to pending");
    if (again.AslM()) {
      CHECK_NEAR(*again.AslM(), 42.0, 0.001, "m", "source height remains available");
    }
  }
  CHECK(pool.Counters().FieldTiles == fields, "repeat point queries start no extra field work");
  pool.Shapes({.Kind = "plane", .AmplitudeM = 80.0});
  CHECK(ground.At(point, 6).Where() == GroundSample::State::Pending,
        "a real terrain scope change invalidates the retained point field");
  return Report();
}
