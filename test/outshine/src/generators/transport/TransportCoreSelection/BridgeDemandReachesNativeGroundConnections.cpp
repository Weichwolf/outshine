#include "TransportCoreFixture.h"
#include "BridgeInputRequirements.h"
#include "Check.h"
#include "Geodesy.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <print>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::World;
  using namespace outshine::Test;
  constexpr double extentDeg = 0.25;
  const auto source = BridgeAcrossCore(extentDeg);
  const auto selection = SelectTransportCore(kTransportCore, source, kCoreLimits);
  CHECK(selection && selection->Segments.size() == 1,
        "geographic demand selects a bridge whose original landings are outside the core");
  if (!selection || selection->Segments.size() != 1) { return Report(); }
  const auto &edge = selection->Source->Topology().Edges()[selection->Segments[0].SourceEdge];
  const std::array seeds{edge.Id};
  const BridgeInputLimits limits{.SourceEdgesMost = kCoreLimits.SourceEdgesMost,
                                 .ContainerCapacityMost = size_t{64} * 1024};
  const auto inputs = SelectBridgeInputRequirements(selection->Source, seeds, limits);
  const std::vector<uint64_t> expectedGround{1, 2};
  CHECK(inputs && inputs->Source.get() == selection->Source.get() &&
            inputs->BridgeEdges.size() == 1 && inputs->GroundNodes == expectedGround &&
            inputs->Unqualified.empty(),
        "the actual selected source edge expands to both original physical ground connections, "
        "including an incoming-only ground edge");
  if (!inputs || inputs->GroundNodes.size() != 2) { return Report(); }
  const auto *west = source->Topology().FindNode(inputs->GroundNodes[0]);
  const auto *east = source->Topology().FindNode(inputs->GroundNodes[1]);
  const auto core = kTransportCore.Bounds();
  CHECK(west != nullptr && east != nullptr && core,
        "the selected conditions retain their native source coordinates");
  if (west == nullptr || east == nullptr || !core) { return Report(); }
  CHECK(west->LongitudeDeg < core->WestDeg - (core->EastDeg - core->WestDeg) &&
            east->LongitudeDeg > core->EastDeg + (core->EastDeg - core->WestDeg),
        "both conditions lie beyond a fixed one-core neighbor ring");
  constexpr double maximumGrade = 0.05;
  constexpr double westHeightM = 0;
  constexpr double eastHeightM = 2000;
  const auto cut = selection->Segments[0].Ends[0].Geographic;
  const auto westOffset =
      EnuOffsetM({.LongitudeDeg = west->LongitudeDeg, .LatitudeDeg = west->LatitudeDeg},
                 {.LongitudeDeg = cut.LongitudeDeg, .LatitudeDeg = cut.LatitudeDeg});
  const auto eastOffset =
      EnuOffsetM({.LongitudeDeg = east->LongitudeDeg, .LatitudeDeg = east->LatitudeDeg},
                 {.LongitudeDeg = cut.LongitudeDeg, .LatitudeDeg = cut.LatitudeDeg});
  const double westRise = maximumGrade * std::hypot(westOffset.EastM, westOffset.NorthM);
  const double eastRise = maximumGrade * std::hypot(eastOffset.EastM, eastOffset.NorthM);
  const double lower = std::max(westHeightM - westRise, eastHeightM - eastRise);
  const double upper = std::min(westHeightM + westRise, eastHeightM + eastRise);
  CHECK(lower > 0 && lower <= upper,
        "the distant source landing changes the feasible cut-height interval, so a local zero "
        "height cannot replace the requested far condition");
  std::println("TRANSPORT_CORE_BRIDGE source_path={} core_segments={} bridge_segments={} "
               "ground_connections={} feasible_cut_min_m={} feasible_cut_max_m={}",
               edge.Id.PathId,
               selection->Segments.size(),
               inputs->BridgeEdges.size(),
               inputs->GroundNodes.size(),
               lower,
               upper);
  return Report();
}
