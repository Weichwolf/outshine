#include "TilePool.h"
#include "tiles/TerrainTiles.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include <world/data/Transport.h>
#include "Check.h"
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <memory>
#include <mutex>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Ground;
constexpr uint8_t kPng0[]{
    0x89, 0x50, 0x4e, 0x47, 0xd,  0xa,  0x1a, 0xa,  0x0,  0x0,  0x0,  0xd,  0x49, 0x48, 0x44,
    0x52, 0x0,  0x0,  0x0,  0x8,  0x0,  0x0,  0x0,  0x8,  0x8,  0x2,  0x0,  0x0,  0x0,  0x4b,
    0x6d, 0x29, 0xdc, 0x0,  0x0,  0x0,  0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x68,
    0x60, 0x60, 0xc0, 0x8a, 0xb0, 0x8b, 0xe,  0x5a, 0x9,  0x0,  0xa1, 0x7c, 0x20, 0x1,  0x64,
    0xc6, 0x93, 0x18, 0x0,  0x0,  0x0,  0x0,  0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

enum class Mode { FieldScope, MeshScope, MeshCancelled };

struct Gate {
  std::mutex Mutex;
  std::condition_variable Changed;
  bool Entered = false, Released = false, TimedOut = false;

  bool Wait() {
    std::unique_lock lock(Mutex);
    return Changed.wait_for(lock, std::chrono::seconds(1), [&] { return Entered; });
  }

  void Release() {
    const std::lock_guard lock(Mutex);
    Released = true;
    Changed.notify_all();
  }

  void Pause() {
    std::unique_lock lock(Mutex);
    if (Entered) { return; }
    Entered = true;
    Changed.notify_all();
    TimedOut = !Changed.wait_for(lock, std::chrono::seconds(5), [&] { return Released; });
  }
};

class NoNetwork final : public Transport {
public:
  FetchStart Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};

class Provider final : public Source {
public:
  explicit Provider(std::shared_ptr<Gate> gate) : Gate_(std::move(gate)) {}

  SourceDecl Decl{.Id = "stale-owner-dem", .Keeps = Cacheability::Never};

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    Gate_->Pause();
    return Fetched::Delivered({kPng0, kPng0 + sizeof(kPng0)});
  }

private:
  std::shared_ptr<Gate> Gate_;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  for (const Mode mode : {Mode::FieldScope, Mode::MeshScope, Mode::MeshCancelled}) {
    const bool mesh = mode != Mode::FieldScope;
    const bool cancelled = mode == Mode::MeshCancelled;
    const float expectedHeight = cancelled ? 0.0f : 9.0f;
    auto gate = std::make_shared<Gate>();
    Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
    Data::SourceSet sources(store);
    NoNetwork transport;
    CHECK(sources.Add(std::make_unique<Provider>(gate)) == Data::SourceSet::Registration::Accepted,
          "controlled local provider registers");
    TilePool pool(
        {.Threads = 2, .ByteBudget = 1024u * 1024u, .PollAttempts = 10}, sources, transport);
    const Data::TileId at{.Zoom = 2, .X = 1, .Y = 1};
    std::shared_ptr<const TerrainField> field;
    TileBuild build;
    const auto ask = [&] { return mesh ? pool.Mesh(at, 4, &build) : pool.Field(at, &field); };
    CHECK(ask() == TilePool::Reply::Pending, "old product starts asynchronously");
    CHECK(gate->Wait(), "old computation pauses before delivering source bytes");
    if (cancelled) {
      pool.ForgetMesh(at.Zoom, at.X, at.Y);
    } else {
      pool.Shapes({.Kind = "sineRidge", .AmplitudeM = 9.0, .WavelengthM = 1.0e12});
    }
    const auto before = pool.Counters().Posts;
    CHECK(ask() == TilePool::Reply::Pending, "replacement starts without accepting old geometry");
    CHECK(pool.Counters().Posts == before + 1, "new scope admits a separate reservation");
    if (cancelled) { gate->Release(); }
    bool ready = false;
    const auto readyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    do {
      ready = ask() == TilePool::Reply::Ready;
      if (!ready) { (void)pool.AwaitLanding(0.01); }
    } while (!ready && std::chrono::steady_clock::now() < readyDeadline);
    CHECK(ready, "replacement publishes from its own live reservation");
    if (ready && mesh) {
      const auto side = static_cast<size_t>(build.Side);
      CHECK(side >= 2 && build.Nodes.size() == side * side &&
                std::abs(build.Nodes[(side / 2) * side + side / 2] - expectedHeight) < 1.0e-4f,
            "mesh contains new source heights, never stale heights");
    } else if (ready) {
      CHECK(field && std::abs(field->AtM(3, 3) - expectedHeight) < 1.0e-4f,
            "height field contains new source heights, never stale heights");
    }
    gate->Release();
    const auto dropped = [&] {
      const auto counters = pool.Counters();
      return mesh ? counters.MeshDropped : counters.FieldDropped;
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (dropped() == 0 && std::chrono::steady_clock::now() < deadline) {
      (void)pool.AwaitLanding(0.01);
    }
    CHECK(dropped() == 1, "obsolete work releases only its own reservation");
    if (mesh) {
      CHECK(ask() == TilePool::Reply::Ready,
            "obsolete completion cannot poison the retained current mesh");
    }
    {
      const std::lock_guard lock(gate->Mutex);
      CHECK(!gate->TimedOut, "ordering comes from barriers rather than source timeouts");
    }
  }
  return Report();
}
