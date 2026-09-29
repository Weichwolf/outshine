#include "TransportTopology.h"
#include "OsmXmlReader.h"
#include "Check.h"

#include <string>

int main() {
  using namespace outshine::World;
  using namespace outshine::Data;
  using namespace outshine::Test;
  const std::string nodes = "<node id='1' lat='0' lon='0'/><node id='2' lat='0' lon='0.001'/>";
  const std::string way = "<way id='10'><nd ref='1'/><nd ref='2'/>"
                          "<tag k='highway' v='residential'/></way>";
  const std::string unrelated = "<relation id='20'><member type='way' ref='99' role=''/>"
                                "<tag k='type' v='route'/></relation>"
                                "<way id='30'><nd ref='98'/><tag k='building' v='yes'/></way>";
  const auto source = OsmXmlReader::Read("<osm version='0.6'>" + nodes + way + unrelated + "</osm>",
                                         {.DatasetId = "spatial", .Revision = "r1"});
  CHECK(source.has_value(), "spatial source retains open references");
  if (!source) { return Report(); }
  const auto region = TransportTopology::BuildRegion(*source);
  CHECK(region && region->Edges().size() == 2 && region->FindNode(1) && region->FindNode(2),
        "complete transport way survives unrelated open route and building references");
  CHECK(!TransportTopology::Build(*source), "strict whole-source graph contract remains strict");
  const auto missing =
      OsmXmlReader::Read("<osm version='0.6'><node id='1' lat='0' lon='0'/>" + way + "</osm>",
                         {.DatasetId = "spatial", .Revision = "r2"});
  CHECK(missing.has_value(), "incomplete transport source parses without discarding references");
  if (missing) {
    const auto rejected = TransportTopology::BuildRegion(*missing);
    CHECK(!rejected && rejected.error().Code == TransportBuildErrorCode::MissingSourceObject &&
              rejected.error().SourceId == 10,
          "a missing consumed node still rejects the complete candidate graph");
  }
  if (region) {
    CHECK(!region->ResolveCircuit(*source, 20),
          "requesting the incomplete route remains a failure rather than a truncated route");
  }
  return Report();
}
