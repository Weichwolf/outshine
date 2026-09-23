#include "OsmXmlReader.h"
#include "TransportTopology.h"
#include "Check.h"

#include <array>
#include <string>
#include <string_view>

namespace {

std::string Source(std::string_view nodeRefs, std::string_view tags) {
  return std::string("<osm version='0.6'>"
                     "<node id='1' lat='0' lon='0'/>"
                     "<node id='2' lat='0' lon='1'/>"
                     "<way id='7'>") +
         std::string(nodeRefs) + "<tag k='highway' v='primary'/>" + std::string(tags) +
         "</way></osm>";
}

}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  using namespace outshine::World;
  constexpr std::string_view refs = "<nd ref='1'/><nd ref='2'/>";
  const std::string validXml = Source(refs, "<tag k='oneway' v='yes'/>");
  const auto unnamed = OsmXmlReader::Read(validXml, {.DatasetId = "", .Revision = "r1"});
  CHECK(!unnamed && unnamed.error() == OsmXmlError::InvalidSourceIdentity,
        "an edge identity requires a nonempty source namespace");
  const auto unversioned = OsmXmlReader::Read(validXml, {.DatasetId = "osm", .Revision = ""});
  CHECK(!unversioned && unversioned.error() == OsmXmlError::InvalidSourceIdentity,
        "a graph snapshot requires an explicit source revision");
  const auto valid = OsmXmlReader::Read(validXml, {.DatasetId = "osm", .Revision = "r1"});
  CHECK(valid.has_value(), "the baseline road source parses");
  if (!valid) { return Report(); }

  struct Rejected {
    std::string Xml;
    TransportBuildErrorCode Code;
  };

  const std::array rejected{
      Rejected{Source("<nd ref='1'/><nd ref='9'/>", ""),
               TransportBuildErrorCode::MissingSourceObject},
      Rejected{Source(refs, "<tag k='oneway' v='sideways'/>"),
               TransportBuildErrorCode::InvalidOneway},
      Rejected{Source(refs, "<tag k='layer' v='1.5'/>"), TransportBuildErrorCode::InvalidLayer},
      Rejected{Source("<nd ref='1'/><nd ref='1'/>", ""),
               TransportBuildErrorCode::DegenerateSegment},
      Rejected{Source(refs, "<tag k='oneway' v='yes'/><tag k='oneway' v='no'/>"),
               TransportBuildErrorCode::AmbiguousTag},
  };
  for (const Rejected &caseInput : rejected) {
    const auto parsed = OsmXmlReader::Read(caseInput.Xml, {.DatasetId = "osm", .Revision = "r2"});
    CHECK(parsed.has_value(), "invalid transport semantics remain valid source XML");
    if (!parsed) { continue; }
    const auto built = TransportTopology::Build(*parsed);
    CHECK(!built && built.error().Code == caseInput.Code && built.error().SourceId != 0,
          "invalid source semantics reject the whole graph with source provenance");
  }
  const auto corrected = OsmXmlReader::Read(validXml, {.DatasetId = "osm", .Revision = "r3"});
  CHECK(corrected.has_value(), "the corrected revision parses");
  if (!corrected) { return Report(); }
  const auto recovered = TransportTopology::Build(*corrected);
  CHECK(recovered && recovered->Edges().size() == 1 && recovered->Edges()[0].Id.WayId == 7 &&
            recovered->SourceIdentity().Revision == "r3",
        "a corrected revision publishes a complete source-keyed graph");
  return Report();
}
