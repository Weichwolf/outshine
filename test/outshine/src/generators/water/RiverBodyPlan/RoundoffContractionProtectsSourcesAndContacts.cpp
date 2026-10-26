#include "WaterSurfaceTopology.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <utility>
#include <vector>
#include <cstdint>

namespace {
using namespace outshine;
using namespace outshine::Generators;

WaterAsset Source(bool hole = false) {
  WaterAsset::Data data{
      .Surfaces = {{.FirstRing = 0, .RingCount = 1}, {.FirstRing = 0, .RingCount = 1}},
      .Rings = {{.FirstPoint = 0, .PointCount = 4}},
      .Courses = {},
      .Levels = {},
      .Points = {-1, -1, -1, 1, 1, 1, 1, -1},
      .Tiles = {},
      .Sources = {},
      .Inputs = {}};
  if (hole) {
    data.Surfaces[0].RingCount = 2;
    data.Rings.push_back({.FirstPoint = 4, .PointCount = 4});
    data.Points.insert(data.Points.end(), {.0001, .0001, .0001, .0002, .0002, .0002, .0002, .0001});
  }
  return WaterAsset(std::move(data));
}

WaterSurfaceTopology Topology(std::array<double, 2> origin = {1, 0}) {
  const double next = std::nextafter(origin[0], std::numeric_limits<double>::infinity());
  return {.Anchor = {},
          .VerticesXY = {origin,
                         {next, origin[1]},
                         {origin[0] + 1, origin[1] + 1},
                         {origin[0] - 1, origin[1] + 1}},
          .Faces = {{{0, 1, 2}, 0}, {{0, 2, 3}, 0}},
          .FaceSourceOffsets = {0, 1, 2},
          .FaceSources = {0, 0}};
}
}

int main() {
  const auto water = Source();
  const auto frame = TangentFrame::At({});
  auto topology = Topology();
  const double displacement = topology.VerticesXY[1][0] - topology.VerticesXY[0][0];
  const auto report = ContractWaterRoundoffEdges(water, frame, topology);
  CHECK(report && report->ContractedEdges == 1 && topology.Faces.size() == 1,
        "only index-degenerate faces disappear when one adjacent-coordinate edge contracts");
  CHECK(report && report->MaximumDisplacementBoundM >= displacement &&
            report->SweptAreaBoundM2 >= displacement / 2,
        "outward error bounds cover the actual vertex shift and removed triangle");
  CHECK(topology.FaceSources == std::vector<uint32_t>{0} &&
            topology.FaceSourceOffsets == std::vector<uint32_t>({0, 1}),
        "surviving facets retain their source coverage");
  topology = Topology();
  const std::array<uint32_t, 1> contact{0};
  const auto pinned =
      ContractWaterRoundoffEdges(water, frame, topology, {.ProtectedVertices = contact});
  CHECK(pinned && pinned->ContractedEdges == 0 && topology.Faces.size() == 2,
        "a protected contact cannot move or merge");
  topology = Topology();
  topology.FaceSources[0] = 1;
  const auto source = ContractWaterRoundoffEdges(water, frame, topology);
  CHECK(source && source->ContractedEdges == 0 && topology.Faces.size() == 2,
        "a contraction cannot erase the only facet of a source feature");
  topology = Topology();
  topology.VerticesXY[1][0] =
      std::nextafter(topology.VerticesXY[1][0], std::numeric_limits<double>::infinity());
  const auto close = ContractWaterRoundoffEdges(water, frame, topology);
  CHECK(close && close->ContractedEdges == 0,
        "merely short edges outside adjacent binary64 coordinates remain unchanged");
  topology = Topology();
  topology.VerticesXY[0][0] = std::nextafter(std::nextafter(1.0, 0.0), 0.0);
  topology.VerticesXY[1][0] = 1;
  const auto exponent = ContractWaterRoundoffEdges(water, frame, topology);
  CHECK(exponent && exponent->ContractedEdges == 0,
        "two binary64 steps below a power of two cannot use the wider spacing above it");
  topology = Topology();
  const auto bounded =
      ContractWaterRoundoffEdges(water, frame, topology, {.Edges = 0, .ProtectedVertices = {}});
  CHECK(!bounded && bounded.error() == WaterTopologyError::CapacityExceeded,
        "needed repair cannot silently exceed the declared work limit");
  const auto island = Source(true);
  const auto point = frame.ToLocalGroundPosition({.LongitudeDeg = .0001, .LatitudeDeg = .0001});
  topology = Topology({point.EastM, point.NorthM});
  const auto protectedHole = ContractWaterRoundoffEdges(island, frame, topology);
  CHECK(protectedHole && protectedHole->ContractedEdges == 0,
        "declared source-island corners are protected before any face mutation");
  return outshine::Test::Report();
}
