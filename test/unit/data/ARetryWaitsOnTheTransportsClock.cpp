#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "ContentStore.h"
#include "SourceSet.h"
#include "TerrariumDem.h"
#include "Transport.h"

using namespace outshine;
using namespace outshine::Data;

namespace {

class StutteringTransport : public Transport {
public:
  int Begins = 0;
  int FailFirst = 2;
  double FakeNowMs = 0.0;

  [[nodiscard]] Ticket Begin(const std::string &url) override {
    (void)url;
    ++Begins;
    return Ticket{(uint64_t)Begins};
  }
  [[nodiscard]] Wire Collect(Ticket ticket) override {
    (void)ticket;
    if (Begins <= FailFirst) { return Wire::Answered(429, {}); }
    return Wire::Answered(200, std::vector<uint8_t>{1, 2, 3, 4});
  }
  void Cancel(Ticket ticket) override { (void)ticket; }
  [[nodiscard]] double NowMs(void) override { return FakeNowMs; }
};

[[nodiscard]] ContentStore::Config StoreOff() {
  ContentStore::Config c;
  c.Using = ContentStore::Use::Off;
  return c;
}

} // namespace

int main() {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  ContentStore store(StoreOff());
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<TerrariumDem>()) == SourceSet::Registration::Accepted,
        "a source registers");

  StutteringTransport wire;
  SourceSet::Query query = sources.Ask(Request(DataKind::Elevation, Address::Tile(14, 8620, 5403)));
  CHECK(!query.Undeclared(), "the request is declared");

  CHECK(sources.Collect(query, wire).Where() == Delivery::State::Pending && wire.Begins == 1,
        "the first ask goes out");
  CHECK(sources.Collect(query, wire).Where() == Delivery::State::Pending && wire.Begins == 1,
        "a 429 answers and the retry is SCHEDULED, not fired -- the wire is not begun again "
        "in the same breath");
  for (int poll = 0; poll < 8; ++poll) { (void)sources.Collect(query, wire); }
  CHECK(wire.Begins == 1,
        "**A 429 AT POLL CADENCE IS NOT HAMMERED**: eight polls inside the backoff window "
        "begin nothing -- a 429 is a request to go away (board:1691)");

  wire.FakeNowMs = 300.0;
  CHECK(sources.Collect(query, wire).Where() == Delivery::State::Pending && wire.Begins == 2,
        "past the first backoff the retry fires -- the TRANSPORT'S clock decides, so this "
        "proof never slept");
  (void)sources.Collect(query, wire);
  for (int poll = 0; poll < 4; ++poll) { (void)sources.Collect(query, wire); }
  CHECK(wire.Begins == 2, "the second 429 backs off LONGER -- 500 ms now, doubling");
  wire.FakeNowMs = 900.0;
  (void)sources.Collect(query, wire);
  CHECK(wire.Begins == 3, "and fires after the doubled window");

  Delivery landed = Delivery::Waiting();
  for (int poll = 0; poll < 4 && landed.Where() != Delivery::State::Delivered; ++poll) {
    landed = sources.Collect(query, wire);
  }
  CHECK(landed.Where() == Delivery::State::Delivered,
        "the 200 after two stutters delivers");
  CHECK(sources.Counters().Retried == 2, "and the ledger counted both scheduled retries");

  {
    // the budget-exhaust arm: a host that never recovers walks 429 through every retry and
    // lands on the fallthrough -- a flipped fallthrough or a lost Attempts_ reset goes red
    StutteringTransport dead;
    dead.FailFirst = 99;
    SourceSet::Query gone =
        sources.Ask(Request(DataKind::Elevation, Address::Tile(14, 8620, 5404)));
    Delivery ended = Delivery::Waiting();
    for (int poll = 0; poll < 32 && ended.Where() == Delivery::State::Pending; ++poll) {
      dead.FakeNowMs += 5000.0;
      ended = sources.Collect(gone, dead);
    }
    CHECK(ended.Where() != Delivery::State::Pending &&
              ended.Where() != Delivery::State::Delivered,
          "**THE BUDGET EXHAUSTS INTO THE REFUSAL PATH**: a host that never recovers walks "
          "all four retries and falls through -- the arm the first proof never reached "
          "(board:1696)");
    CHECK(dead.Begins == 1 + 4,
          "and exactly budget-many retries were begun -- one first ask plus four, never a "
          "hammer and never one lost");
  }

  Covers("I.23 a retry waits on the transport's clock: scheduled with a doubling backoff, "
         "never re-begun at poll cadence, proven against a faked clock without one sleep "
         "(board:1691, 1692)");
  return Report();
}
