/* A DELIVERY CARRIES WHICH SOURCE ANSWERED AND AT WHICH ADDRESS, never only bytes. A request above
 * a source's last native zoom is served from an ancestor, and *which* ancestor is what the decoder
 * downstream computes its crop from — a crop taken from the requested address over an ancestor's
 * pixels is a wrong picture drawn silently. What resolution actually answered was an assumption
 * until this was a value.
 *
 * The two upstreams differ here and the difference is declared, not assumed: a raster can be cropped
 * from its parent and a vector tile cannot, because clipping geometry is not resampling it. */
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

class OneAnswerTransport : public Transport {
public:
  [[nodiscard]] Ticket Begin(const std::string &url) override {
    Urls.push_back(url);
    return Ticket{1};
  }
  [[nodiscard]] Wire Collect(Ticket ticket) override {
    (void)ticket;
    return Wire::Answered(200, std::vector<uint8_t>{1, 2, 3, 4});
  }
  void Cancel(Ticket ticket) override { (void)ticket; }

  std::vector<std::string> Urls;
};

[[nodiscard]] ContentStore::Config StoreOff() {
  ContentStore::Config c;
  c.Using = ContentStore::Use::Off;
  return c;
}

} // namespace

int main() {
  Test::Covers("I.22 a Delivery carries which source answered and at which address");
  Test::Covers("I.22 the zoom bound is the source's declaration about itself and exists once");

  const TerrariumDem dem;
  const VersatilesVector vector;

  CHECK(dem.Declaration().MaxZoom == 15, "the elevation source declares its own last native zoom");
  CHECK(vector.Declaration().MaxZoom == 14, "and so does the vector source, to a different value");

  /* AT ITS OWN ZOOM the answer is the question. */
  {
    const Request asked(DataKind::Elevation, Address::Tile(14, 8620, 5403));
    CHECK(dem.Covers(asked) == Coverage::Inside, "a z14 elevation tile is inside");
    CHECK(dem.Serves(asked) == asked.Where(), "and is served from itself");
  }

  /* ABOVE IT the ancestor answers, and the arithmetic is the one the crop inverts: two zooms up is
   * a shift of two, so 4x4 sub-tiles of one parent. */
  {
    const Request asked(DataKind::Elevation, Address::Tile(17, 68960, 43224));
    CHECK(dem.Covers(asked) == Coverage::Inside, "a z17 elevation tile is still inside its domain");
    const Address at = dem.Serves(asked);
    CHECK(at == Address::Tile(15, 17240, 10806), "and is served from its z15 ancestor");
    CHECK(at != asked.Where(), "which is not the address that was asked for");
    CHECK(at.Text() == "15/17240/10806", "and the canonical text names it");
  }

  /* A VECTOR TILE CANNOT BE CROPPED FROM ITS PARENT, so above its last zoom it is Outside instead —
   * which is what lets the selector hand the request to another source rather than guess. */
  {
    const Request asked(DataKind::VectorMap, Address::Tile(15, 17240, 10806));
    CHECK(vector.Covers(asked) == Coverage::Outside,
          "a z15 vector tile is outside the vector source's domain");
  }

  /* AND THE REGISTRY REPORTS THE ANCESTOR'S ADDRESS, not the request's. */
  {
    ContentStore store(StoreOff());
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<TerrariumDem>()) == SourceSet::Registration::Accepted,
          "the elevation source registers");
    OneAnswerTransport transport;
    SourceSet::Query query = sources.Ask(Request(DataKind::Elevation, Address::Tile(17, 68960, 43224)));
    Delivery answer = sources.Collect(query, transport);
    Delivery::Answer taken;
    CHECK(answer.TryTake(&taken), "the ancestor delivered");
    CHECK(taken.At == Address::Tile(15, 17240, 10806), "and the delivery names the address it came from");
    CHECK(taken.SourceId == "terrarium.s3", "and the source that answered");
    CHECK(transport.Urls.size() == 1 && transport.Urls[0].find("/15/17240/10806.png") != std::string::npos,
          "and the URL that was fetched is the ancestor's, not the request's");
  }

  return Test::Report();
}
