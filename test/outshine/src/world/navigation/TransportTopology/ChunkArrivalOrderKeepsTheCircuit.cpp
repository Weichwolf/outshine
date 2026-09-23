#include "OsmXmlReader.h"
#include "TransportTopology.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <string_view>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  using namespace outshine::World;
  constexpr std::string_view firstChunk =
      "<osm version='0.6'>"
      "<node id='1' lat='0' lon='0'/><node id='2' lat='0' lon='1'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/><tag k='highway' v='raceway'/>"
      "<tag k='oneway' v='yes'/></way>"
      "<relation id='9'><member type='way' ref='12' role=''/>"
      "<member type='way' ref='10' role=''/><member type='way' ref='13' role='pitlane'/>"
      "<member type='way' ref='11' role=''/><tag k='type' v='circuit'/></relation></osm>";
  constexpr std::string_view secondChunk =
      "<osm version='0.6'>"
      "<node id='2' lat='0' lon='1'/><node id='3' lat='1' lon='1'/>"
      "<node id='4' lat='0.5' lon='1.5'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/><tag k='oneway' v='yes'/>"
      "<tag k='highway' v='raceway'/></way>"
      "<way id='11'><nd ref='2'/><nd ref='3'/><tag k='highway' v='raceway'/>"
      "<tag k='oneway' v='yes'/></way>"
      "<way id='12'><nd ref='3'/><nd ref='1'/><tag k='highway' v='raceway'/>"
      "<tag k='oneway' v='yes'/></way>"
      "<way id='13'><nd ref='2'/><nd ref='4'/><nd ref='3'/>"
      "<tag k='highway' v='raceway'/><tag k='oneway' v='yes'/></way></osm>";
  const auto first = OsmXmlReader::Read(firstChunk, {.DatasetId = "osm", .Revision = "r1"});
  const auto second = OsmXmlReader::Read(secondChunk, {.DatasetId = "osm", .Revision = "r1"});
  CHECK(first && second && first->FirstMissingReference() && second->FirstMissingReference(),
        "source chunks are individually incomplete but syntactically valid");
  if (!first || !second) { return Report(); }
  const std::array forward{*first, *second};
  const std::array reverse{*second, *first};
  const auto mergedForward = OsmElements::Merge(forward, 11);
  const auto mergedReverse = OsmElements::Merge(reverse, 11);
  CHECK(mergedForward && mergedReverse && !mergedForward->FirstMissingReference() &&
            !mergedReverse->FirstMissingReference() && mergedForward->Nodes().size() == 4 &&
            mergedForward->Ways().size() == 4,
        "identical repeated IDs deduplicate and all cross-chunk references close");
  if (!mergedForward || !mergedReverse) { return Report(); }
  const auto graphForward = TransportTopology::Build(*mergedForward);
  const auto graphReverse = TransportTopology::Build(*mergedReverse);
  CHECK(graphForward && graphReverse, "both chunk orders build a complete graph");
  if (!graphForward || !graphReverse) { return Report(); }
  const auto routeForward = graphForward->ResolveCircuit(*mergedForward, 9);
  const auto routeReverse = graphReverse->ResolveCircuit(*mergedReverse, 9);
  bool sameEdges = graphForward->Edges().size() == graphReverse->Edges().size();
  if (sameEdges) {
    for (size_t at = 0; at < graphForward->Edges().size(); ++at) {
      const TransportEdge &left = graphForward->Edges()[at];
      const TransportEdge &right = graphReverse->Edges()[at];
      sameEdges &= left.Id == right.Id && left.FromNodeId == right.FromNodeId &&
                   left.ToNodeId == right.ToNodeId && left.Modes == right.Modes;
    }
  }
  CHECK(routeForward && routeReverse && sameEdges &&
            routeForward->SourceIdentity == routeReverse->SourceIdentity &&
            routeForward->EdgeIds == routeReverse->EdgeIds && routeForward->EdgeIds.size() == 3,
        "reversing chunk arrival preserves sorted edge IDs and the topological circuit");

  const auto smallBudget = OsmElements::Merge(forward, 10);
  CHECK(!smallBudget && smallBudget.error().Code == OsmMergeErrorCode::BudgetExceeded,
        "input element work is bounded before merge allocation");
  const auto newerRevision =
      OsmXmlReader::Read(secondChunk, {.DatasetId = "osm", .Revision = "r2"});
  CHECK(newerRevision.has_value(), "the next source revision parses independently");
  if (newerRevision) {
    const std::array mixedRevisions{*first, *newerRevision};
    const auto refused = OsmElements::Merge(mixedRevisions, 11);
    CHECK(!refused && refused.error().Code == OsmMergeErrorCode::IdentityMismatch,
          "chunks from distinct source revisions cannot form one graph snapshot");
  }
  constexpr std::string_view changedNode =
      "<osm version='0.6'><node id='2' lat='1' lon='1'/></osm>";
  const auto changed = OsmXmlReader::Read(changedNode, {.DatasetId = "osm", .Revision = "r1"});
  CHECK(changed.has_value(), "a conflicting source chunk parses");
  if (changed) {
    const std::array conflicting{*first, *changed};
    const auto refused = OsmElements::Merge(conflicting, 5);
    CHECK(!refused && refused.error().Code == OsmMergeErrorCode::ConflictingElement &&
              refused.error().Kind == OsmElementKind::Node && refused.error().Id == 2,
          "conflicting repeated source IDs reject the candidate with exact provenance");
  }
  return Report();
}
