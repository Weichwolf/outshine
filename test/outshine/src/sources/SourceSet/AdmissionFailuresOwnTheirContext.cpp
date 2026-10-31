#include "Check.h"
#include "OfflineSourceTransport.h"
#include "SourceSet.h"
#include "WebTileFixture.h"
#include <expected>
#include <memory>
#include <string>

namespace {
using namespace outshine::Data;
constexpr int kNotFound = 404;
constexpr int kUnavailable = 503;
constexpr double kAfterRetryMs = 100000;

class AdmissionTransport final : public SourceTransport {
public:
  SourceFailureReason Reason = SourceFailureReason::CapacityRefused;
  int Begins = 0, Collects = 0, Cancels = 0;
  bool FirstNotFound = false, FirstRetry = false;
  double Now = 0;

  FetchStart Begin(const std::string & /*url*/) override {
    ++Begins;
    if ((FirstNotFound || FirstRetry) && Begins == 1) { return Ticket{1}; }
    return std::unexpected(Reason);
  }

  Wire Collect(Ticket /*ticket*/) override {
    ++Collects;
    return Wire::Answered(FirstNotFound ? kNotFound : kUnavailable, {});
  }

  void Cancel(Ticket /*ticket*/) override { ++Cancels; }

  double NowMs() override { return Now; }
};
}

int main() {
  using namespace outshine::Test;
  const SourceRequest request(DataKind::Elevation,
                              Address::At({.Zoom = 17, .X = 68660, .Y = 44812}));
  for (const auto reason : {SourceFailureReason::CapacityRefused,
                            SourceFailureReason::Unavailable,
                            SourceFailureReason::InvalidRequest,
                            SourceFailureReason::OfflineMiss}) {
    for (const bool fallback : {false, true}) {
      RawSourceCache store({.Directory = {}, .Using = RawSourceCache::Use::Off});
      SourceSet sources(store);
      CHECK(sources.Add(std::make_unique<outshine::Test::WebTileFixture>(
                "primary-pin",
                Rank{0},
                AbsencePolicy::Continue,
                "primary",
                "https://primary.example/{z}/{x}/{y}.png")) == SourceSet::Registration::Accepted,
            "primary provider registered");
      CHECK(sources.Add(std::make_unique<outshine::Test::WebTileFixture>(
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
      const auto response = sources.Collect(query, transport);
      CHECK(response.Where() == SourceDelivery::State::Refused && response.Failure(),
            "refused admission settles directly without collecting a nonexistent ticket");
      const auto &failure = response.Failure();
      CHECK(failure && failure->Reason == reason && failure->SourceId == decl.Id &&
                failure->SourceRevision == decl.Revision && failure->SourceKey == key &&
                failure->Requested == request.Where() &&
                failure->Served == Address::At({.Zoom = 15, .X = 17165, .Y = 11203}),
            "admission cause retains actual provider and ancestor context");
      CHECK(transport.Begins == (fallback ? 2 : 1) && transport.Collects == (fallback ? 1 : 0) &&
                transport.Cancels == 0 && sources.Counters().Retried == 0,
            "failed admission is never collected, cancelled or retried");
      CHECK(sources.Collect(query, transport).Where() == SourceDelivery::State::Consumed,
            "settled admission is consumed exactly once");
    }
  }
  {
    RawSourceCache store({.Directory = {}, .Using = RawSourceCache::Use::Off});
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<outshine::Test::WebTileFixture>(
              "pin", Rank{0}, AbsencePolicy::Fail)) == SourceSet::Registration::Accepted,
          "retry provider registered");
    AdmissionTransport transport;
    transport.FirstRetry = true;
    auto query = sources.Ask(request);
    CHECK(sources.Collect(query, transport).Where() == SourceDelivery::State::Pending,
          "first HTTP refusal schedules retry");
    transport.Now = kAfterRetryMs;
    const auto refused = sources.Collect(query, transport);
    CHECK(refused.Where() == SourceDelivery::State::Refused && refused.Failure() &&
              refused.Failure()->Reason == SourceFailureReason::CapacityRefused &&
              transport.Begins == 2 && transport.Collects == 1 && transport.Cancels == 0,
          "retry admission failure settles with its own cause before another collect");
  }
  OfflineSourceTransport offline;
  const auto start = offline.Begin("https://example.test/tile");
  CHECK(!start && start.error() == SourceFailureReason::OfflineMiss,
        "offline transport identifies refusal at admission");
  return Report();
}
