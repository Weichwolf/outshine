#include "OsmXmlReader.h"
#include "TransportTopology.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace {

enum class Variation {
  Base,
  Permuted,
  ReversedMiddle,
  Fork,
  PitAsMain,
  Bidirectional,
  NotCircuit,
  MissingWay
};

std::string CircuitXml(Variation variation) {
  std::string xml = "<osm version='0.6'>"
                    "<node id='1' lat='0' lon='0'/><node id='2' lat='0' lon='1'/>"
                    "<node id='3' lat='1' lon='1'/><node id='4' lat='0.5' lon='1.5'/>";
  const std::string_view way10 =
      "<way id='10'><nd ref='1'/><nd ref='2'/><tag k='highway' v='raceway'/>"
      "<tag k='oneway' v='yes'/></way>";
  const std::string_view way11 =
      variation == Variation::ReversedMiddle
          ? "<way id='11'><nd ref='3'/><nd ref='2'/>"
            "<tag k='highway' v='raceway'/><tag k='oneway' v='yes'/></way>"
      : variation == Variation::Bidirectional
          ? "<way id='11'><nd ref='2'/><nd ref='3'/>"
            "<tag k='highway' v='raceway'/><tag k='oneway' v='no'/></way>"
          : "<way id='11'><nd ref='2'/><nd ref='3'/>"
            "<tag k='highway' v='raceway'/><tag k='oneway' v='yes'/></way>";
  const std::string_view way12 =
      "<way id='12'><nd ref='3'/><nd ref='1'/><tag k='highway' v='raceway'/>"
      "<tag k='oneway' v='yes'/></way>";
  const std::string_view pit = "<way id='13'><nd ref='2'/><nd ref='4'/><nd ref='3'/>"
                               "<tag k='highway' v='raceway'/><tag k='oneway' v='yes'/></way>";
  const std::array ways{way10, way11, way12, pit};
  const std::array<size_t, 4> order = variation == Variation::Permuted
                                          ? std::array<size_t, 4>{3, 2, 0, 1}
                                          : std::array<size_t, 4>{0, 1, 2, 3};
  for (const size_t index : order) { xml += ways[index]; }
  if (variation == Variation::Fork) {
    xml += "<way id='14'><nd ref='2'/><nd ref='3'/>"
           "<tag k='highway' v='raceway'/><tag k='oneway' v='yes'/></way>";
  }
  xml += "<relation id='9'>";
  if (variation == Variation::Permuted) {
    xml += "<member type='way' ref='11' role=''/>"
           "<member type='way' ref='13' role='pitlane'/>"
           "<member type='way' ref='10' role=''/>"
           "<member type='way' ref='12' role=''/>";
  } else {
    xml += "<member type='way' ref='12' role=''/>"
           "<member type='way' ref='10' role=''/>"
           "<member type='way' ref='11' role=''/>";
    xml += variation == Variation::PitAsMain ? "<member type='way' ref='13' role=''/>"
                                             : "<member type='way' ref='13' role='pitlane'/>";
  }
  if (variation == Variation::Fork) { xml += "<member type='way' ref='14' role=''/>"; }
  if (variation == Variation::MissingWay) { xml += "<member type='way' ref='99' role=''/>"; }
  xml += variation == Variation::NotCircuit ? "<tag k='type' v='route'/></relation></osm>"
                                            : "<tag k='type' v='circuit'/></relation></osm>";
  return xml;
}

}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  using namespace outshine::World;
  const auto source =
      OsmXmlReader::Read(CircuitXml(Variation::Base), {.DatasetId = "osm", .Revision = "r1"});
  CHECK(source.has_value(), "the three-way circuit source parses");
  if (!source) { return Report(); }
  const auto graph = TransportTopology::Build(*source);
  CHECK(graph.has_value(), "the source builds a graph with an alternate pit branch");
  if (!graph) { return Report(); }
  const auto route = graph->ResolveCircuit(*source, 9);
  CHECK(route && route->EdgeIds.size() == 3 && route->StartNodeId == 1 &&
            route->EdgeIds[0].WayId == 10 && route->EdgeIds[1].WayId == 11 &&
            route->EdgeIds[2].WayId == 12,
        "directed topology orders the main cycle, excluding pitlane by role");
  if (!route) { return Report(); }

  const auto newerSource =
      OsmXmlReader::Read(CircuitXml(Variation::Base), {.DatasetId = "osm", .Revision = "r2"});
  CHECK(newerSource.has_value(), "a separate revision parses");
  if (newerSource) {
    const auto mixed = graph->ResolveCircuit(*newerSource, 9);
    CHECK(!mixed && mixed.error().Code == CircuitErrorCode::SourceMismatch,
          "route resolution cannot combine a graph and relation from different revisions");
  }

  const auto permutedSource =
      OsmXmlReader::Read(CircuitXml(Variation::Permuted), {.DatasetId = "osm", .Revision = "r1"});
  CHECK(permutedSource.has_value(), "permuted source parses");
  if (!permutedSource) { return Report(); }
  const auto permutedGraph = TransportTopology::Build(*permutedSource);
  CHECK(permutedGraph.has_value(), "permuted source builds");
  if (!permutedGraph) { return Report(); }
  const auto permutedRoute = permutedGraph->ResolveCircuit(*permutedSource, 9);
  bool sameEdges = graph->Edges().size() == permutedGraph->Edges().size();
  if (sameEdges) {
    for (size_t at = 0; at < graph->Edges().size(); ++at) {
      sameEdges &= graph->Edges()[at].Id == permutedGraph->Edges()[at].Id;
    }
  }
  CHECK(permutedRoute && sameEdges && route->EdgeIds == permutedRoute->EdgeIds,
        "source and relation member order cannot change edge IDs or the circuit route");

  constexpr std::array failures{
      std::pair{Variation::ReversedMiddle, CircuitErrorCode::AmbiguousDirection},
      std::pair{Variation::Fork, CircuitErrorCode::AmbiguousDirection},
      std::pair{Variation::PitAsMain, CircuitErrorCode::AmbiguousDirection},
      std::pair{Variation::Bidirectional, CircuitErrorCode::AmbiguousDirection},
      std::pair{Variation::NotCircuit, CircuitErrorCode::NotCircuit}};
  for (const auto &[variation, expected] : failures) {
    const auto changedSource =
        OsmXmlReader::Read(CircuitXml(variation), {.DatasetId = "osm", .Revision = "r2"});
    CHECK(changedSource.has_value(), "mutated source remains valid OSM XML");
    if (!changedSource) { continue; }
    const auto changedGraph = TransportTopology::Build(*changedSource);
    CHECK(changedGraph.has_value(), "mutated source still builds a graph");
    if (!changedGraph) { continue; }
    const auto changedRoute = changedGraph->ResolveCircuit(*changedSource, 9);
    CHECK(!changedRoute && changedRoute.error().Code == expected,
          "ambiguous or semantically wrong source cannot publish a circuit route");
  }
  const auto missingSource =
      OsmXmlReader::Read(CircuitXml(Variation::MissingWay), {.DatasetId = "osm", .Revision = "r2"});
  CHECK(missingSource.has_value(), "an incomplete relation remains parseable");
  if (missingSource) {
    const auto refused = TransportTopology::Build(*missingSource);
    CHECK(!refused && refused.error().Code == TransportBuildErrorCode::MissingSourceObject &&
              refused.error().SourceId == 9,
          "a missing member rejects the whole graph candidate with relation provenance");
  }
  return Report();
}
