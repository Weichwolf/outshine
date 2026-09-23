#include "TerrainLoader.h"
#include "HeightField.h"
#include "SourceSet.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace {
class DelayedSource final : public outshine::Data::Source {
public:
  outshine::Data::SourceDecl Decl{
      .Id = "delayed", .Revision = "r1", .Keeps = outshine::Data::Cacheability::Never};
  std::thread::id Caller = std::this_thread::get_id();
  mutable std::atomic<bool> CalledOnCaller{false};
  std::atomic<bool> Released{false};

  const outshine::Data::SourceDecl &Declaration() const noexcept override { return Decl; }

  outshine::Data::Coverage Covers(const outshine::Data::Fetch &) const noexcept override {
    return outshine::Data::Coverage::Inside;
  }

  outshine::Data::Address Serves(const outshine::Data::Fetch &request) const noexcept override {
    return request.Where();
  }

  outshine::Data::Ticket Begin(const outshine::Data::Address &,
                               outshine::Data::Transport &) const override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Fetched Collect(const outshine::Data::Address &,
                                  outshine::Data::Ticket,
                                  outshine::Data::Transport &) const override {
    if (std::this_thread::get_id() == Caller) { CalledOnCaller = true; }
    if (!Released) { return outshine::Data::Fetched::Working(); }
    return outshine::Data::Fetched::Delivered(
        {137, 80,  78,  71,  13,  10, 26,  10,  0,  0,   0,  13,  73,  72, 68, 82, 0,   0,  0,
         4,   0,   0,   0,   4,   8,  2,   0,   0,  0,   38, 147, 9,   41, 0,  0,  0,   16, 73,
         68,  65,  84,  120, 156, 99, 104, 208, 98, 128, 35, 6,   226, 56, 0,  28, 131, 10, 161,
         92,  158, 195, 228, 0,   0,  0,   0,   73, 69,  78, 68,  174, 66, 96, 130});
  }
};

class NoNetwork final : public outshine::Data::Transport {
public:
  outshine::Data::Ticket Begin(const std::string &) override {
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
  auto source = std::make_unique<DelayedSource>();
  auto *probe = source.get();
  CHECK(sources.Add(std::move(source)) == Data::SourceSet::Registration::Accepted,
        "source registered");
  Ground::TilePool pool({.PollAttempts = 100}, sources, transport);
  Ground::GroundStream ground(pool, {.Z = 4, .Grid = 4});
  CHECK(ground.At({}).Where() == GroundSample::State::Pending, "unavailable tile remains pending");
  CHECK(!probe->CalledOnCaller, "ground query never collects sources on its caller");
  probe->Released = true;
  auto sample = GroundSample::Waiting();
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    sample = ground.At({});
    if (sample.Where() == GroundSample::State::Resolved) { break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(sample.AslM().has_value(), "pending tile resolves after source release");
  if (sample.AslM()) {
    CHECK_NEAR(*sample.AslM(), 42.0, 0.001, "m", "decoded terrain height preserved");
  }
  CHECK(!probe->CalledOnCaller, "retry also leaves source work on carriers");
  CHECK(pool.Counters().FieldTiles >= 1,
        "stitched height field is built by tile workers before caller samples it");
  const Ground::GroundBlock block = ground.BlockAt(Ground::HeightField::SpotOf({}, 4));
  CHECK(block.Where() == Ground::GroundBlock::State::Resolved && !block.Sources().empty(),
        "resident ground block carries its source set");
  Ground::HeightField::Block copied;
  CHECK(Ground::HeightField::Copies(block, copied) &&
            copied.Sources.size() == block.Sources().size(),
        "height snapshot owns the resident source set");
  CHECK(std::ranges::all_of(copied.Sources,
                            [](const Data::TileSourceIdentity &identity) {
                              return identity.SourceId == "delayed" && identity.Revision == "r1";
                            }),
        "height snapshot retains provider revision");
  auto terrain = std::make_shared<Ground::TerrainField>(2, 2);
  for (uint32_t row = 0; row < 2; ++row) {
    for (uint32_t column = 0; column < 2; ++column) { terrain->SetM(row, column, 42.0f); }
  }
  Ground::HeightField::Block shared;
  CHECK(Ground::HeightField::SharesField(terrain, {.Zoom = 0}, shared) && shared.Nodes.empty() &&
            shared.Terrain == terrain,
        "height snapshot shares an immutable terrain raster");
  const auto field = Ground::HeightField::Of(0, {std::move(shared)});
  CHECK_NEAR(*field->At({}).AslM(), 42.0, 0.001, "m", "shared height snapshot remains sampleable");
  return Report();
}
