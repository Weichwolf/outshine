#include "Check.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "TerrariumDem.h"
#include "VersatilesVector.h"

#include <memory>
#include <string>

using namespace outshine;
using namespace outshine::Data;

namespace {

class CountingTransport : public Transport {
public:
  [[nodiscard]] Ticket Begin(const std::string &url) override {
    Begun++;
    LastUrl = url;
    return Ticket{1};
  }
  [[nodiscard]] Wire Collect(Ticket ticket) override {
    (void)ticket;
    return Wire::Answered(200, std::vector<uint8_t>{1, 2, 3});
  }
  void Cancel(Ticket ticket) override { (void)ticket; }

  int Begun = 0;
  std::string LastUrl;
};

[[nodiscard]] ContentStore::Config StoreOff() {
  ContentStore::Config c;
  c.Using = ContentStore::Use::Off;
  return c;
}

}

int main() {
  Test::Covers("I.42 Covers is the only producer of the right to mint a world fact");
  Test::Covers("I.22 no provider here and no data here are different answers");

  ContentStore store(StoreOff());
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<TerrariumDem>()) == SourceSet::Registration::Accepted,
        "the elevation source registers");
  CHECK(sources.Add(std::make_unique<VersatilesVector>()) == SourceSet::Registration::Accepted,
        "the vector source registers");

  CountingTransport transport;

  {
    SourceSet::Query query = sources.Ask(Request(DataKind::VectorMap, Address::Tile(15, 17234, 10808)));
    CHECK(query.Undeclared(), "a z15 vector request is covered by no registered source");
    Delivery answer = sources.Collect(query, transport);
    CHECK(answer.Where() == Delivery::State::Undeclared,
          "an uncovered request answers Undeclared, never Vacant");
    CHECK(transport.Begun == 0, "an uncovered request touched no transport");
  }

  {
    SourceSet::Query query = sources.Ask(Request(DataKind::Elevation, Address::Tile(15, 17245, 10804)));
    CHECK(!query.Undeclared(), "a z15 elevation request is covered");
    Delivery answer = sources.Collect(query, transport);
    Delivery::Answer taken;
    CHECK(answer.TryTake(&taken), "the covering source delivered");
    CHECK(taken.SourceId == "terrarium.s3", "the elevation source answered, not the vector one");
    CHECK(transport.Begun == 1, "exactly one upstream was reached");
    CHECK(transport.LastUrl.find("elevation-tiles-prod") != std::string::npos,
          "and it was the elevation upstream's own URL");
  }

  {
    SourceSet::Query query = sources.Ask(Request(DataKind::VectorMap, Address::Tile(14, 99999, 5404)));
    CHECK(query.Undeclared(), "an address outside the pyramid is covered by nobody");
  }

  {
    SourceSet::Query query = sources.Ask(Request(DataKind::Elevation, Address::Whole(0)));
    CHECK(query.Undeclared(), "a whole-world address is outside every tile source");
  }

  Test::Note("transports begun over four requests, one of which was covered",
             (double)transport.Begun, "fetch");
  return Test::Report();
}
