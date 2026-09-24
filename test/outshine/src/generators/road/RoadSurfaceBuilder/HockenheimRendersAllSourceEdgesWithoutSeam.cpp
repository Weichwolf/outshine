#include "Check.h"
#include "EarthworkPress.h"
#include "OsmXmlReader.h"
#include "RoadSurfaceBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
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
  std::vector<EastNorth> contactPoints;
  std::vector<double> roadHeights;
  std::vector<double> contactHeights;
  constexpr std::array stationFractions{0.0, 0.25, 0.5, 0.75, 1.0};
  constexpr std::array widthFractions{0.0, 0.5, 1.0};
  constexpr size_t samplesPerSpan = stationFractions.size() * widthFractions.size();
  contactPoints.reserve(surface->Spans.size() * samplesPerSpan);
  roadHeights.reserve(surface->Spans.size() * samplesPerSpan);
  contactHeights.reserve(surface->Spans.size() * samplesPerSpan);
  for (size_t index = 0; index < surface->Spans.size(); ++index) {
    const size_t at = index * 12;
    for (const double station : stationFractions) {
      for (const double width : widthFractions) {
        const auto coordinate = [&](size_t component) {
          const double left = std::lerp(static_cast<double>(positions[at + component]),
                                        static_cast<double>(positions[at + 6 + component]),
                                        station);
          const double right = std::lerp(static_cast<double>(positions[at + 3 + component]),
                                         static_cast<double>(positions[at + 9 + component]),
                                         station);
          return std::lerp(left, right, width);
        };
        contactPoints.push_back({.EastM = coordinate(0), .NorthM = -coordinate(2)});
        roadHeights.push_back(coordinate(1));
        contactHeights.push_back(coordinate(1) + 2.0);
      }
    }
  }
  const auto contact =
      ApplyEarthworkStamps(surface->Earthworks, contactPoints, contactHeights, kMostEarthworkM);
  CHECK(surface->Earthworks.size() == surface->Spans.size() &&
            contact.Moved == contactPoints.size(),
        "every road cross-section samples inside a matching ground contact footprint");
  double minimumClearanceM = std::numeric_limits<double>::infinity();
  double maximumClearanceM = -std::numeric_limits<double>::infinity();
  for (size_t index = 0; index < contactHeights.size(); ++index) {
    const double clearanceM = roadHeights[index] - contactHeights[index];
    minimumClearanceM = std::min(minimumClearanceM, clearanceM);
    maximumClearanceM = std::max(maximumClearanceM, clearanceM);
  }
  Note("minimum road-ground clearance", minimumClearanceM, "m");
  Note("maximum road-ground clearance", maximumClearanceM, "m");
  CHECK(minimumClearanceM >= 0.02 && maximumClearanceM <= 0.15,
        "every road interval and edge remains visibly above its pressed terrain contact");
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
