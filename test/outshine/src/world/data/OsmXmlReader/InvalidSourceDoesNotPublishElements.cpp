#include "OsmXmlReader.h"
#include "Check.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;

  const auto ReadSource = [](std::string_view xml) {
    return OsmXmlReader::Read(xml, {.DatasetId = "osm-test", .Revision = "r1"});
  };

  struct Rejected {
    std::string_view Xml;
    OsmXmlError Error;
  };

  constexpr std::array rejected{
      Rejected{"<osm version='0.6'><node id='1' lat='0' lon='0'>", OsmXmlError::InvalidDocument},
      Rejected{"<osm version='0.5' />", OsmXmlError::UnsupportedRoot},
      Rejected{"<osm version='0.6'><node id='-1' lat='0' lon='0'/></osm>", OsmXmlError::InvalidId},
      Rejected{"<osm version='0.6'><node id='1x' lat='0' lon='0'/></osm>", OsmXmlError::InvalidId},
      Rejected{"<osm version='0.6'><node id='1' lat='nan' lon='0'/></osm>",
               OsmXmlError::InvalidCoordinate},
      Rejected{"<osm version='0.6'><node id='1' lat='91' lon='0'/></osm>",
               OsmXmlError::InvalidCoordinate},
      Rejected{"<osm version='0.6'><node id='1' lat='0' lon='181'/></osm>",
               OsmXmlError::InvalidCoordinate},
      Rejected{"<osm version='0.6'><node id='1' lat='0' lon='0'/>"
               "<node id='1' lat='0' lon='0'/></osm>",
               OsmXmlError::DuplicateElement},
      Rejected{"<osm version='0.6'><way id='1'/><way id='1'/></osm>",
               OsmXmlError::DuplicateElement},
      Rejected{"<osm version='0.6'><node id='1' lat='0' lon='0'>"
               "<tag k='highway'/></node></osm>",
               OsmXmlError::InvalidTag},
      Rejected{"<osm version='0.6'><relation id='1'>"
               "<member type='building' ref='2' role=''/></relation></osm>",
               OsmXmlError::InvalidMember},
  };
  for (const Rejected &input : rejected) {
    const auto result = ReadSource(input.Xml);
    CHECK(!result && result.error() == input.Error,
          "invalid source element returns a typed rejection without a partial document");
  }

  const auto missingNode = ReadSource("<osm version='0.6'><way id='7'><nd ref='9'/></way></osm>");
  CHECK(missingNode && missingNode->FirstMissingReference() &&
            missingNode->FirstMissingReference()->OwnerKind == OsmElementKind::Way &&
            missingNode->FirstMissingReference()->MissingKind == OsmElementKind::Node &&
            missingNode->FirstMissingReference()->MissingId == 9,
        "a partial way remains parseable but exposes its unresolved node reference");
  const auto missingWay = ReadSource("<osm version='0.6'><relation id='8'>"
                                     "<member type='way' ref='9' role='main'/></relation></osm>");
  CHECK(missingWay && missingWay->FirstMissingReference() &&
            missingWay->FirstMissingReference()->OwnerKind == OsmElementKind::Relation &&
            missingWay->FirstMissingReference()->MissingKind == OsmElementKind::Way &&
            missingWay->FirstMissingReference()->MissingId == 9,
        "a partial relation retains role and reports its unresolved way ID");
  const auto corrected = ReadSource("<osm version='0.6'><node id='9' lat='0' lon='0'/>"
                                    "<way id='7'><nd ref='9'/></way></osm>");
  CHECK(corrected && !corrected->FirstMissingReference() && corrected->FindNode(9) != nullptr &&
            corrected->FindWay(7) != nullptr,
        "a corrected source parses and closes after prior rejection");
  const auto reversed = ReadSource("<osm version='0.6'>"
                                   "<node id='1' lat='0' lon='0'/>"
                                   "<node id='2' lat='0' lon='1'/>"
                                   "<way id='7'><nd ref='2'/><nd ref='1'/></way></osm>");
  CHECK(reversed && !reversed->FirstMissingReference() && reversed->FindWay(7) != nullptr &&
            reversed->FindWay(7)->NodeIds == std::vector<uint64_t>({2, 1}),
        "way node order is source data, not inferred or corrected by the XML reader");
  return Report();
}
