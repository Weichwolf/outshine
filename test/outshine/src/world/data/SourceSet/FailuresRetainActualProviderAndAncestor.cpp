#include "Check.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "TerrariumDem.h"
#include "TilePool.h"
#include "Transport.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;

class MissingTransport final : public Transport {
public:
  FetchFailureReason Reason = FetchFailureReason::OfflineMiss;
  double Now = 0;
  bool Authorized = false;

  Ticket Begin(const std::string &url) override {
    return url.starts_with("https://primary.example/") ? Ticket{1} : Ticket{2};
  }

  Wire Collect(Ticket ticket) override {
    if (Authorized) { return Wire::Answered(200, {11, 22}); }
    return ticket == Ticket{1} ? Wire::Answered(404, {}) : Wire::Never(Reason);
  }

  void Cancel(Ticket) override {}

  double NowMs() override { return Now; }
};

void AddSources(SourceSet &sources) {
  using namespace outshine::Test;
  CHECK(sources.Add(std::make_unique<TerrariumDem>("first-pin",
                                                   Rank{0},
                                                   AbsencePolicy::Continue,
                                                   "primary",
                                                   "https://primary.example/{z}/{x}/{y}.png")) ==
            SourceSet::Registration::Accepted,
        "primary source registered");
  CHECK(sources.Add(std::make_unique<TerrariumDem>("second-pin",
                                                   Rank{1},
                                                   AbsencePolicy::Continue,
                                                   "fallback",
                                                   "https://fallback.example/{z}/{x}/{y}.png")) ==
            SourceSet::Registration::Accepted,
        "fallback source registered");
}

void CheckFailure(const std::optional<FetchFailure> &failure, const std::string &key) {
  using namespace outshine::Test;
  CHECK(failure.has_value(), "refused request owns its failure");
  if (!failure) { return; }
  CHECK(failure->Kind == DataKind::Elevation &&
            failure->Requested == Address::At({.Zoom = 17, .X = 69, .Y = 45}) &&
            failure->Served == Address::At({.Zoom = 15, .X = 17, .Y = 11}),
        "fine request and actually served ancestor remain distinct");
  CHECK(failure->SourceId == "fallback" && failure->SourceRevision == "second-pin" &&
            failure->SourceKey == key && failure->Reason == FetchFailureReason::OfflineMiss,
        "failure belongs to the actual fallback provider and retains its cause");
}
}

int main() {
  using namespace outshine::Test;
  const Fetch request(DataKind::Elevation, Address::At({.Zoom = 17, .X = 69, .Y = 45}));
  const auto owned = [&] {
    ContentStore store({.Using = ContentStore::Use::Off});
    SourceSet sources(store);
    AddSources(sources);
    MissingTransport transport;
    auto query = sources.Ask(request);
    auto reply = sources.Collect(query, transport);
    CHECK(reply.Where() == Delivery::State::Refused, "offline fallback refuses");
    return std::pair(std::move(reply), SourceKey(sources.At(1).Declaration()));
  }();
  CheckFailure(owned.first.Failure(), owned.second);
  CHECK(!Delivery::Waiting().Failure() &&
            !Delivery::From("ok", "pin", request.Where(), {1}).Failure(),
        "success and pending do not carry diagnostic payloads");
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  AddSources(sources);
  MissingTransport transport;
  outshine::Ground::TilePool pool({.Threads = 1}, sources, transport);
  for (int attempt = 0; attempt < 2; ++attempt) {
    outshine::Ground::TilePool::Landing landing;
    CHECK(pool.BytesBlocking(request, &landing) == outshine::Ground::TilePool::Reply::Refused,
          "refusal is observable through the pool and its retry cache");
    CheckFailure(landing.Failure, owned.second);
  }
  CHECK(sources.Counters().Refused == 1, "retry-cache hit does not issue another source request");
  transport.Now = 5000;
  transport.Authorized = true;
  outshine::Ground::TilePool::Landing recovered;
  recovered.Failure = owned.first.Failure();
  CHECK(pool.BytesBlocking(request, &recovered) == outshine::Ground::TilePool::Reply::Ready &&
            !recovered.Failure && recovered.SourceId == "primary",
        "successful recovery clears previous diagnostic payloads");
  const auto deliveries = sources.Counters().Delivered;
  for (int repeat = 0; repeat < 2; ++repeat) {
    CHECK(pool.BytesBlocking(request, &recovered) == outshine::Ground::TilePool::Reply::Ready &&
              !recovered.Failure && recovered.Bytes == std::vector<uint8_t>({11, 22}),
          "recovered bytes remain resident with no stale refusal");
  }
  CHECK(sources.Counters().Delivered == deliveries,
        "repeated recovered requests do not consult source providers again");
  for (const auto reason : {FetchFailureReason::TimedOut,
                            FetchFailureReason::Cancelled,
                            FetchFailureReason::Unavailable}) {
    ContentStore empty({.Using = ContentStore::Use::Off});
    SourceSet isolated(empty);
    AddSources(isolated);
    MissingTransport failed;
    failed.Reason = reason;
    auto query = isolated.Ask(request);
    const auto reply = isolated.Collect(query, failed);
    CHECK(reply.Failure() && reply.Failure()->Reason == reason &&
              reply.Failure()->SourceId == "fallback",
          "transport failures retain their distinct cause through source fallback");
  }
  return Report();
}
