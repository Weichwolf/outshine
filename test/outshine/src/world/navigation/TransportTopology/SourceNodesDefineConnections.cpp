#include "OsmXmlReader.h"
#include "TransportTopology.h"
#include "Check.h"

#include <cstdint>
#include <string_view>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  using namespace outshine::World;
  constexpr std::string_view xml =
      "<osm version='0.6'>"
      "<node id='1' lat='0' lon='0'/><node id='2' lat='0' lon='1'/>"
      "<node id='3' lat='0' lon='2'/><node id='4' lat='0' lon='3'/>"
      "<node id='5' lat='0' lon='1'/><node id='6' lat='0' lon='2'/>"
      "<node id='7' lat='0' lon='4'/><node id='8' lat='0' lon='5'/>"
      "<node id='9' lat='1' lon='1'/><node id='11' lat='1' lon='0'/>"
      "<node id='12' lat='1' lon='1'/><node id='13' lat='2' lon='0'/>"
      "<node id='14' lat='2' lon='1'/><node id='15' lat='3' lon='0'/>"
      "<node id='16' lat='3' lon='1'/><node id='17' lat='4' lon='0'/>"
      "<node id='18' lat='4' lon='1'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/><tag k='highway' v='primary'/>"
      "<tag k='oneway' v='yes'/></way>"
      "<way id='11'><nd ref='2'/><nd ref='3'/><tag k='highway' v='primary'/>"
      "<tag k='oneway' v='yes'/><tag k='bridge' v='yes'/><tag k='layer' v='1'/></way>"
      "<way id='12'><nd ref='3'/><nd ref='4'/><tag k='highway' v='primary'/>"
      "<tag k='oneway' v='yes'/></way>"
      "<way id='13'><nd ref='5'/><nd ref='6'/><tag k='highway' v='primary'/>"
      "<tag k='oneway' v='yes'/></way>"
      "<way id='14'><nd ref='4'/><nd ref='7'/><tag k='highway' v='primary'/>"
      "<tag k='oneway' v='yes'/><tag k='tunnel' v='yes'/><tag k='layer' v='-1'/></way>"
      "<way id='15'><nd ref='7'/><nd ref='8'/><tag k='highway' v='primary'/>"
      "<tag k='oneway' v='yes'/></way>"
      "<way id='20'><nd ref='2'/><nd ref='9'/><tag k='railway' v='rail'/>"
      "<tag k='oneway' v='yes'/></way>"
      "<way id='30'><nd ref='11'/><nd ref='12'/><tag k='highway' v='service'/>"
      "<tag k='oneway' v='-1'/></way>"
      "<way id='40'><nd ref='13'/><nd ref='14'/><tag k='highway' v='service'/>"
      "<tag k='oneway' v='no'/></way>"
      "<way id='50'><nd ref='15'/><nd ref='16'/><tag k='highway' v='construction'/></way>"
      "<way id='60'><nd ref='17'/><nd ref='18'/><tag k='highway' v='service'/>"
      "<tag k='access' v='no'/></way></osm>";
  const auto source = OsmXmlReader::Read(xml, {.DatasetId = "osm-analytical", .Revision = "r1"});
  CHECK(source.has_value(), "analytical transport source parses");
  if (!source) { return Report(); }
  const auto built = TransportTopology::Build(*source);
  CHECK(built.has_value(), "the complete source builds one native topology");
  if (!built) { return Report(); }
  const TransportTopology &graph = *built;
  const auto forward = [](uint64_t wayId) {
    return TransportEdgeId{
        .WayId = wayId, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward};
  };
  const auto reverse = [](uint64_t wayId) {
    return TransportEdgeId{
        .WayId = wayId, .SegmentOrdinal = 0, .Direction = EdgeDirection::Reverse};
  };
  const TransportEdge *road = graph.FindEdge(forward(10));
  const TransportEdge *bridge = graph.FindEdge(forward(11));
  const TransportEdge *ground = graph.FindEdge(forward(12));
  const TransportEdge *xyCrossing = graph.FindEdge(forward(13));
  const TransportEdge *tunnel = graph.FindEdge(forward(14));
  const TransportEdge *tunnelExit = graph.FindEdge(forward(15));
  const TransportEdge *rail = graph.FindEdge(forward(20));
  CHECK(road && bridge && ground && xyCrossing && tunnel && tunnelExit && rail,
        "source ways yield stable directed edge IDs");
  if (!road || !bridge || !ground || !xyCrossing || !tunnel || !tunnelExit || !rail) {
    return Report();
  }
  CHECK(TransportTopology::CanContinue(*road, *bridge) &&
            TransportTopology::CanContinue(*bridge, *ground) && bridge->Layer == 1 &&
            bridge->Bridge && ground->Layer == 0,
        "an explicit bridge abutment connects despite a layer transition");
  CHECK(TransportTopology::CanContinue(*ground, *tunnel) &&
            TransportTopology::CanContinue(*tunnel, *tunnelExit) && tunnel->Tunnel,
        "a tunnel and its ground exit remain in the logical graph");
  CHECK(!TransportTopology::CanContinue(*road, *xyCrossing) &&
            graph.FindNode(2)->LongitudeDeg == graph.FindNode(5)->LongitudeDeg,
        "coincident XY coordinates with distinct OSM node IDs do not connect");
  CHECK(!TransportTopology::CanContinue(*road, *rail),
        "a shared node does not turn a motor road into a rail line");
  CHECK(graph.FindEdge(forward(30)) == nullptr && graph.FindEdge(reverse(30)) &&
            graph.FindEdge(reverse(30))->FromNodeId == 12 &&
            graph.FindEdge(reverse(30))->ToNodeId == 11,
        "oneway=-1 reverses source way direction without renumbering its segment");
  CHECK(graph.FindEdge(forward(40)) && graph.FindEdge(reverse(40)) &&
            graph.UnclassifiedWayCount() == 1 && graph.FindEdge(forward(50))->Modes == 0 &&
            !graph.FindEdge(forward(60))->Allows(TransportMode::Motor),
        "two-way, unknown and forbidden source semantics remain distinct");
  bool outgoingBySourceNode = true;
  for (const OutgoingTransportEdge &outgoing : graph.OutgoingFrom(2)) {
    outgoingBySourceNode &= graph.Edges()[outgoing.EdgeIndex].FromNodeId == 2;
  }
  CHECK(outgoingBySourceNode && graph.OutgoingFrom(2).size() == 2,
        "adjacency is indexed by source node ID rather than geographic snapping");
  return Report();
}
