#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadSurfaceBuilder.h"

#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  std::ifstream input("src/assets/world/osm/HockenheimringGrandPrix.osm", std::ios::binary);
  CHECK(input.good(), "the pinned Hockenheim source exists");
  if (!input) { return Report(); }
  const std::string xml(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  auto source = OsmXmlReader::Read(xml, {.DatasetId = "openstreetmap", .Revision = "pin-r1"});
  CHECK(source.has_value(), "the circuit source parses");
  if (!source) { return Report(); }
  auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "the circuit topology builds");
  if (!topology) { return Report(); }
  auto route = topology->ResolveCircuit(*source, 284588);
  CHECK(route.has_value(), "the directed source circuit resolves");
  if (!route) { return Report(); }

  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100.0f, 100.0f, 100.0f, 100.0f};
  block.Sources = {{.Kind = DataKind::Elevation,
                    .Tile = {.Zoom = 0, .X = 0, .Y = 0},
                    .SourceId = "analytic-dem",
                    .Revision = "r1"}};
  const auto terrain = Ground::HeightField::Of(0, {block});
  const auto constraints =
      RoadConstraintChain::Build(*topology, route->SourceIdentity, route->EdgeIds, *terrain);
  CHECK(constraints.has_value(), "all source edges sample the controlled terrain");
  if (!constraints) { return Report(); }
  const auto alignment = RoadAlignmentBuilder::Build(*constraints);
  CHECK(alignment && alignment->Closed(), "the circuit has a periodic native centerline");
  if (!alignment) { return Report(); }
  const auto surface = RoadSurfaceBuilder::Build(*alignment, TangentFrame::At(alignment->Anchor()));
  CHECK(surface.has_value(), "all Hockenheim source edges form one bounded native surface");
  if (!surface) { return Report(); }
  CHECK(surface->SurfaceGeometry.parts() == 1 && surface->SurfaceGeometry.wellFormed() &&
            surface->SurfaceGeometry.windingAgainstNormals(0) == 0 &&
            surface->Spans.size() > route->EdgeIds.size(),
        "the asphalt circuit is subdivided without missing or reversed triangles");
  std::vector<TransportEdgeId> seen;
  bool monotonic = true;
  for (const RoadSurfaceSpan &span : surface->Spans) {
    monotonic &= span.StartStationM < span.EndStationM && span.Part == 0;
    if (seen.empty() || seen.back() != span.SourceEdge) { seen.push_back(span.SourceEdge); }
  }
  CHECK(monotonic && seen == route->EdgeIds &&
            std::abs(surface->Spans.back().EndStationM - alignment->LengthM()) < 1e-7,
        "contact triangles retain all 267 ordered source edges and full lap station coverage");
  const auto positions = surface->SurfaceGeometry.positionsOf(0);
  const size_t last = positions.size() - 12;
  bool closed = positions.size() >= 24;
  if (closed) {
    for (size_t component = 0; component < 6; ++component) {
      closed &= positions[last + 6 + component] == positions[component];
    }
  }
  CHECK(closed, "the final float vertices close exactly on the first road cross section");
  return Report();
}
