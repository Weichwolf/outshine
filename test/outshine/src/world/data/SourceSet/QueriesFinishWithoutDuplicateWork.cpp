#include "SourceSet.h"
#include "Check.h"
#include <memory>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <string>
#include <type_traits>
#include <utility>

namespace {
using namespace outshine::Data;

class ProbeTransport final : public Transport {
public:
  int Begins = 0;
  int Cancels = 0;

  Ticket Begin(const std::string &) override { return static_cast<Ticket>(++Begins); }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override { ++Cancels; }
};

class ProbeSource final : public Source {
public:
  SourceDecl Decl{.Id = "probe", .Keeps = Cacheability::Never};
  Meaning Result = Meaning::Bytes;
  bool Working = false;
  mutable int Calls = 0;

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  Ticket Begin(const Address &, Transport &transport) const override {
    return transport.Begin(Decl.Id);
  }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    ++Calls;
    if (Working) { return Fetched::Working(); }
    return Result == Meaning::Bytes ? Fetched::Delivered({1}) : Fetched::Meant(Result);
  }
};
}

int main() {
  using namespace outshine::Test;
  ContentStore store({.Using = ContentStore::Use::Off});
  const Fetch request(DataKind::Elevation, Address::Whole(0));
  CHECK(!std::is_move_assignable_v<SourceSet::Query>,
        "assignment cannot overwrite an active ticket");
  for (const Meaning outcome : {Meaning::Bytes, Meaning::Absent, Meaning::Refused}) {
    SourceSet sources(store);
    ProbeTransport transport;
    auto source = std::make_unique<ProbeSource>();
    source->Result = outcome;
    const auto *probe = source.get();
    CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted, "source registered");
    auto query = sources.Ask(request);
    const auto first = sources.Collect(query, transport);
    CHECK(first.Where() != Delivery::State::Pending, "script produces a terminal result");
    const auto before = sources.Counters();
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Consumed,
          "terminal query does not manufacture a second result");
    const auto after = sources.Counters();
    CHECK(probe->Calls == 1 && transport.Begins == 1, "completed source is never polled again");
    CHECK(before.Delivered == after.Delivered && before.Refused == after.Refused &&
              before.Vacant == after.Vacant,
          "terminal query does not recount its outcome");
  }
  {
    SourceSet sources(store);
    ProbeTransport transport;
    auto query = sources.Ask(request);
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Undeclared,
          "missing source reported");
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Consumed &&
              sources.Counters().Undeclared == 1,
          "undeclared result is counted once");
  }
  {
    SourceSet sources(store);
    SourceSet foreign(store);
    ProbeTransport transport;
    auto source = std::make_unique<ProbeSource>();
    auto *probe = source.get();
    CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted,
          "movable query source registered");
    auto query = sources.Ask(request);
    CHECK(foreign.Collect(query, transport).Where() == Delivery::State::Refused &&
              probe->Calls == 0,
          "foreign source set cannot execute a query");
    probe->Working = true;
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Pending,
          "owner starts a live ticket");
    auto moved(std::move(query));
    SourceSet::Abandon(query, transport);
    CHECK(transport.Cancels == 0, "moved-from query cannot cancel transferred ticket");
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Consumed,
          "moved-from query cannot issue requests");
    probe->Working = false;
    CHECK(sources.Collect(moved, transport).Where() == Delivery::State::Delivered &&
              transport.Begins == 1,
          "moved query completes the existing ticket");
    auto cancelled = sources.Ask(request);
    probe->Working = true;
    CHECK(sources.Collect(cancelled, transport).Where() == Delivery::State::Pending,
          "second live ticket created");
    SourceSet::Abandon(cancelled, transport);
    SourceSet::Abandon(cancelled, transport);
    CHECK(transport.Cancels == 1 &&
              sources.Collect(cancelled, transport).Where() == Delivery::State::Consumed,
          "abandon is idempotent and terminal");
  }
  {
    auto directory =
        (std::filesystem::temp_directory_path() / "outshine-query-cache-XXXXXX").string();
    CHECK(mkdtemp(directory.data()) != nullptr, "cache directory created");
    if (!std::filesystem::is_directory(directory)) { return Report(); }
    ContentStore cache({.Directory = directory});
    SourceSet sources(cache);
    ProbeTransport transport;
    auto source = std::make_unique<ProbeSource>();
    source->Decl.Keeps = Cacheability::Forever;
    const auto *probe = source.get();
    const std::array<uint8_t, 3> bytes{1, 2, 3};
    CHECK(cache.Keep(ContentKey(source->Decl, request.Where()), bytes.data(), bytes.size()),
          "cache entry prepared independently of the source");
    CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted,
          "cached source registered");
    auto query = sources.Ask(request);
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Delivered,
          "cache satisfies query");
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Consumed &&
              probe->Calls == 0 && transport.Begins == 0 && sources.Counters().FromStore == 1,
          "cached completion cannot enter the source afterward");
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    CHECK(!error, "cache directory removed");
  }
  return Report();
}
