#include "OsmXmlReader.h"
#include "TransportTopology.h"
#include "Check.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string ReverseWayNodes(std::string xml, uint64_t wayId) {
  const size_t wayAt = xml.find("<way id=\"" + std::to_string(wayId) + "\">");
  if (wayAt == std::string::npos) { return {}; }
  const size_t firstNode = xml.find("    <nd ", wayAt);
  const size_t firstTag = xml.find("    <tag ", firstNode);
  if (firstNode == std::string::npos || firstTag == std::string::npos) { return {}; }
  const std::string_view block(xml.data() + firstNode, firstTag - firstNode);
  std::vector<std::string_view> lines;
  for (size_t at = 0; at < block.size();) {
    const size_t end = block.find('\n', at);
    if (end == std::string_view::npos) { return {}; }
    lines.push_back(block.substr(at, end - at + 1));
    at = end + 1;
  }
  std::string reversed;
  for (auto line = lines.rbegin(); line != lines.rend(); ++line) { reversed += *line; }
  xml.replace(firstNode, firstTag - firstNode, reversed);
  return xml;
}

}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  using namespace outshine::World;
  std::ifstream input("src/assets/world/osm/HockenheimringGrandPrix.osm", std::ios::binary);
  CHECK(input.good(), "the pinned Hockenheim OSM source is available");
  if (!input) { return Report(); }
  const std::string xml(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  const auto source = OsmXmlReader::Read(xml, {.DatasetId = "openstreetmap", .Revision = "pin-r1"});
  CHECK(source.has_value(), "the pinned Hockenheim source parses");
  if (!source) { return Report(); }
  const auto graph = TransportTopology::Build(*source);
  CHECK(graph.has_value(), "the complete source builds a native logical transport graph");
  if (!graph) { return Report(); }
  const auto route = graph->ResolveCircuit(*source, 284588);
  const auto undersizedBudget = graph->ResolveCircuit(*source, 284588, {}, 266);
  CHECK(!undersizedBudget && undersizedBudget.error().Code == CircuitErrorCode::TooManyEdges &&
            undersizedBudget.error().SourceId == 284588,
        "route edge budget rejects before selecting an overlong circuit");
  CHECK(route && route->EdgeIds.size() == 267 && graph->UnclassifiedWayCount() == 0,
        "the main Grand Prix relation resolves to 267 directed raceway edges");
  if (!route) { return Report(); }
  const OsmRelation *relation = source->FindRelation(284588);
  CHECK(relation != nullptr, "the route retains its original relation ID");
  if (relation == nullptr) { return Report(); }
  uint64_t pitWayId = 0;
  for (const OsmRelationMember &member : relation->Members) {
    if (member.Role == "pitlane") { pitWayId = member.Id; }
  }
  CHECK(pitWayId != 0, "the alternate pit way is identified by its relation role");
  uint64_t node = route->StartNodeId;
  bool continuous = true;
  for (const TransportEdgeId id : route->EdgeIds) {
    const TransportEdge *edge = graph->FindEdge(id);
    continuous &= edge != nullptr && edge->FromNodeId == node &&
                  edge->Allows(TransportMode::Motor) && edge->Id.WayId != pitWayId;
    if (edge == nullptr) { break; }
    node = edge->ToNodeId;
  }
  CHECK(continuous && node == route->StartNodeId,
        "the ID-based lap closes without a gap, jump or pitlane substitution");
  uint64_t mainWayId = 0;
  for (const OsmRelationMember &member : relation->Members) {
    if (member.Role.empty()) {
      mainWayId = member.Id;
      break;
    }
  }
  const std::string reversedWay = ReverseWayNodes(xml, mainWayId);
  CHECK(!reversedWay.empty(), "the pinned main way can be reversed independently");
  if (!reversedWay.empty()) {
    const auto changedSource =
        OsmXmlReader::Read(reversedWay, {.DatasetId = "openstreetmap", .Revision = "reversed-way"});
    CHECK(changedSource.has_value(), "the reversed way remains valid OSM source");
    if (changedSource) {
      const auto changedGraph = TransportTopology::Build(*changedSource);
      CHECK(changedGraph && !changedGraph->ResolveCircuit(*changedSource, 284588),
            "reversing one source way breaks the directed circuit rather than teleporting");
    }
  }
  std::string pitAsMain = xml;
  const size_t pitRole = pitAsMain.find("role=\"pitlane\"");
  CHECK(pitRole != std::string::npos, "the pinned pit role is present");
  if (pitRole != std::string::npos) {
    pitAsMain.replace(pitRole, sizeof("role=\"pitlane\"") - 1, "role=\"\"");
    const auto changedSource =
        OsmXmlReader::Read(pitAsMain, {.DatasetId = "openstreetmap", .Revision = "pit-as-main"});
    CHECK(changedSource.has_value(), "the altered relation remains valid OSM source");
    if (changedSource) {
      const auto changedGraph = TransportTopology::Build(*changedSource);
      CHECK(changedGraph && !changedGraph->ResolveCircuit(*changedSource, 284588),
            "promoting the pit branch to the main role rejects the ambiguous route");
    }
  }
  std::string missingMember = xml;
  const size_t relationAt = missingMember.find("<relation id=\"284588\">");
  const size_t refAt = missingMember.find("ref=\"", relationAt);
  CHECK(relationAt != std::string::npos && refAt != std::string::npos,
        "the pinned relation contains source way references");
  if (relationAt != std::string::npos && refAt != std::string::npos) {
    const size_t valueAt = refAt + sizeof("ref=\"") - 1;
    const size_t valueEnd = missingMember.find('"', valueAt);
    missingMember.replace(valueAt, valueEnd - valueAt, "999999999999999999");
    const auto changedSource = OsmXmlReader::Read(
        missingMember, {.DatasetId = "openstreetmap", .Revision = "missing-member"});
    CHECK(changedSource.has_value(), "an incomplete relation remains syntactically valid");
    if (changedSource) {
      const auto refused = TransportTopology::Build(*changedSource);
      CHECK(!refused && refused.error().Code == TransportBuildErrorCode::MissingSourceObject,
            "a missing pinned member rejects the graph before route publication");
    }
  }
  return Report();
}
