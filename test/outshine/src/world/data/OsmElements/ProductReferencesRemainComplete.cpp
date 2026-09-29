#include "OsmElements.h"
#include "OsmXmlReader.h"
#include "Check.h"

#include <array>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;

  const auto source =
      OsmXmlReader::Read("<osm version='0.6'>"
                         "<node id='1' lat='0' lon='0'/>"
                         "<node id='2' lat='0' lon='1'/>"
                         "<node id='3' lat='1' lon='1'/>"
                         "<way id='1'><nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='1'/>"
                         "<tag k='building' v='yes'/></way>"
                         "<way id='2'><nd ref='1'/><nd ref='99'/></way>"
                         "<relation id='1'><member type='way' ref='1' role='outer'/>"
                         "<member type='relation' ref='2' role='part'/></relation>"
                         "<relation id='2'><member type='relation' ref='1' role='part'/></relation>"
                         "<relation id='3'><member type='way' ref='2' role='inner'/></relation>"
                         "<relation id='4'><member type='relation' ref='3' role='part'/></relation>"
                         "<relation id='5'><member type='way' ref='900' role=''/>"
                         "<tag k='type' v='route'/><tag k='route' v='bus'/></relation>"
                         "</osm>",
                         {.DatasetId = "product-closure", .Revision = "r1"});
  CHECK(source.has_value(), "partial spatial source preserves all original elements");
  if (!source) { return Report(); }

  const std::array building{OsmElementId{.Kind = OsmElementKind::Way, .Id = 1}};
  CHECK(!source->FirstMissingReference(building),
        "closed building is independent of incomplete unrelated ways and routes");
  CHECK(source->FirstMissingReference().has_value(),
        "strict complete-source validation still rejects unrelated missing references");

  const std::array cycle{OsmElementId{.Kind = OsmElementKind::Relation, .Id = 1},
                         OsmElementId{.Kind = OsmElementKind::Relation, .Id = 2},
                         OsmElementId{.Kind = OsmElementKind::Way, .Id = 1}};
  CHECK(!source->FirstMissingReference(cycle),
        "shared dependencies and relation cycles terminate with typed IDs kept distinct");

  const std::array nested{OsmElementId{.Kind = OsmElementKind::Relation, .Id = 4}};
  const auto missing = source->FirstMissingReference(nested);
  CHECK(missing && missing->OwnerKind == OsmElementKind::Way && missing->OwnerId == 2 &&
            missing->MissingKind == OsmElementKind::Node && missing->MissingId == 99,
        "nested relation closure reports the actual broken way-to-node dependency");

  const std::array route{OsmElementId{.Kind = OsmElementKind::Relation, .Id = 5}};
  const auto missingWay = source->FirstMissingReference(route);
  CHECK(missingWay && missingWay->OwnerKind == OsmElementKind::Relation &&
            missingWay->OwnerId == 5 && missingWay->MissingKind == OsmElementKind::Way &&
            missingWay->MissingId == 900,
        "the same incomplete route fails when it is a consumed product");

  const std::array absent{OsmElementId{.Kind = OsmElementKind::Relation, .Id = 99}};
  const auto missingRoot = source->FirstMissingReference(absent);
  CHECK(missingRoot && missingRoot->OwnerId == 99 && missingRoot->MissingId == 99 &&
            missingRoot->MissingKind == OsmElementKind::Relation,
        "an absent requested root cannot pass closure validation");
  CHECK(!source->FirstMissingReference(std::span<const OsmElementId>{}),
        "empty product selection has no dependencies");
  CHECK(source->Relations().size() == 5 && source->FindRelation(5)->Tags.size() == 2,
        "product validation neither filters source elements nor strips their tags");
  return Report();
}
