#include "Check.h"
#include "ContentStore.h"
#include "SourceSet.h"

#include <memory>
#include <string>
#include <utility>

using namespace outshine;
using namespace outshine::Data;

namespace {

class Scripted : public Source {
public:
  Scripted(std::string id, Rank order, Meaning answer, Coverage coverage)
      : Answer_(answer), Coverage_(coverage) {
    Decl_.Id = std::move(id);
    Decl_.Kind = DataKind::Elevation;
    Decl_.How = Scheme::TileZxy;
    Decl_.Order = order;
    Decl_.MinZoom = 0;
    Decl_.MaxZoom = 15;
    Decl_.Keeps = Cacheability::Never;
    Decl_.RetryBudget = 0;
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }
  [[nodiscard]] Coverage Covers(const Request &request) const noexcept override {
    (void)request;
    return Coverage_;
  }
  [[nodiscard]] Address Serves(const Request &request) const noexcept override {
    return request.Where();
  }
  [[nodiscard]] Ticket Begin(const Address &at, Transport &transport) const override {
    (void)at;
    (void)transport;
    Asked++;
    return Ticket{1};
  }
  [[nodiscard]] Fetched Collect(const Address &at, Ticket ticket,
                                Transport &transport) const override {
    (void)at;
    (void)ticket;
    (void)transport;
    if (Answer_ != Meaning::Bytes) return Fetched::Meant(Answer_);
    return Fetched::Delivered(std::vector<uint8_t>{7, 7, 7});
  }

  mutable int Asked = 0;

private:
  SourceDecl Decl_;
  Meaning Answer_;
  Coverage Coverage_;
};

class SilentTransport : public Transport {
public:
  [[nodiscard]] Ticket Begin(const std::string &url) override {
    (void)url;
    return Ticket{1};
  }
  [[nodiscard]] Wire Collect(Ticket ticket) override {
    (void)ticket;
    return Wire::Working();
  }
  void Cancel(Ticket ticket) override { (void)ticket; }
};

[[nodiscard]] ContentStore::Config StoreOff() {
  ContentStore::Config c;
  c.Using = ContentStore::Use::Off;
  return c;
}

const Request kSomewhere(DataKind::Elevation, Address::Tile(14, 8620, 5403));

}

int main() {
  Test::Covers("I.39 an Absent from one source hands over to the next");
  Test::Covers("I.40 the terminal absence is the exhaustion of the list");
  Test::Covers("I.46 a duplicate rank within one kind is refused at registration");

  SilentTransport transport;

  {
    ContentStore store(StoreOff());
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<Scripted>("first", Rank{0}, Meaning::Bytes,
                                                 Coverage::Inside)) ==
              SourceSet::Registration::Accepted,
          "rank 0 registers");
    CHECK(sources.Add(std::make_unique<Scripted>("second", Rank{0}, Meaning::Bytes,
                                                 Coverage::Inside)) ==
              SourceSet::Registration::DuplicateRank,
          "a second source of the same kind at the same rank is refused");
    CHECK(sources.Count() == 1, "and it is not in the registry");
  }

  {
    ContentStore store(StoreOff());
    SourceSet sources(store);
    auto vacantOwned = std::make_unique<Scripted>("national", Rank{0}, Meaning::Absent,
                                                  Coverage::Inside);
    auto holderOwned = std::make_unique<Scripted>("global", Rank{1}, Meaning::Bytes,
                                                  Coverage::Inside);
    const Scripted *vacant = vacantOwned.get();
    const Scripted *holder = holderOwned.get();
    CHECK(sources.Add(std::move(vacantOwned)) == SourceSet::Registration::Accepted, "rank 0 registers");
    CHECK(sources.Add(std::move(holderOwned)) == SourceSet::Registration::Accepted, "rank 1 registers");

    SourceSet::Query query = sources.Ask(kSomewhere);
    Delivery answer = sources.Collect(query, transport);
    Delivery::Answer taken;
    CHECK(answer.TryTake(&taken), "the query delivered rather than ending at the first absence");
    CHECK(taken.SourceId == "global", "the bytes came from the lower-ranked source");
    CHECK(vacant->Asked == 1, "the higher-ranked source was asked first");
    CHECK(holder->Asked == 1, "and the lower-ranked one was asked exactly once after it");
    CHECK(sources.Counters().HandedOver == 1, "the ledger records one handover");
  }

  {
    ContentStore store(StoreOff());
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<Scripted>("a", Rank{0}, Meaning::Absent,
                                                 Coverage::Inside)) ==
              SourceSet::Registration::Accepted,
          "rank 0 registers");
    CHECK(sources.Add(std::make_unique<Scripted>("b", Rank{1}, Meaning::Absent,
                                                 Coverage::Inside)) ==
              SourceSet::Registration::Accepted,
          "rank 1 registers");
    SourceSet::Query query = sources.Ask(kSomewhere);
    Delivery answer = sources.Collect(query, transport);
    CHECK(answer.Where() == Delivery::State::Vacant,
          "every covering source absent is Vacant, which is a statement about the world");
    Delivery::Answer taken;
    CHECK(!answer.TryTake(&taken), "and it carries no bytes and no source to read them from");
  }

  {
    ContentStore store(StoreOff());
    SourceSet sources(store);
    auto refuserOwned = std::make_unique<Scripted>("refuser", Rank{0}, Meaning::Refused,
                                                   Coverage::Inside);
    auto holderOwned = std::make_unique<Scripted>("holder", Rank{1}, Meaning::Bytes,
                                                  Coverage::Inside);
    const Scripted *holder = holderOwned.get();
    CHECK(sources.Add(std::move(refuserOwned)) == SourceSet::Registration::Accepted, "rank 0 registers");
    CHECK(sources.Add(std::move(holderOwned)) == SourceSet::Registration::Accepted, "rank 1 registers");
    SourceSet::Query query = sources.Ask(kSomewhere);
    Delivery answer = sources.Collect(query, transport);
    CHECK(answer.Where() == Delivery::State::Refused, "a refusal is its own answer");
    CHECK(holder->Asked == 0, "and it does not hand over, because it says nothing about the place");
  }

  {
    ContentStore store(StoreOff());
    SourceSet sources(store);
    auto outsideOwned = std::make_unique<Scripted>("elsewhere", Rank{0}, Meaning::Bytes,
                                                   Coverage::Outside);
    auto holderOwned = std::make_unique<Scripted>("here", Rank{1}, Meaning::Bytes,
                                                  Coverage::Inside);
    const Scripted *outside = outsideOwned.get();
    CHECK(sources.Add(std::move(outsideOwned)) == SourceSet::Registration::Accepted, "rank 0 registers");
    CHECK(sources.Add(std::move(holderOwned)) == SourceSet::Registration::Accepted, "rank 1 registers");
    SourceSet::Query query = sources.Ask(kSomewhere);
    Delivery answer = sources.Collect(query, transport);
    Delivery::Answer taken;
    CHECK(answer.TryTake(&taken), "the covering source answered");
    CHECK(taken.SourceId == "here", "and it is the one that declared the place inside its domain");
    CHECK(outside->Asked == 0, "the source that declared Outside was never asked");
  }

  return Test::Report();
}
