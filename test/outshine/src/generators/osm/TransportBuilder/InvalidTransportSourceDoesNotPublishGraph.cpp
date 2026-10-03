#include "OsmTransportBuilder.h"
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
  const auto unnamed =
      outshine::Generators::Osm::XmlReader::Read(validXml, {.DatasetId = "", .Revision = "r1"});
  CHECK(!unnamed && unnamed.error() == outshine::Generators::Osm::XmlError::InvalidSourceIdentity,
        "an edge identity requires a nonempty source namespace");
  const auto unversioned =
      outshine::Generators::Osm::XmlReader::Read(validXml, {.DatasetId = "osm", .Revision = ""});
  CHECK(!unversioned &&
            unversioned.error() == outshine::Generators::Osm::XmlError::InvalidSourceIdentity,
        "a graph snapshot requires an explicit source revision");
  const auto valid =
      outshine::Generators::Osm::XmlReader::Read(validXml, {.DatasetId = "osm", .Revision = "r1"});
  CHECK(valid.has_value(), "the baseline road source parses");
  if (!valid) { return Report(); }

  struct Rejected {
    std::string Xml;
    outshine::Generators::Osm::TransportBuildErrorCode Code;
  };

  const std::array rejected{
      Rejected{Source("<nd ref='1'/><nd ref='9'/>", ""),
               outshine::Generators::Osm::TransportBuildErrorCode::MissingSourceObject},
      Rejected{Source(refs, "<tag k='oneway' v='sideways'/>"),
               outshine::Generators::Osm::TransportBuildErrorCode::InvalidOneway},
      Rejected{Source(refs, "<tag k='layer' v='1.5'/>"),
               outshine::Generators::Osm::TransportBuildErrorCode::InvalidLayer},
      Rejected{Source(refs, "<tag k='width' v='12 yards'/>"),
               outshine::Generators::Osm::TransportBuildErrorCode::InvalidWidth},
      Rejected{Source(refs, "<tag k='lanes' v='1;2'/>"),
               outshine::Generators::Osm::TransportBuildErrorCode::InvalidLaneCount},
      Rejected{Source("<nd ref='1'/><nd ref='1'/>", ""),
               outshine::Generators::Osm::TransportBuildErrorCode::DegenerateSegment},
      Rejected{Source(refs, "<tag k='oneway' v='yes'/><tag k='oneway' v='no'/>"),
               outshine::Generators::Osm::TransportBuildErrorCode::AmbiguousTag},
  };
  for (const Rejected &caseInput : rejected) {
    const auto parsed = outshine::Generators::Osm::XmlReader::Read(
        caseInput.Xml, {.DatasetId = "osm", .Revision = "r2"});
    CHECK(parsed.has_value(), "invalid transport semantics remain valid source XML");
    if (!parsed) { continue; }
    const auto built = outshine::Generators::Osm::TransportBuilder::Build(*parsed);
    CHECK(!built && built.error().Code == caseInput.Code && built.error().SourceId != 0,
          "invalid source semantics reject the whole graph with source provenance");
  }
  const auto corrected =
      outshine::Generators::Osm::XmlReader::Read(validXml, {.DatasetId = "osm", .Revision = "r3"});
  CHECK(corrected.has_value(), "the corrected revision parses");
  if (!corrected) { return Report(); }
  const auto recovered = outshine::Generators::Osm::TransportBuilder::Build(*corrected);
  CHECK(recovered && recovered->Edges().size() == 1 && recovered->Edges()[0].Id.PathId == 7 &&
            recovered->SourceIdentity().Revision == "r3",
        "a corrected revision publishes a complete source-keyed graph");
  return Report();
}
