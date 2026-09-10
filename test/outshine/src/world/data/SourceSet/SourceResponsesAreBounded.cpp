#include "SourceSet.h"
#include "Check.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;

class ClockTransport final : public Transport {
public:
  double Now = 0;
  int Begins = 0;
  int Cancels = 0;

  Ticket Begin(const std::string &) override { return static_cast<Ticket>(++Begins); }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override { ++Cancels; }

  double NowMs() override { return Now; }
};

class ScriptedSource final : public Source {
public:
  SourceDecl Decl;
  mutable std::vector<Fetched> Answers;
  mutable size_t Calls = 0;

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  Ticket Begin(const Address &, Transport &transport) const override {
    return transport.Begin(Decl.Id);
  }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    const size_t at = Calls++;
    return at < Answers.size() ? std::move(Answers[at]) : Fetched::Meant(Meaning::Refused);
  }
};

template <class... Answers> auto SourceWith(std::string name, Rank order, Answers &&...answers) {
  auto source = std::make_unique<ScriptedSource>();
  source->Decl.Id = std::move(name);
  source->Decl.Order = order;
  source->Decl.Keeps = Cacheability::Never;
  source->Answers.reserve(sizeof...(Answers));
  (source->Answers.push_back(std::forward<Answers>(answers)), ...);
  return source;
}
}

int main() {
  using namespace outshine::Test;
  const Fetch request(DataKind::Elevation, Address::Whole(0));
  ContentStore store({.Using = ContentStore::Use::Off});
  ClockTransport transport;
  {
    SourceSet sources(store);
    auto source = SourceWith("invalid", Rank{0}, Fetched::Meant(static_cast<Meaning>(255)));
    const auto *probe = source.get();
    CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted, "source registered");
    auto query = sources.Ask(request);
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Refused,
          "unknown response is refused");
    CHECK(probe->Calls == 1, "unknown response never reenters the source in the same collect");
    CHECK(sources.Counters().Refused == 1, "invalid response counted once");
  }
  {
    SourceSet sources(store);
    CHECK(sources.Add(SourceWith("absent", Rank{0}, Fetched::Meant(Meaning::Absent))) ==
              SourceSet::Registration::Accepted,
          "first source registered");
    CHECK(sources.Add(SourceWith("bytes", Rank{1}, Fetched::Delivered({1, 2, 3}))) ==
              SourceSet::Registration::Accepted,
          "fallback source registered");
    auto query = sources.Ask(request);
    auto delivery = sources.Collect(query, transport);
    const auto answer = delivery.Take();
    CHECK(answer && answer->SourceId == "bytes" && answer->Bytes == std::vector<uint8_t>({1, 2, 3}),
          "absent response hands over to the next ranked source");
    const auto ledger = sources.Counters();
    CHECK(ledger.Asked == 2 && ledger.HandedOver == 1 && ledger.Delivered == 1 &&
              ledger.DeliveredBytes == 3,
          "fallback updates the correct counters");
  }
  {
    SourceSet sources(store);
    auto source = SourceWith("retry",
                             Rank{0},
                             Fetched::Working(),
                             Fetched::MeantAfter(Meaning::Retry, 0.5),
                             Fetched::Delivered({4}));
    source->Decl.RetryBudget = 1;
    const auto *probe = source.get();
    CHECK(sources.Add(std::move(source)) == SourceSet::Registration::Accepted,
          "retry source registered");
    auto query = sources.Ask(request);
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Pending,
          "working remains pending");
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Pending,
          "retry schedules backoff");
    transport.Now = 499;
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Pending &&
              probe->Calls == 2,
          "provider is not polled during retry delay");
    transport.Now = 500;
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Pending &&
              probe->Calls == 2,
          "deadline restarts the transport before collecting again");
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Delivered,
          "retry delivers");
    CHECK(sources.Counters().Retried == 1, "retry budget consumed once");
  }
  {
    SourceSet sources(store);
    CHECK(sources.Add(SourceWith("refused", Rank{0}, Fetched::Meant(Meaning::Retry))) ==
              SourceSet::Registration::Accepted,
          "zero retry budget registered");
    auto query = sources.Ask(request);
    const auto answer = sources.Collect(query, transport);
    CHECK(answer.Where() == Delivery::State::Refused && answer.AfterMs() == 4000,
          "exhausted retry budget returns refusal with bounded fallback delay");
  }
  {
    SourceSet sources(store);
    CHECK(sources.Add(SourceWith("pending", Rank{0}, Fetched::Working())) ==
              SourceSet::Registration::Accepted,
          "pending source registered");
    auto query = sources.Ask(request);
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Pending,
          "live ticket created");
    SourceSet::Abandon(query, transport);
    CHECK(transport.Cancels == 1, "abandon cancels the live transport ticket");
    CHECK(sources.Collect(query, transport).Where() == Delivery::State::Consumed,
          "abandoned query is consumed and cannot restart");
  }
  return Report();
}
