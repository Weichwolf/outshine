#include "Check.h"
#include "OfflineTransport.h"
#include "SourceSet.h"
#include "TerrariumDem.h"
#include <memory>
#include <string>

namespace {
using namespace outshine::Data;

class AdmissionTransport final : public Transport {
public:
  FetchFailureReason Reason = FetchFailureReason::CapacityRefused;
  int Begins = 0, Collects = 0, Cancels = 0;
  bool FirstNotFound = false, FirstRetry = false;
  double Now = 0;

  FetchStart Begin(const std::string &) override {
    ++Begins;
    if ((FirstNotFound || FirstRetry) && Begins == 1) { return Ticket{1}; }
    return std::unexpected(Reason);
  }

  Wire Collect(Ticket) override {
    ++Collects;
    return Wire::Answered(FirstNotFound ? 404 : 503, {});
  }

  void Cancel(Ticket) override { ++Cancels; }

  double NowMs() override { return Now; }
};
}

int main() {
  using namespace outshine::Test;
  const Fetch request(DataKind::Elevation, Address::At({.Zoom = 17, .X = 68660, .Y = 44812}));
  for (const auto reason : {FetchFailureReason::CapacityRefused,
                            FetchFailureReason::Unavailable,
                            FetchFailureReason::InvalidRequest,
                            FetchFailureReason::OfflineMiss}) {
    for (const bool fallback : {false, true}) {
      ContentStore store({.Using = ContentStore::Use::Off});
      SourceSet sources(store);
      CHECK(sources.Add(std::make_unique<TerrariumDem>(
                "primary-pin",
                Rank{0},
                AbsencePolicy::Continue,
                "primary",
                "https://primary.example/{z}/{x}/{y}.png")) == SourceSet::Registration::Accepted,
            "primary provider registered");
      CHECK(sources.Add(std::make_unique<TerrariumDem>(
                "fallback-pin",
                Rank{1},
                AbsencePolicy::Continue,
                "fallback",
                "https://fallback.example/{z}/{x}/{y}.png")) == SourceSet::Registration::Accepted,
            "fallback provider registered");
      const auto &decl = sources.At(fallback ? 1 : 0).Declaration();
      const auto key = SourceKey(decl);
      AdmissionTransport transport;
      transport.Reason = reason;
      transport.FirstNotFound = fallback;
      auto query = sources.Ask(request);
      auto response = sources.Collect(query, transport);
      CHECK(response.Where() == Delivery::State::Refused && response.Failure(),
            "refused admission settles directly without collecting a nonexistent ticket");
      const auto failure = response.Failure();
      CHECK(failure && failure->Reason == reason && failure->SourceId == decl.Id &&
                failure->SourceRevision == decl.Revision && failure->SourceKey == key &&
                failure->Requested == request.Where() &&
                failure->Served == Address::At({.Zoom = 15, .X = 17165, .Y = 11203}),
            "admission cause retains actual provider and ancestor context");
      CHECK(transport.Begins == (fallback ? 2 : 1) && transport.Collects == (fallback ? 1 : 0) &&
                transport.Cancels == 0 && sources.Counters().Retried == 0,
            "failed admission is never collected, cancelled or retried");
      CHECK(sources.Collect(query, transport).Where() == Delivery::State::Consumed,
            "settled admission is consumed exactly once");
    }
  }
  {
    ContentStore store({.Using = ContentStore::Use::Off});
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<TerrariumDem>("pin", Rank{0}, AbsencePolicy::Fail)) ==
              SourceSet::Registration::Accepted,
          "retry provider registered");
    AdmissionTransport transport;
    transport.FirstRetry = true;
    auto query = sources.Ask(request);
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Pending,
          "first HTTP refusal schedules retry");
    transport.Now = 100000;
    const auto refused = sources.Collect(query, transport);
    CHECK(refused.Where() == Delivery::State::Refused && refused.Failure() &&
              refused.Failure()->Reason == FetchFailureReason::CapacityRefused &&
              transport.Begins == 2 && transport.Collects == 1 && transport.Cancels == 0,
          "retry admission failure settles with its own cause before another collect");
  }
  OfflineTransport offline;
  const auto start = offline.Begin("https://example.test/tile");
  CHECK(!start && start.error() == FetchFailureReason::OfflineMiss,
        "offline transport identifies refusal at admission");
  return Report();
}
