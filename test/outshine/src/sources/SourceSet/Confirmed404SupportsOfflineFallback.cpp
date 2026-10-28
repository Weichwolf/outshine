#include <cstdint>
#include "Check.h"
#include "RawSourceCache.h"
#include "OfflineSourceTransport.h"
#include "SourceSet.h"
#include "WebTileFixture.h"
#include <SourceTransport.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine::Data;

class Http final : public SourceTransport {
public:
  int Status = 404;
  std::optional<SourceFailureReason> Failure;
  int Begins = 0;
  double Clock = 0;

  FetchStart Begin(const std::string &url) override {
    ++Begins;
    return url.starts_with("https://primary.example/") ? Ticket{1} : Ticket{2};
  }

  Wire Collect(Ticket ticket) override {
    if (Failure) { return Wire::Unreachable(*Failure); }
    return ticket == Ticket{1} ? Wire::Answered(Status, {}) : Wire::Answered(200, {11, 22});
  }

  double NowMs() override {
    Clock += 10000;
    return Clock;
  }

  void Cancel(Ticket /*ticket*/) override {}
};

class UnprovenAbsence final : public Source {
public:
  UnprovenAbsence() {
    Decl.Id = "unproven";
    Decl.Keeps = Cacheability::Forever;
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl; }

  [[nodiscard]] Coverage Covers(const SourceRequest & /*request*/) const noexcept override {
    return Coverage::Inside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address & /*at*/, SourceTransport & /*transport*/) const override {
    return Ticket::None;
  }

  SourceResponse Collect(const Address & /*at*/,
                         Ticket /*ticket*/,
                         SourceTransport & /*transport*/) const override {
    return SourceResponse::Meant(Meaning::Absent);
  }

  SourceDecl Decl;
};

void Add(SourceSet &sources,
         const std::string &revision = {},
         AbsencePolicy policy = AbsencePolicy::Continue) {
  using namespace outshine::Test;
  CHECK(sources.Add(std::make_unique<outshine::Test::WebTileFixture>(
            revision, Rank{0}, policy, "primary", "https://primary.example/{z}/{x}/{y}.png")) ==
            SourceSet::Registration::Accepted,
        "primary registered");
  CHECK(sources.Add(std::make_unique<outshine::Test::WebTileFixture>(
            "",
            Rank{1},
            AbsencePolicy::Continue,
            "fallback",
            "https://fallback.example/{z}/{x}/{y}.png")) == SourceSet::Registration::Accepted,
        "fallback registered");
}
}

int main() {
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-404-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated cache created");
  const RawSourceCache::Config config{.Directory = directory};
  const SourceRequest request(DataKind::Elevation, Address::At({.Zoom = 17, .X = 69, .Y = 45}));
  {
    RawSourceCache store(config);
    SourceSet sources(store);
    Add(sources);
    Http online;
    auto query = sources.Ask(request);
    const auto bytes = sources.Collect(query, online).Take();
    CHECK(bytes && bytes->SourceId == "fallback" && online.Begins == 2,
          "real not-found evidence and fallback bytes obtained online");
  }
  {
    RawSourceCache store(config);
    SourceSet sources(store);
    Add(sources);
    OfflineSourceTransport offline;
    auto query = sources.Ask(request);
    const auto bytes = sources.Collect(query, offline).Take();
    CHECK(bytes && bytes->SourceId == "fallback" && bytes->Bytes == std::vector<uint8_t>({11, 22}),
          "immediate offline replay skips confirmed missing ancestor and delivers fallback");
    CHECK(sources.Counters().ProviderStarts == 0 && store.Counters().Misses == 0 &&
              store.Counters().Hits == 2,
          "known absence and bytes are cache hits without any provider start");
  }
  {
    RawSourceCache store(config);
    SourceSet sources(store);
    Add(sources, {}, AbsencePolicy::Fail);
    OfflineSourceTransport offline;
    auto query = sources.Ask(request);
    const auto reply = sources.Collect(query, offline);
    CHECK(reply.Failure() && reply.Failure()->Reason == SourceFailureReason::ConfirmedAbsent &&
              reply.Failure()->SourceId == "primary" && sources.Counters().ProviderStarts == 0,
          "fail policy applies identically to cached not-found evidence");
  }
  for (const bool changedRevision : {true, false}) {
    RawSourceCache store(config);
    SourceSet sources(store);
    Add(sources, changedRevision ? "different-pin" : "");
    OfflineSourceTransport offline;
    auto query = sources.Ask(request);
    auto reply = sources.Collect(query, offline);
    if (changedRevision) {
      CHECK(reply.Failure() && reply.Failure()->Reason == SourceFailureReason::OfflineMiss,
            "a different source revision cannot reuse previous not-found evidence");
    } else {
      const auto bytes = reply.Take();
      CHECK(bytes && bytes->SourceId == "fallback" && sources.Counters().ProviderStarts == 0,
            "unchanged source identity reuses the same cached fallback without an expiry");
    }
  }
  for (const int status : {403, 408, 500}) {
    RawSourceCache store(config);
    SourceSet sources(store);
    Add(sources, std::to_string(status));
    Http failed;
    failed.Status = status;
    auto query = sources.Ask(request);
    auto reply = sources.Collect(query, failed);
    for (int poll = 0; poll < 20 && reply.Where() == SourceDelivery::State::Pending; ++poll) {
      reply = sources.Collect(query, failed);
    }
    CHECK(reply.Where() == SourceDelivery::State::Refused,
          "non-404 failure terminates with refusal");
    CHECK(reply.Failure() &&
              reply.Failure()->Reason == (status == 408 ? SourceFailureReason::TimedOut
                                                        : SourceFailureReason::ProviderRefused),
          "HTTP timeout remains distinct from provider refusal after retry exhaustion");
    CHECK(store.Lookup(ContentKey(sources.At(0).Declaration(), sources.At(0).Serves(request)))
                  .Where == RawSourceCache::Presence::Unknown,
          "forbidden, timeout and server errors never create absence evidence");
  }
  for (const auto reason : {SourceFailureReason::Cancelled, SourceFailureReason::CapacityRefused}) {
    RawSourceCache store(config);
    SourceSet sources(store);
    Add(sources, std::string(Name(reason)));
    Http failed;
    failed.Failure = reason;
    auto query = sources.Ask(request);
    const auto reply = sources.Collect(query, failed);
    CHECK(reply.Where() == SourceDelivery::State::Refused && reply.Failure() &&
              reply.Failure()->Reason == reason && sources.Counters().Retried == 0 &&
              failed.Begins == 1,
          "cancellation and body-budget refusals never restart a transfer");
    CHECK(store.Lookup(ContentKey(sources.At(0).Declaration(), sources.At(0).Serves(request)))
                  .Where == RawSourceCache::Presence::Unknown,
          "terminal transport errors cannot create absence evidence");
  }
  {
    RawSourceCache store(config);
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<UnprovenAbsence>()) == SourceSet::Registration::Accepted,
          "unproven source registered");
    OfflineSourceTransport offline;
    auto query = sources.Ask(request);
    CHECK(sources.Collect(query, offline).Where() == SourceDelivery::State::Vacant,
          "generic absence can finish its current request");
    CHECK(store.Lookup(ContentKey(sources.At(0).Declaration(), request.Where())).Where ==
              RawSourceCache::Presence::Unknown,
          "generic absence without explicit HTTP evidence cannot become a persistent fact");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "fixture removed");
  return Report();
}
