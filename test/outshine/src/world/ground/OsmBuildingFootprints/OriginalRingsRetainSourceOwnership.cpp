#include "OsmBuildingFootprints.h"
#include "OsmXmlReader.h"
#include "Check.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::shared_ptr<const outshine::Data::OsmSourceSnapshot> Source(std::string_view body) {
  auto read =
      outshine::Data::OsmXmlReader::Read("<osm version='0.6'>" + std::string(body) + "</osm>",
                                         {.DatasetId = "original-buildings", .Revision = "r1"});
  if (!read) { return {}; }
  return std::make_shared<const outshine::Data::OsmSourceSnapshot>(
      outshine::Data::OsmSourceSnapshot{.Elements = std::move(*read)});
}

constexpr std::string_view kNodes = "<node id='1' lat='0' lon='0'/><node id='2' lat='0' lon='4'/>"
                                    "<node id='3' lat='4' lon='4'/><node id='4' lat='4' lon='0'/>"
                                    "<node id='5' lat='1' lon='1'/><node id='6' lat='1' lon='2'/>"
                                    "<node id='7' lat='2' lon='2'/><node id='8' lat='2' lon='1'/>";

}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Data;
  using namespace outshine::Test;
  const std::string polygon =
      std::string(kNodes) +
      "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/>"
      "<tag k='building' v='yes'/></way>"
      "<way id='11'><nd ref='1'/><nd ref='4'/><nd ref='3'/></way>"
      "<way id='12'><nd ref='5'/><nd ref='6'/><nd ref='7'/><nd ref='8'/><nd ref='5'/></way>"
      "<relation id='20'><member type='way' ref='11' role='outer'/>"
      "<member type='way' ref='12' role='inner'/><member type='way' ref='10' role='outer'/>"
      "<tag k='type' v='multipolygon'/><tag k='building' v='church'/>"
      "<tag k='roof:shape' v='half-hipped'/><tag k='custom:original' v='retained'/></relation>"
      "<relation id='30'><member type='way' ref='999' role=''/>"
      "<tag k='type' v='route'/><tag k='type' v='unknown-route'/></relation>";
  auto source = Source(polygon);
  auto built = OsmBuildingFootprints::Build(source, 8);
  CHECK(built.has_value(), "complete building survives an unrelated incomplete route");
  if (!built) { return Report(); }
  CHECK(built->Buildings().size() == 1 && built->Rings().size() == 2 &&
            built->Points().size() == 16,
        "joined outer ways and one courtyard produce one building without duplicate ways");
  CHECK(built->Rings()[0].Exterior && !built->Rings()[1].Exterior && built->Rings()[0].Count == 4 &&
            built->Rings()[1].Count == 4,
        "ring roles and open coordinate spans survive reversed member directions");
  CHECK(built->Points()[0] == 0 && built->Points()[1] == 0 && built->Points()[6] == 4 &&
            built->Points()[7] == 0,
        "source node joins use identity and preserve double latitude/longitude order");
  const OsmElementId expectedId{.Kind = OsmElementKind::Relation, .Id = 20};
  CHECK(built->Buildings()[0].Source == expectedId,
        "native building retains its typed original relation ID");
  const auto *original = source.get();
  source.reset();
  CHECK(&built->Source() == original && built->Tags(built->Buildings()[0]).size() == 4 &&
            std::ranges::any_of(built->Tags(built->Buildings()[0]),
                                [](const auto &tag) {
                                  return tag.Key == "custom:original" && tag.Value == "retained";
                                }),
        "the geometry owner pins all original tags beyond caller source lifetime");
  const auto limited = OsmBuildingFootprints::Build(Source(polygon), 7);
  CHECK(!limited && limited.error().Code == OsmFootprintErrorCode::PointBudgetExceeded,
        "one point below the required budget rejects the whole candidate");

  const auto chimney = OsmBuildingFootprints::Build(
      Source(std::string(kNodes) +
             "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='1'/>"
             "<tag k='building' v='yes'/><tag k='man_made' v='chimney'/>"
             "<tag k='height' v='80'/></way>"),
      3);
  CHECK(chimney && chimney->Buildings().size() == 1 &&
            std::ranges::any_of(
                chimney->Tags(chimney->Buildings()[0]),
                [](const auto &tag) { return tag.Key == "man_made" && tag.Value == "chimney"; }),
        "chimney classification remains original data instead of a height-derived house");
  const auto missing = OsmBuildingFootprints::Build(
      Source("<way id='10'><nd ref='99'/><tag k='building' v='yes'/></way>"), 8);
  CHECK(!missing && missing.error().Code == OsmFootprintErrorCode::MissingReference &&
            missing.error().Missing && missing.error().Missing->MissingId == 99,
        "missing required nodes carry the exact dependency instead of dropping the building");
  const auto open = OsmBuildingFootprints::Build(
      Source(std::string(kNodes) + "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/>"
                                   "<tag k='building' v='yes'/></way>"),
      8);
  CHECK(!open && open.error().Code == OsmFootprintErrorCode::InvalidRing,
        "open standalone ways are not silently closed with invented geometry");
  const auto ambiguous = OsmBuildingFootprints::Build(
      Source(std::string(kNodes) +
             "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='1'/>"
             "<tag k='building' v='yes'/><tag k='building' v='no'/></way>"),
      8);
  CHECK(!ambiguous && ambiguous.error().Code == OsmFootprintErrorCode::AmbiguousTag,
        "contradictory classification cannot disappear behind a first-tag choice");
  const auto duplicated = OsmBuildingFootprints::Build(
      Source(std::string(kNodes) +
             "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='1'/></way>"
             "<relation id='20'><member type='way' ref='10' role='outer'/>"
             "<member type='way' ref='10' role='inner'/>"
             "<tag k='type' v='multipolygon'/><tag k='building' v='yes'/></relation>"),
      8);
  CHECK(!duplicated && duplicated.error().Code == OsmFootprintErrorCode::AmbiguousJunction,
        "one source way cannot be both shell and courtyard");
  const auto openRelation = OsmBuildingFootprints::Build(
      Source(std::string(kNodes) +
             "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/></way>"
             "<relation id='20'><member type='way' ref='10' role='outer'/>"
             "<tag k='type' v='multipolygon'/><tag k='building' v='yes'/></relation>"),
      8);
  CHECK(!openRelation && openRelation.error().Code == OsmFootprintErrorCode::AmbiguousJunction,
        "an open member chain cannot invent its missing closing edge");
  return Report();
}
