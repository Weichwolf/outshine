#include "TransportProvenance.h"
#include "TransportNetworkFixture.h"
#include "TransportProvenanceFixture.h"
#include "Check.h"
#include <cstdint>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::World;
  using namespace outshine::Test;
  const auto network = TransportNetworkFixture({}, {});
  const auto &graph = network->Topology();
  const auto expected = TransportProvenanceFixture();
  CHECK(ValidateTransportProvenance(graph, expected),
        "multiple source fragments, missing provider IDs and changed supplied values are valid");
  CHECK(ValidateTransportProvenance(graph, {}),
        "the original element identity contract remains an explicit compatible input");
  auto altered = expected;
  altered.Sources.clear();
  CHECK(!ValidateTransportProvenance(graph, altered),
        "derived identities cannot publish without their bound source rows");
  altered = expected;
  altered.References[0].Source = 2;
  CHECK(!ValidateTransportProvenance(graph, altered), "an invalid source row is refused");
  altered = expected;
  ++altered.Paths[0].PathId;
  CHECK(!ValidateTransportProvenance(graph, altered), "the topology path must own its references");
  altered = expected;
  altered.Paths[0].ReferenceCount = 1;
  CHECK(!ValidateTransportProvenance(graph, altered), "unowned source references are refused");
  altered = expected;
  altered.Paths[0].FirstReference = 1;
  CHECK(!ValidateTransportProvenance(graph, altered), "source spans must partition owned storage");
  altered = expected;
  altered.Paths[0].DerivedFields = uint16_t{1} << 15;
  CHECK(!ValidateTransportProvenance(graph, altered), "unknown functional fields are refused");
  altered = expected;
  std::swap(altered.Sources[0], altered.Sources[1]);
  CHECK(!ValidateTransportProvenance(graph, altered), "source rows have canonical unique order");
  altered = expected;
  altered.Sources[1] = altered.Sources[0];
  CHECK(!ValidateTransportProvenance(graph, altered), "duplicate source rows must be deduplicated");
  altered = expected;
  altered.IdentitySpace = static_cast<TransportIdentitySpace>(2);
  CHECK(!ValidateTransportProvenance(graph, altered), "unknown identity models are refused");
  altered = expected;
  altered.Sources[0].Item.clear();
  CHECK(!ValidateTransportProvenance(graph, altered), "a source row requires its opaque item key");
  return Report();
}
