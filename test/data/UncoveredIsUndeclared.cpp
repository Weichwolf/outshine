/* THE COVERAGE PREDICATE IS THE ONLY THING THAT CAN MINT THE RIGHT TO A WORLD FACT, and it is
 * answered before any wire is touched. Before the registry existed this was not testable: the one
 * hard-coded upstream was asked for everything and its answer was read as a statement about the
 * Earth.
 *
 * Two claims over the DECLARED sources, and the second is the one that used to be unspellable:
 *   - a request no source covers answers Undeclared, which is a DECLARATION error and not a place
 *   - a request covered by one source and not another reaches only that source's own upstream
 *
 * An Outside source standing beside an Inside one is the neighbouring claim and it is held once, in
 * AbsenceHandsOver, over scripted sources whose coverage the test dictates. */
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

/* Every ticket handed out is counted, so how often the wire was reached is a number the test reads
 * rather than a thing it hopes for: zero for the uncovered requests, exactly one for the covered. */
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

/* A store that is present and off: the seam is exercised, the disk is not. */
[[nodiscard]] ContentStore::Config StoreOff() {
  ContentStore::Config c;
  c.Using = ContentStore::Use::Off;
  return c;
}

} // namespace

int main() {
  Test::Covers("I.22 Covers is the only producer of the right to mint a world fact");
  Test::Covers("I.22 no provider here and no data here are different answers");

  ContentStore store(StoreOff());
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<TerrariumDem>()) == SourceSet::Registration::Accepted,
        "the elevation source registers");
  CHECK(sources.Add(std::make_unique<VersatilesVector>()) == SourceSet::Registration::Accepted,
        "the vector source registers");

  CountingTransport transport;

  /* z15 VECTOR. Versatiles' last zoom is 14 and a vector tile cannot be cropped from its parent, so
   * it declares this Outside; the elevation source declares it Outside because the kind is wrong.
   * Nothing covers it, and that is a statement about the registry and not about the place. */
  {
    SourceSet::Query query = sources.Ask(Request(DataKind::VectorMap, Address::Tile(15, 17234, 10808)));
    CHECK(query.Undeclared(), "a z15 vector request is covered by no registered source");
    Delivery answer = sources.Collect(query, transport);
    CHECK(answer.Where() == Delivery::State::Undeclared,
          "an uncovered request answers Undeclared, never Vacant");
    CHECK(transport.Begun == 0, "an uncovered request touched no transport");
  }

  /* z15 ELEVATION. Terrarium's last native zoom is 15 too, so this one IS covered — and the vector
   * source must not have been asked, because the kind filter comes before the wire. */
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

  /* AN ADDRESS THAT IS NOT IN THE PYRAMID. x = 2^z is outside every slippy source by construction,
   * which is what stops the one measured upstream 500 (a malformed versatiles request) from ever
   * being asked for. */
  {
    SourceSet::Query query = sources.Ask(Request(DataKind::VectorMap, Address::Tile(14, 99999, 5404)));
    CHECK(query.Undeclared(), "an address outside the pyramid is covered by nobody");
  }

  /* THE WRONG SCHEME. A whole-world address is not a tile address, and a tile source must say so
   * rather than read three numbers that have no meaning under it. */
  {
    SourceSet::Query query = sources.Ask(Request(DataKind::Elevation, Address::Whole(0)));
    CHECK(query.Undeclared(), "a whole-world address is outside every tile source");
  }

  Test::Note("transports begun over four requests, one of which was covered",
             (double)transport.Begun, "fetch");
  return Test::Report();
}
