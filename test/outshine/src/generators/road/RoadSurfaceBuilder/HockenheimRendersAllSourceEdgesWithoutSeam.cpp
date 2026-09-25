#include "Check.h"
#include "EarthworkPress.h"
#include "OsmXmlReader.h"
#include "RoadSurfaceBuilder.h"
#include "RoadSurfaceSampler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <numeric>
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
  auto surface = RoadSurfaceBuilder::Build(*alignment, TangentFrame::At(alignment->Anchor()));
  CHECK(surface.has_value(), "all Hockenheim source edges form one bounded native surface");
  if (!surface) { return Report(); }
  size_t missingContacts = 0;
  double firstMissingStationM = -1.0;
  double firstMissingLateralM = 0.0;
  double firstMissingWidthM = 0.0;
  double maximumCenterOffsetM = 0.0;
  const auto sampleContact = [&](double stationM) {
    const auto pose = alignment->AtStation(stationM);
    if (!pose) {
      ++missingContacts;
      if (firstMissingStationM < 0.0) { firstMissingStationM = stationM; }
      return;
    }
    for (double fraction : {-0.45, 0.0, 0.45}) {
      const auto contact =
          RoadSurfaceSampler::At(*alignment, *surface, stationM, pose->WidthM * fraction);
      if (!contact) {
        ++missingContacts;
        if (firstMissingStationM < 0.0) {
          firstMissingStationM = stationM;
          firstMissingLateralM = pose->WidthM * fraction;
          firstMissingWidthM = pose->WidthM;
        }
        continue;
      }
      if (fraction == 0.0) {
        maximumCenterOffsetM =
            std::max(maximumCenterOffsetM, std::abs(contact->PositionM[1] - pose->PositionM.UpM));
      }
    }
  };
  for (double stationM = 0.0; stationM < alignment->LengthM(); stationM += 0.5) {
    sampleContact(stationM);
  }
  for (const RoadAlignmentEdge &edge : alignment->Edges()) {
    sampleContact(edge.StartStationM);
    sampleContact(edge.EndStationM);
  }
  sampleContact(alignment->LengthM());
  if (missingContacts > 0) {
    Note("missing road contacts", static_cast<double>(missingContacts), "samples");
    Note("first missing road contact station", firstMissingStationM, "m");
    Note("first missing road contact lateral offset", firstMissingLateralM, "m");
    Note("first missing road contact width", firstMissingWidthM, "m");
    const auto spanAt = std::ranges::upper_bound(
        surface->Spans, firstMissingStationM, {}, &RoadSurfaceSpan::StartStationM);
    if (spanAt != surface->Spans.begin()) {
      Note("missing contact span start", (spanAt - 1)->StartStationM, "m");
      Note("missing contact span end", (spanAt - 1)->EndStationM, "m");
      Note("missing contact source way", static_cast<double>((spanAt - 1)->SourceEdge.WayId), "id");
    }
    for (double distanceM : {-1.0, 0.0, 1.0}) {
      const auto pose = alignment->AtStation(firstMissingStationM + distanceM);
      if (pose) {
        Note("turn tangent east", pose->TangentEnu[0], "unit");
        Note("turn tangent north", pose->TangentEnu[1], "unit");
      }
    }
    Note("contact one centimetre before",
         static_cast<double>(
             RoadSurfaceSampler::At(
                 *alignment, *surface, firstMissingStationM - 0.01, firstMissingLateralM)
                 .has_value()),
         "bool");
    Note("contact one centimetre after",
         static_cast<double>(
             RoadSurfaceSampler::At(
                 *alignment, *surface, firstMissingStationM + 0.01, firstMissingLateralM)
                 .has_value()),
         "bool");
  }
  Note("largest centerline-to-road height difference", maximumCenterOffsetM, "m");
  CHECK(missingContacts == 0,
        "the whole circuit has center and near-edge contact at half-metre stations and every seam");
  const auto outside = RoadSurfaceSampler::At(*alignment, *surface, 0.0, 100.0);
  CHECK(!outside && !RoadSurfaceSampler::At(*alignment, *surface, -1.0, 0.0) &&
            !RoadSurfaceSampler::At(
                *alignment, *surface, std::numeric_limits<double>::quiet_NaN(), 0.0),
        "invalid station and lateral offsets refuse contact");
  surface->TerrainDigest ^= 1u;
  CHECK(!RoadSurfaceSampler::At(*alignment, *surface, 0.0, 0.0),
        "a changed terrain revision refuses old contact geometry");
  surface->TerrainDigest ^= 1u;
  surface->SourceIdentity.Revision = "pin-r2";
  CHECK(!RoadSurfaceSampler::At(*alignment, *surface, 0.0, 0.0),
        "a changed OSM revision refuses old contact geometry");
  surface->SourceIdentity.Revision = "pin-r1";
  const RoadSurfaceSpan removed = surface->Spans[1];
  surface->Spans.erase(surface->Spans.begin() + 1);
  CHECK(!RoadSurfaceSampler::At(
            *alignment, *surface, std::midpoint(removed.StartStationM, removed.EndStationM), 0.0),
        "a missing published station span cannot be inferred from alignment alone");
  surface->Spans.insert(surface->Spans.begin() + 1, removed);
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
