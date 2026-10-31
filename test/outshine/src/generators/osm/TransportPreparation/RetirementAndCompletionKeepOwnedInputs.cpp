#include "OsmTransportPreparation.h"
#include "OsmXmlReader.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <semaphore>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr auto kDeadline = std::chrono::seconds(3);

struct Probe {
  std::binary_semaphore Gone{0};
  std::atomic_bool ReleasedOffCaller{false};
  const std::thread::id Caller = std::this_thread::get_id();
};

std::shared_ptr<const SourceSnapshot> Source(Probe &probe) {
  auto parsed =
      XmlReader::Read("<osm version='0.6'><node id='1' lat='0' lon='0'/>"
                      "<node id='2' lat='0' lon='0.001'/><way id='10'><nd ref='1'/><nd ref='2'/>"
                      "<tag k='highway' v='residential'/></way></osm>",
                      {.DatasetId = "transport-retirement", .Revision = "r1"});
  CHECK(parsed, "the original transport input parses");
  if (!parsed) { return {}; }
  return {new SourceSnapshot{.Elements = std::move(*parsed), .Coverage = {}, .Chunks = {}},
          [&probe](const SourceSnapshot *source) {
            delete source;
            probe.ReleasedOffCaller = std::this_thread::get_id() != probe.Caller;
            probe.Gone.release();
          }};
}

class WorkerBlock {
public:
  WorkerBlock() = default;

  ~WorkerBlock() { Release(); }

  WorkerBlock(const WorkerBlock &) = delete;
  WorkerBlock &operator=(const WorkerBlock &) = delete;

  [[nodiscard]] bool Start(Tasks &pool) {
    Signal_ = std::make_shared<Signal>();
    if (!pool.PostDetached([signal = Signal_] {
          signal->Entered.release();
          signal->Release.acquire();
        })) {
      Signal_.reset();
      return false;
    }
    return Signal_->Entered.try_acquire_for(kDeadline);
  }

  void Release() {
    if (const auto signal = std::exchange(Signal_, {})) { signal->Release.release(); }
  }

private:
  struct Signal {
    std::binary_semaphore Entered{0}, Release{0};
  };

  std::shared_ptr<Signal> Signal_;
};

void Barrier(Tasks &pool) {
  const auto handle = pool.Post([] {});
  CHECK(handle != Tasks::kNoTask, "the worker accepts a terminal barrier");
  if (handle != Tasks::kNoTask) { pool.Wait(handle); }
}

void Retire(bool ready) {
  Probe probe;
  Tasks pool(1);
  auto input = Source(probe);
  const std::weak_ptr<const SourceSnapshot> lifetime = input;
  auto loader = std::make_unique<TransportPreparation>(pool);
  WorkerBlock held;
  if (!ready) { CHECK(held.Start(pool), "the worker is held before lookup/generation"); }
  CHECK(loader->RequestSource(input), "the shared source is admitted");
  input.reset();
  if (ready) {
    Barrier(pool);
    loader->Poll();
    CHECK(loader->CurrentPhase() == TransportPreparation::Phase::Ready && loader->Current(),
          "a real network is published before idle retirement");
    CHECK(held.Start(pool), "the worker is held before idle cleanup");
  }
  std::binary_semaphore retired(0);
  std::thread release([&] {
    loader.reset();
    retired.release();
  });
  CHECK(retired.try_acquire_for(std::chrono::milliseconds(250)),
        "queued and idle transport retirement return before the worker is released");
  CHECK(!lifetime.expired(), "the source stays owned by queued work or worker cleanup");
  held.Release();
  release.join();
  Barrier(pool);
  CHECK(probe.Gone.try_acquire_for(kDeadline) && lifetime.expired() && probe.ReleasedOffCaller,
        "the final source owner is released by a worker");
}

void Discard() {
  Probe probe;
  auto pool = std::make_unique<Tasks>(1);
  WorkerBlock held;
  CHECK(held.Start(*pool), "the worker is held before queue discard");
  auto input = Source(probe);
  const std::weak_ptr<const SourceSnapshot> lifetime = input;
  auto loader = std::make_unique<TransportPreparation>(*pool);
  CHECK(loader->RequestSource(input), "the source is queued before pool shutdown");
  input.reset();
  std::binary_semaphore retired(0);
  std::thread release([&] {
    loader.reset();
    retired.release();
  });
  const bool returned = retired.try_acquire_for(std::chrono::milliseconds(250));
  CHECK(returned, "retirement returns without waiting for a queued job");
  if (!returned) { held.Release(); }
  release.join();
  if (!returned) { return; }
  CHECK(!lifetime.expired(), "queued work retains its input after the frontend is gone");
  std::atomic_bool stopped{false};
  std::thread stop([&] {
    pool.reset();
    stopped = true;
  });
  CHECK(probe.Gone.try_acquire_for(kDeadline) && lifetime.expired() && !stopped,
        "shutdown discards queued input before joining the held worker");
  held.Release();
  stop.join();
  CHECK(stopped, "shutdown completes after its running blocker is released");
}

void Completion() {
  Probe probe;
  Tasks pool(1);
  const auto input = Source(probe);
  TransportPreparation loader(pool);
  const auto unrelated = pool.Post([] {});
  CHECK(pool.AwaitCompletion(unrelated, 1), "an unrelated tracked completion is retained");
  WorkerBlock held;
  CHECK(held.Start(pool), "the worker is held before its own transport job");
  CHECK(loader.RequestSource(input), "the actual transport job is queued");
  CHECK(!loader.AwaitProgress(0) && !loader.AwaitProgress(0.002),
        "unrelated completion cannot satisfy the transport wait");
  std::binary_semaphore entered(0);
  std::binary_semaphore finished(0);
  std::atomic_bool progress{false};
  std::thread waiter([&] {
    entered.release();
    progress = loader.AwaitProgress(1);
    finished.release();
  });
  CHECK(entered.try_acquire_for(kDeadline), "the waiter starts before its job can run");
  CHECK(!finished.try_acquire_for(std::chrono::milliseconds(1)),
        "transport waiting cannot finish from an unrelated handle");
  held.Release();
  waiter.join();
  CHECK(progress, "the actual detached transport job wakes its waiter");
  Barrier(pool);
  CHECK(loader.AwaitProgress(1), "completion stays observable before Poll consumes it");
  loader.Poll();
  CHECK(loader.CurrentPhase() == TransportPreparation::Phase::Ready && loader.Current() &&
            loader.Current()->Topology().FindNode(1) != nullptr,
        "real completed network data is published after worker completion");
  CHECK(!loader.AwaitProgress(0.002), "a consumed request does not leave a completion event");
  CHECK(pool.TakeCompletion(unrelated), "unrelated tracked completion keeps its original owner");
}
}

int main() {
  Retire(false);
  Retire(true);
  Discard();
  Completion();
  return Report();
}
