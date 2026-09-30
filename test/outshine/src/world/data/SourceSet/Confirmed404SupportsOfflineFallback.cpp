#include "Check.h"
#include "ContentStore.h"
#include "OfflineTransport.h"
#include "SourceSet.h"
#include "TerrariumDem.h"
#include <world/data/Transport.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine::Data;

class Http final : public Transport {
public:
  int Status = 404;
  std::optional<FetchFailureReason> Failure;
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

  void Cancel(Ticket) override {}
};

class UnprovenAbsence final : public Source {
public:
  UnprovenAbsence() {
    Decl.Id = "unproven";
    Decl.Keeps = Cacheability::Forever;
  }

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Meant(Meaning::Absent);
  }

  SourceDecl Decl;
};

void Add(SourceSet &sources,
         std::string revision = {},
         AbsencePolicy policy = AbsencePolicy::Continue) {
  using namespace outshine::Test;
  CHECK(sources.Add(std::make_unique<TerrariumDem>(
            revision, Rank{0}, policy, "primary", "https://primary.example/{z}/{x}/{y}.png")) ==
            SourceSet::Registration::Accepted,
        "primary registered");
  CHECK(sources.Add(std::make_unique<TerrariumDem>("",
                                                   Rank{1},
                                                   AbsencePolicy::Continue,
                                                   "fallback",
                                                   "https://fallback.example/{z}/{x}/{y}.png")) ==
            SourceSet::Registration::Accepted,
        "fallback registered");
}
}

int main() {
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-404-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated cache created");
  int64_t now = 1000;
  const ContentStore::Config config{.Directory = directory, .UtcSeconds = [&now] { return now; }};
  const Fetch request(DataKind::Elevation, Address::At({.Zoom = 17, .X = 69, .Y = 45}));
  {
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources);
    Http online;
    auto query = sources.Ask(request);
    const auto bytes = sources.Collect(query, online).Take();
    CHECK(bytes && bytes->SourceId == "fallback" && online.Begins == 2,
          "real not-found evidence and fallback bytes obtained online");
  }
  {
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources);
    OfflineTransport offline;
    auto query = sources.Ask(request);
    const auto bytes = sources.Collect(query, offline).Take();
    CHECK(bytes && bytes->SourceId == "fallback" && bytes->Bytes == std::vector<uint8_t>({11, 22}),
          "immediate offline replay skips confirmed missing ancestor and delivers fallback");
    CHECK(sources.Counters().ProviderStarts == 0 && store.Counters().Misses == 0 &&
              store.Counters().Hits == 2,
          "known absence and bytes are cache hits without any provider start");
  }
  {
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources, {}, AbsencePolicy::Fail);
    OfflineTransport offline;
    auto query = sources.Ask(request);
    const auto reply = sources.Collect(query, offline);
    CHECK(reply.Failure() && reply.Failure()->Reason == FetchFailureReason::ConfirmedAbsent &&
              reply.Failure()->SourceId == "primary" && sources.Counters().ProviderStarts == 0,
          "fail policy applies identically to cached not-found evidence");
  }
  for (const bool changedRevision : {true, false}) {
    if (!changedRevision) { now += ContentStore::UnpinnedAbsenceLifetimeS; }
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources, changedRevision ? "different-pin" : "");
    OfflineTransport offline;
    auto query = sources.Ask(request);
    const auto reply = sources.Collect(query, offline);
    CHECK(reply.Failure() && reply.Failure()->Reason == FetchFailureReason::OfflineMiss,
          "expired or differently pinned absence cannot masquerade as confirmed missing data");
  }
  for (const int status : {403, 408, 500}) {
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources, std::to_string(status));
    Http failed;
    failed.Status = status;
    auto query = sources.Ask(request);
    auto reply = sources.Collect(query, failed);
    for (int poll = 0; poll < 20 && reply.Where() == Delivery::State::Pending; ++poll) {
      reply = sources.Collect(query, failed);
    }
    CHECK(reply.Where() == Delivery::State::Refused, "non-404 failure terminates with refusal");
    CHECK(reply.Failure() &&
              reply.Failure()->Reason == (status == 408 ? FetchFailureReason::TimedOut
                                                        : FetchFailureReason::ProviderRefused),
          "HTTP timeout remains distinct from provider refusal after retry exhaustion");
    CHECK(store.Lookup(ContentKey(sources.At(0).Declaration(), sources.At(0).Serves(request)))
                  .Where == ContentStore::Presence::Unknown,
          "forbidden, timeout and server errors never create absence evidence");
  }
  for (const auto reason : {FetchFailureReason::Cancelled, FetchFailureReason::CapacityRefused}) {
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources, std::string(Name(reason)));
    Http failed;
    failed.Failure = reason;
    auto query = sources.Ask(request);
    const auto reply = sources.Collect(query, failed);
    CHECK(reply.Where() == Delivery::State::Refused && reply.Failure() &&
              reply.Failure()->Reason == reason && sources.Counters().Retried == 0 &&
              failed.Begins == 1,
          "cancellation and body-budget refusals never restart a transfer");
    CHECK(store.Lookup(ContentKey(sources.At(0).Declaration(), sources.At(0).Serves(request)))
                  .Where == ContentStore::Presence::Unknown,
          "terminal transport errors cannot create absence evidence");
  }
  {
    ContentStore store(config);
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<UnprovenAbsence>()) == SourceSet::Registration::Accepted,
          "unproven source registered");
    OfflineTransport offline;
    auto query = sources.Ask(request);
    CHECK(sources.Collect(query, offline).Where() == Delivery::State::Vacant,
          "generic absence can finish its current request");
    CHECK(store.Lookup(ContentKey(sources.At(0).Declaration(), request.Where())).Where ==
              ContentStore::Presence::Unknown,
          "generic absence without explicit HTTP evidence cannot become a persistent fact");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "fixture removed");
  return Report();
}
