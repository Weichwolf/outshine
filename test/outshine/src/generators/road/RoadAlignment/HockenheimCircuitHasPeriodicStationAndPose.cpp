#include "HockenheimTransportFixture.h"
#include "HockenheimCircuitFixture.h"
#include "OsmTransportBuilder.h"
#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadAlignment.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace Circuit = outshine::Test::HockenheimFixture;

namespace {
constexpr double kMinimumArcLengthM = 4500;
constexpr double kMaximumArcLengthM = 4700;
constexpr double kPositionToleranceM = 1e-7;
constexpr double kDirectionTolerance = 1e-10;
constexpr double kMaximumMetreStepM = 1.1;
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  auto fixture = Circuit::LoadNetwork();
  CHECK(fixture,
        fixture ? "pinned source, topology and directed circuit load" : fixture.error().c_str());
  if (!fixture) { return Report(); }
  const auto &topology = fixture->Graph;
  auto &route = fixture->Route;

  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100.0f, 100.0f, 100.0f, 100.0f};
  block.Sources = {{.Kind = DataKind::Elevation,
                    .Tile = {.Zoom = 0, .X = 0, .Y = 0},
                    .SourceId = "analytic-dem",
                    .Revision = "r1"}};
  const auto heights = Ground::HeightField::Of(0, {block});
  const auto constraints =
      RoadConstraintChain::Build(topology, route.SourceIdentity, route.EdgeIds, *heights);
  CHECK(constraints.has_value(), "all route edges have pinned terrain constraints");
  if (!constraints) { return Report(); }
  const auto alignment = RoadAlignmentBuilder::Build(*constraints);
  CHECK(alignment.has_value(), "the closed source circuit builds a periodic road centerline");
  if (!alignment) { return Report(); }
  CHECK(alignment->Closed() && alignment->Edges().size() == Circuit::EdgeCount &&
            alignment->LengthM() > kMinimumArcLengthM && alignment->LengthM() < kMaximumArcLengthM,
        "the source circuit retains every edge and a plausible measured arc length");

  bool allSeams = true;
  double maximumSourceOffsetM = 0.0;
  for (size_t index = 0; index < alignment->Edges().size(); ++index) {
    const RoadAlignmentEdge &current = alignment->Edges()[index];
    const RoadAlignmentEdge &next = alignment->Edges()[(index + 1) % alignment->Edges().size()];
    const auto end =
        alignment->AtEdgeStation(current.SourceEdge, current.EndStationM - current.StartStationM);
    const auto start = alignment->AtEdgeStation(next.SourceEdge, 0.0);
    allSeams &= current.SourceEdge == route.EdgeIds[index] &&
                alignment->FindEdge(current.SourceEdge) != nullptr && end && start &&
                std::abs(end->PositionM.EastM - start->PositionM.EastM) < kPositionToleranceM &&
                std::abs(end->PositionM.NorthM - start->PositionM.NorthM) < kPositionToleranceM &&
                std::abs(end->PositionM.UpM - start->PositionM.UpM) < kPositionToleranceM &&
                Dot(end->TangentEnu, start->TangentEnu) > 1.0 - kDirectionTolerance &&
                std::abs(end->WidthM - start->WidthM) < kDirectionTolerance &&
                current.EndStationM > current.StartStationM;
    const EastNorthUp &from = constraints->Points()[index].TerrainLocalM;
    const EastNorthUp &to = constraints->Points()[index + 1].TerrainLocalM;
    const double eastM = to.EastM - from.EastM;
    const double northM = to.NorthM - from.NorthM;
    const double chordM = std::hypot(eastM, northM);
    for (int sample = 1; sample < 8; ++sample) {
      const double edgeStationM =
          (current.EndStationM - current.StartStationM) * static_cast<double>(sample) / 8.0;
      const auto pose = alignment->AtEdgeStation(current.SourceEdge, edgeStationM);
      if (!pose) {
        allSeams = false;
        continue;
      }
      const double offsetM = std::abs((pose->PositionM.EastM - from.EastM) * northM -
                                      (pose->PositionM.NorthM - from.NorthM) * eastM) /
                             chordM;
      maximumSourceOffsetM = std::max(maximumSourceOffsetM, offsetM);
    }
  }
  CHECK(allSeams, "all 267 edge seams, including lap closure, share pose and width");
  CHECK(maximumSourceOffsetM < 1.0,
        "the smoothed centerline stays within one metre of the independent source chords");

  const auto first = alignment->AtStation(0.0);
  const auto lap = alignment->AtStation(alignment->LengthM());
  CHECK(first && lap && first->SourceEdge == lap->SourceEdge &&
            first->PositionM == lap->PositionM && first->TangentEnu.Axis == lap->TangentEnu.Axis,
        "one full station period returns to the identical source pose");
  bool continuousMotion = true;
  auto previous = first;
  for (size_t station = 1; static_cast<double>(station) < alignment->LengthM(); ++station) {
    const auto stationM = static_cast<double>(station);
    const auto current = alignment->AtStation(stationM);
    continuousMotion &=
        previous && current &&
        std::hypot(current->PositionM.EastM - previous->PositionM.EastM,
                   current->PositionM.NorthM - previous->PositionM.NorthM,
                   current->PositionM.UpM - previous->PositionM.UpM) < kMaximumMetreStepM;
    previous = current;
  }
  CHECK(continuousMotion, "metre-spaced camera samples contain no route-edge teleport");
  return Report();
}
