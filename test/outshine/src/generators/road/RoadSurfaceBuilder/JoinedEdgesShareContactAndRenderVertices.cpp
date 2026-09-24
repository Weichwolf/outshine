#include "Check.h"
#include "EarthworkPress.h"
#include "OsmXmlReader.h"
#include "RoadSurfaceBuilder.h"

#include <array>
#include <cmath>
#include <span>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  constexpr std::string_view xml =
      "<osm version='0.6'>"
      "<node id='1' lat='0.01' lon='0.01'/>"
      "<node id='2' lat='0.01' lon='0.0101'/>"
      "<node id='3' lat='0.01005' lon='0.0102'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/>"
      "<tag k='highway' v='primary'/><tag k='oneway' v='yes'/>"
      "<tag k='surface' v='asphalt'/><tag k='width' v='6'/></way>"
      "<way id='20'><nd ref='2'/><nd ref='3'/>"
      "<tag k='highway' v='primary'/><tag k='oneway' v='yes'/>"
      "<tag k='surface' v='asphalt'/><tag k='width' v='8'/></way></osm>";
  auto source = OsmXmlReader::Read(xml, {.DatasetId = "analytic", .Revision = "r1"});
  CHECK(source.has_value(), "the analytic road source parses");
  if (!source) { return Report(); }
  auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "the road topology builds");
  if (!topology) { return Report(); }

  Ground::HeightField::Block block;
  block.At = Ground::HeightField::SpotOf({.LongitudeDeg = 0.01, .LatitudeDeg = 0.01}, 14);
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100.0f, 100.0f, 100.0f, 100.0f};
  block.Sources = {{.Kind = DataKind::Elevation,
                    .Tile = {.Zoom = 14,
                             .X = static_cast<uint32_t>(block.At.X),
                             .Y = static_cast<uint32_t>(block.At.Y)},
                    .SourceId = "analytic-dem",
                    .Revision = "r1"}};
  const auto terrain = Ground::HeightField::Of(14, {block});
  const TransportEdgeId first{
      .WayId = 10, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward};
  const TransportEdgeId second{
      .WayId = 20, .SegmentOrdinal = 0, .Direction = EdgeDirection::Forward};
  const std::array route{first, second};
  const auto constraints =
      RoadConstraintChain::Build(*topology, source->SourceIdentity(), route, *terrain);
  CHECK(constraints.has_value(), "both road edges sample sourced terrain");
  if (!constraints) { return Report(); }
  const auto alignment = RoadAlignmentBuilder::Build(*constraints);
  CHECK(alignment.has_value(), "the road edges build one native centerline");
  if (!alignment) { return Report(); }

  const TangentFrame frame = TangentFrame::At({.LongitudeDeg = 0.009, .LatitudeDeg = 0.009});
  const auto surface = RoadSurfaceBuilder::Build(*alignment, frame);
  CHECK(surface.has_value(), "the source alignment builds a native road surface");
  if (!surface) { return Report(); }
  CHECK(surface->SourceIdentity == source->SourceIdentity() &&
            surface->TerrainDigest == terrain->RasterDigest() &&
            surface->RenderAnchor.LongitudeDeg == frame.Anchor().LongitudeDeg &&
            surface->SurfaceGeometry.parts() == 1 && surface->SurfaceGeometry.wellFormed() &&
            surface->SurfaceGeometry.windingAgainstNormals(0) == 0,
        "render and contact provenance share one well-wound native geometry");
  const auto positions = surface->SurfaceGeometry.positionsOf(0);
  const auto triangles = surface->SurfaceGeometry.trianglesOf(0);
  CHECK(!surface->Spans.empty() && triangles.size() == surface->Spans.size() * 6 &&
            positions.size() == surface->Spans.size() * 12 &&
            surface->Earthworks.size() == surface->Spans.size(),
        "each station interval owns two triangles and one matching ground contact");
  const EastNorth firstRoadCenter{
      .EastM = (positions[0] + positions[3] + positions[6] + positions[9]) * 0.25,
      .NorthM = -(positions[2] + positions[5] + positions[8] + positions[11]) * 0.25};
  const double firstRoadHeightM =
      (positions[1] + positions[4] + positions[7] + positions[10]) * 0.25;
  const std::array groundAt{firstRoadCenter};
  std::array groundHeightM{firstRoadHeightM + 2.0};
  const auto pressed =
      ApplyEarthworkStamps(std::span<const EarthworkStamp>(&surface->Earthworks.front(), 1),
                           groundAt,
                           groundHeightM,
                           kMostEarthworkM);
  CHECK(pressed.Moved == 1 && groundHeightM[0] < firstRoadHeightM - 0.02 &&
            groundHeightM[0] > firstRoadHeightM - 0.15,
        "the matching corridor stamp cuts a raised terrain sample beneath the road triangles");
  bool continuous = true;
  bool allEdges = false;
  for (size_t index = 0; index < surface->Spans.size(); ++index) {
    const RoadSurfaceSpan &span = surface->Spans[index];
    continuous &= span.StartStationM < span.EndStationM && span.Part == 0 &&
                  span.FirstTriangle == index * 2 &&
                  (span.SourceEdge == first || span.SourceEdge == second);
    allEdges |= span.SourceEdge == second;
    if (index == 0) { continue; }
    const RoadSurfaceSpan &previous = surface->Spans[index - 1];
    continuous &= std::abs(previous.EndStationM - span.StartStationM) < 1e-9;
    for (size_t component = 0; component < 6; ++component) {
      continuous &=
          positions[(index - 1) * 12 + 6 + component] == positions[index * 12 + component];
    }
  }
  CHECK(continuous && allEdges && surface->Spans.front().StartStationM == 0.0 &&
            std::abs(surface->Spans.back().EndStationM - alignment->LengthM()) < 1e-8,
        "triangle provenance and shared float vertices cover both source edges without gaps");
  const auto tooSmall = RoadSurfaceBuilder::Build(*alignment, frame, {.MaximumSegments = 1});
  CHECK(!tooSmall && tooSmall.error().Code == RoadSurfaceErrorCode::SampleBudgetExceeded,
        "a bounded builder rejects incomplete road surface generation");
  const auto invalid = RoadSurfaceBuilder::Build(*alignment, frame, {.SurfaceLiftM = -0.1});
  CHECK(!invalid && invalid.error().Code == RoadSurfaceErrorCode::InvalidOptions,
        "negative surface lift cannot be hidden inside a mesh");
  return Report();
}
