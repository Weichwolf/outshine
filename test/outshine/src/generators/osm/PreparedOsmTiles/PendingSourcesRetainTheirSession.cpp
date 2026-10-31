#include "Check.h"
#include "OsmCacheFixture.h"
#include "PreparedOsmTiles.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <thread>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Generators::Osm;

struct Calls {
  size_t PayloadMost = 1024;
  std::thread::id Owner = std::this_thread::get_id();
  std::atomic_uint Begins{0}, Polls{0}, Cancellations{0}, Destructions{0};
  std::atomic_bool WorkerOnly{true};

  void Observe() {
    if (std::this_thread::get_id() == Owner) { WorkerOnly = false; }
  }
};

class PendingSource final : public Source {
public:
  explicit PendingSource(Calls &calls) : Calls_(calls) {
    Decl_.Id = "pending-osm-input";
    Decl_.Revision = "r1";
    Decl_.Kind = DataKind::VectorMap;
    Decl_.Wire = WireFormat::MapboxVectorTile;
    Decl_.Keeps = Cacheability::Never;
    Decl_.MaximumPayloadBytes = Calls_.PayloadMost;
  }

  ~PendingSource() override { ++Calls_.Destructions; }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const SourceRequest & /*request*/) const noexcept override {
    return Coverage::Inside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address & /*at*/, SourceTransport & /*transport*/) const override {
    Calls_.Observe();
    ++Calls_.Begins;
    return Ticket{17};
  }

  SourceResponse Collect(const Address & /*at*/,
                         Ticket /*ticket*/,
                         SourceTransport & /*transport*/) const override {
    Calls_.Observe();
    ++Calls_.Polls;
    return SourceResponse::Working();
  }

  void Cancel(Ticket ticket, SourceTransport & /*transport*/) const override {
    CHECK(ticket == Ticket{17}, "retirement cancels the original provider ticket");
    ++Calls_.Cancellations;
  }

private:
  Calls &Calls_;
  SourceDecl Decl_;
};

void Retire(const std::filesystem::path &directory) {
  Calls calls;
  Test::OsmCache::NoTransport transport;
  Tasks compute(1);
  auto session = Test::OsmCache::OpenSession(std::make_unique<PendingSource>(calls));
  CHECK(session, "the pending provider belongs to an owned source session");
  if (!session) { return; }
  const std::weak_ptr<SourceSession> retained = *session;
  auto cache = PreparedOsmTiles::Open(directory.string(), *session, transport, compute);
  CHECK(cache, "native tile preparation binds its source scope before demand");
  if (!cache) { return; }
  session->reset();
  CHECK(!retained.expired() && calls.Destructions == 0 && calls.Begins == 0,
        "the service pins its configured provider without starting source IO");
  {
    const std::array<std::string, 1> layers{"buildings"};
    const OsmField layout(2, layers, MvtSchema::OpenMapTiles);
    const auto first = (*cache)->Acquire({.X = 1, .Y = 1}, layout);
    CHECK(first && first->Status == PreparedOsmTiles::State::Pending && calls.Begins == 0,
          "initial demand performs no provider IO on the caller");
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (calls.Polls < 3 && std::chrono::steady_clock::now() < until) {
      const auto next = (*cache)->Acquire({.X = 1, .Y = 1}, layout);
      CHECK(next && next->Status == PreparedOsmTiles::State::Pending,
            "an unresolved source leaves the native request pending");
      (void)compute.AwaitCompletion(0.001);
    }
    CHECK(calls.Begins == 1 && calls.Polls >= 3 && calls.WorkerOnly && calls.Cancellations == 0 &&
              !retained.expired(),
          "worker polls resume one live provider read while its session stays pinned");
  }
  cache->reset();
  CHECK(calls.Cancellations == 1 && calls.Destructions == 1 && retained.expired(),
        "service retirement drains its worker then cancels before releasing the last session");
}

void RefuseBeforeIO(const std::filesystem::path &directory, size_t payloadMost) {
  Calls calls;
  calls.PayloadMost = payloadMost;
  Test::OsmCache::NoTransport transport;
  Tasks compute(1);
  auto session = Test::OsmCache::OpenSession(std::make_unique<PendingSource>(calls));
  CHECK(session, "the pre-IO source fixture opens");
  if (!session) { return; }
  auto cache = PreparedOsmTiles::Open(directory.string(),
                                      *session,
                                      transport,
                                      compute,
                                      {.InputBytesMost = Data::MaximumResponseHeaderBytes});
  CHECK(cache, "the native service opens with a deliberately insufficient input budget");
  if (!cache) { return; }
  const std::array<std::string, 1> layers{"buildings"};
  const OsmField layout(2, layers, MvtSchema::OpenMapTiles);
  bool refused = false;
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!refused && std::chrono::steady_clock::now() < until) {
    refused = !(*cache)->Acquire({.X = 1, .Y = 1}, layout);
    (void)compute.AwaitCompletion(0.001);
  }
  CHECK(refused && calls.Begins == 0 && calls.Polls == 0 && calls.Cancellations == 0,
        "unknown or unadmittable source bytes refuse before any provider IO starts");
}

}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-owned-osm-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Retire(directory);
  RefuseBeforeIO(directory / "unknown", 0);
  RefuseBeforeIO(directory / "insufficient", 1024);
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private native cache is removed after every source owner retires");
  return outshine::Test::Report();
}
