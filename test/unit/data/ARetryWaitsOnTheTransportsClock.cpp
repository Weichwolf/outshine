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

  Covers("I.23 a retry waits on the transport's clock: scheduled with a doubling backoff, "
         "never re-begun at poll cadence, proven against a faked clock without one sleep "
         "(board:1691, 1692)");
  return Report();
}
