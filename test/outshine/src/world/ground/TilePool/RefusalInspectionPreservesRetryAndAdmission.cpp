#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <expected>

namespace {
using namespace outshine::Data;

class ClockTransport final : public Transport {
public:
  std::atomic<double> Now = 0.0;

  FetchStart Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}

  double NowMs() override { return Now.load(); }
};

class ControlledSource final : public Source {
public:
  ControlledSource() {
    Decl.Id = "offline-vector";
    Decl.Revision = "test-r3";
    Decl.Kind = DataKind::VectorMap;
    Decl.Keeps = Cacheability::Never;
  }

  SourceDecl Decl;
  mutable std::atomic<int> Starts = 0;
  std::atomic<bool> Ready = false;

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &at, Transport &) const override {
    ++Starts;
    if (Ready.load()) { return Ticket::None; }
    return std::unexpected(at.Tile()->X == 0 ? FetchFailureReason::OfflineMiss
                                             : FetchFailureReason::Unavailable);
  }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Delivered({7});
  }
};
}

int main() {
  using namespace outshine::Test;
  using outshine::Ground::TilePool;
  RawSourceCache::Config cache;
  cache.Using = RawSourceCache::Use::Off;
  RawSourceCache store(cache);
  SourceSet sources(store);
  ClockTransport transport;
  auto source = std::make_unique<ControlledSource>();
  auto *control = source.get();
  CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted,
        "controlled vector source registers");
  TilePool pool({}, sources, transport);
  const Fetch missing(DataKind::VectorMap, Address::At({.Zoom = 1, .X = 0, .Y = 1}));
  const Fetch transient(DataKind::VectorMap, Address::At({.Zoom = 1, .X = 1, .Y = 1}));
  const auto before = pool.Counters();
  const auto emptyBytes = pool.ByteCacheBytes();
  CHECK(!pool.Refusal(missing) && !pool.Refusal(transient),
        "unknown requests have no cached refusal");
  CHECK(control->Starts == 0 && pool.ByteCacheBytes() == emptyBytes &&
            pool.Counters().Posts == before.Posts && pool.Counters().Fetches == before.Fetches,
        "failure inspection neither starts IO nor consumes admission or cache storage");
  TilePool::Landing landing;
  CHECK(pool.BytesBlocking(missing, &landing) == TilePool::Reply::Refused,
        "offline source request is classified as refused");
  const auto failed = pool.Refusal(missing);
  CHECK(failed && failed->Reason == FetchFailureReason::OfflineMiss &&
            failed->Requested.Text() == missing.Where().Text() && failed->Served &&
            failed->Served->Text() == missing.Where().Text() &&
            failed->SourceId == "offline-vector" && failed->SourceRevision == "test-r3" &&
            !failed->SourceKey.empty(),
        "owned failure retains exact demand and provider identity");
  CHECK(pool.BytesBlocking(transient, &landing) == TilePool::Reply::Refused,
        "temporarily unavailable transport is also classified as refused");
  const auto retry = pool.Refusal(transient);
  CHECK(retry && retry->Reason == FetchFailureReason::Unavailable,
        "temporary refusal stays distinguishable from an offline miss");
  const auto starts = control->Starts.load();
  const auto refusedBytes = pool.ByteCacheBytes();
  const auto refused = pool.Counters();
  for (int i = 0; i < 20; ++i) {
    CHECK(pool.Refusal(missing).has_value(), "refusal remains observable");
  }
  CHECK(control->Starts == starts && pool.ByteCacheBytes() == refusedBytes &&
            pool.Counters().Posts == refused.Posts && pool.Counters().Fetches == refused.Fetches,
        "repeated inspection neither retries nor changes residency");
  transport.Now = 1000000.0;
  CHECK(!pool.Refusal(missing) && !pool.Refusal(transient),
        "expired refusal is not reported as a currently blocked request");
  control->Ready = true;
  CHECK(pool.BytesBlocking(missing, &landing) == TilePool::Reply::Ready &&
            landing.Bytes == std::vector<uint8_t>{7} && !pool.Refusal(missing),
        "normal retry replaces refusal with the recovered payload");
  CHECK(failed && failed->Reason == FetchFailureReason::OfflineMiss &&
            failed->SourceRevision == "test-r3",
        "a returned diagnostic remains owned after cache replacement");
  CHECK(pool.BytesBlocking(transient, &landing) == TilePool::Reply::Ready &&
            !pool.Refusal(transient),
        "temporary refusal still recovers through the original loading path");
  return Report();
}
