#include "Check.h"
#include "OsmCacheFixture.h"
#include "PreparedOsmTiles.h"
#include "Source.h"
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr auto kDeadline = std::chrono::seconds(3);

struct Gate {
  std::binary_semaphore Entered{0}, Release{0}, Gone{0};
  std::atomic_uint Begins{0}, Polls{0}, Cancelled{0}, Destroyed{0};
  std::thread::id Owner = std::this_thread::get_id();
  std::atomic_bool ReleasedOffOwner{false};
};

class HeldProvider final : public Source {
public:
  explicit HeldProvider(Gate &gate) : Gate_(gate) {
    Decl_.Id = "blocked-osm-retirement";
    Decl_.Revision = "1";
    Decl_.Kind = DataKind::VectorMap;
    Decl_.Wire = WireFormat::MapboxVectorTile;
    Decl_.Keeps = Cacheability::Never;
    Decl_.MaximumPayloadBytes = 1024;
  }

  ~HeldProvider() override {
    Gate_.ReleasedOffOwner = std::this_thread::get_id() != Gate_.Owner;
    ++Gate_.Destroyed;
    Gate_.Gone.release();
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const SourceRequest & /*request*/) const noexcept override {
    return Coverage::Inside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address & /*at*/, SourceTransport & /*transport*/) const override {
    ++Gate_.Begins;
    return Ticket{19};
  }

  SourceResponse Collect(const Address & /*at*/,
                         Ticket /*ticket*/,
                         SourceTransport & /*transport*/) const override {
    ++Gate_.Polls;
    Gate_.Entered.release();
    Gate_.Release.acquire();
    return SourceResponse::Working();
  }

  void Cancel(Ticket ticket, SourceTransport & /*transport*/) const override {
    CHECK(ticket == Ticket{19}, "retirement cancels the held provider ticket");
    ++Gate_.Cancelled;
  }

private:
  Gate &Gate_;
  SourceDecl Decl_;
};

void Exercise(const std::filesystem::path &directory) {
  Gate gate;
  OsmCache::NoTransport transport;
  Tasks compute(1);
  auto session = OsmCache::OpenSession(std::make_unique<HeldProvider>(gate));
  CHECK(session, "the held provider belongs to one owned source session");
  if (!session) { return; }
  const std::weak_ptr<SourceSession> lifetime = *session;
  auto cache = PreparedOsmTiles::Open(directory.string(), *session, transport, compute);
  CHECK(cache, "the real native OSM service opens");
  if (!cache) { return; }
  session->reset();
  const std::array<std::string, 1> layers{"buildings"};
  const OsmField layout(2, layers, MvtSchema::OpenMapTiles);
  bool entered = false;
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (!entered && std::chrono::steady_clock::now() < until) {
    const auto result = (*cache)->Acquire({.X = 1, .Y = 1}, layout);
    CHECK(result, "the real producer advances toward its held provider");
    entered = gate.Entered.try_acquire_for(std::chrono::milliseconds(1));
  }
  CHECK(entered, "the worker is held inside a real provider call");
  std::binary_semaphore retired(0);
  std::thread release([&] {
    cache->reset();
    retired.release();
  });
  const bool returned = retired.try_acquire_for(std::chrono::milliseconds(250));
  CHECK(returned, "frontend retirement returns while the provider is still held");
  CHECK(!lifetime.expired() && gate.Destroyed == 0,
        "unfinished work retains its source after the frontend has gone");
  gate.Release.release();
  release.join();
  const auto barrier = compute.Post([] {});
  CHECK(barrier != Tasks::kNoTask, "the worker drain has an explicit terminal barrier");
  if (barrier != Tasks::kNoTask) { compute.Wait(barrier); }
  CHECK(gate.Begins == 1 && gate.Polls == 1 && gate.Cancelled == 1 && gate.Destroyed == 1 &&
            lifetime.expired(),
        "worker completion cancels exactly once and releases the last source owner");
}

struct HeldPool {
  std::binary_semaphore Entered{0};
  std::counting_semaphore<2> Release{0};
  std::unique_ptr<Tasks> Pool = std::make_unique<Tasks>(1);

  [[nodiscard]] bool Hold() {
    return Pool->PostDetached([this] {
      Entered.release();
      Release.acquire();
    }) && Entered.try_acquire_for(kDeadline);
  }

  ~HeldPool() {
    Release.release();
    Pool.reset();
  }
};

void DiscardQueuedWork(const std::filesystem::path &directory) {
  Gate gate;
  OsmCache::NoTransport transport;
  HeldPool worker;
  const bool held = worker.Hold();
  CHECK(held, "an unrelated job holds the only worker");
  if (!held) { return; }
  auto session = OsmCache::OpenSession(std::make_unique<HeldProvider>(gate));
  CHECK(session, "the queued provider owns a valid source session");
  if (!session) { return; }
  const std::weak_ptr<SourceSession> lifetime = *session;
  auto cache = PreparedOsmTiles::Open(directory.string(), *session, transport, *worker.Pool);
  CHECK(cache, "the native producer shares the blocked pool");
  if (!cache) { return; }
  session->reset();
  const std::array<std::string, 1> layers{"buildings"};
  const OsmField layout(2, layers, MvtSchema::OpenMapTiles);
  CHECK((*cache)->Acquire({.X = 1, .Y = 1}, layout), "logical work queues behind the held worker");
  std::binary_semaphore retired(0);
  std::thread release([&] {
    cache->reset();
    retired.release();
  });
  const bool returned = retired.try_acquire_for(std::chrono::milliseconds(250));
  CHECK(returned, "frontend retirement does not drain queued producer work");
  if (!returned) {
    worker.Release.release();
    release.join();
    return;
  }
  release.join();
  CHECK(!lifetime.expired() && gate.Destroyed == 0,
        "queued work retains its source after frontend retirement");
  std::thread shutdown([&] { worker.Pool.reset(); });
  const bool discarded = gate.Gone.try_acquire_for(kDeadline);
  CHECK(discarded && lifetime.expired() && gate.Destroyed == 1 && gate.Cancelled == 0 &&
            gate.Begins == 0 && gate.Polls == 0,
        "pool discard releases an unstarted producer without polling or cancelling its provider");
  worker.Release.release();
  shutdown.join();
}

void RetireIdle(const std::filesystem::path &directory) {
  Gate gate;
  OsmCache::NoTransport transport;
  HeldPool worker;
  const bool held = worker.Hold();
  CHECK(held, "idle retirement shares a deliberately blocked pool");
  if (!held) { return; }
  auto session = OsmCache::OpenSession(std::make_unique<HeldProvider>(gate));
  CHECK(session, "idle preparation owns a valid provider session");
  if (!session) { return; }
  const std::weak_ptr<SourceSession> lifetime = *session;
  auto cache = PreparedOsmTiles::Open(directory.string(), *session, transport, *worker.Pool);
  CHECK(cache, "the real idle native service opens");
  if (!cache) { return; }
  session->reset();
  cache->reset();
  CHECK(!lifetime.expired() && gate.Destroyed == 0,
        "idle frontend release does not close its provider or SQLite backend on the caller");
  worker.Release.release();
  const auto barrier = worker.Pool->Post([] {});
  CHECK(barrier != Tasks::kNoTask, "idle backend cleanup has a terminal worker barrier");
  if (barrier != Tasks::kNoTask) { worker.Pool->Wait(barrier); }
  CHECK(lifetime.expired() && gate.Destroyed == 1 && gate.Cancelled == 0 && gate.Begins == 0 &&
            gate.Polls == 0 && gate.ReleasedOffOwner,
        "the existing pool releases unused backend resources without starting provider work");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-osm-facade-retirement-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  Exercise(directory / "held");
  DiscardQueuedWork(directory / "queued");
  RetireIdle(directory / "idle");
  std::filesystem::remove_all(directory);
  return Report();
}
