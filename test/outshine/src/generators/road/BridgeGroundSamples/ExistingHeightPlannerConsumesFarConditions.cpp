#include "BridgeGroundFixture.h"
#include "TransportCoreFixture.h"
#include "TransportCoreSelection.h"
#include "RoadHeightPlan.h"
#include "Geodesy.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <print>
#include <variant>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto fixture = MakeBridgeGroundFixture();
  const auto core = SelectTransportCore(kTransportCore, fixture.Inputs.Source, kCoreLimits);
  const auto samples = SampleBridgeGround(fixture.Inputs, fixture.Terrain, kGroundLimits);
  CHECK(core && samples && core->Segments.size() == 1 && samples->Samples.size() == 2 &&
            samples->Work.Unresolved == 0,
        "geographic bridge demand binds both actual source-provenanced far DEM conditions");
  if (!core || !samples || core->Segments.size() != 1 || samples->Samples.size() != 2) {
    return Report();
  }
  const auto *west = std::get_if<BridgeGroundValue>(&samples->Samples[0].Value);
  const auto *east = std::get_if<BridgeGroundValue>(&samples->Samples[1].Value);
  const auto *westNode = samples->Source->Topology().FindNode(samples->Samples[0].SourceNode);
  const auto *eastNode = samples->Source->Topology().FindNode(samples->Samples[1].SourceNode);
  CHECK(west != nullptr && east != nullptr && westNode != nullptr && eastNode != nullptr,
        "each sourced condition keeps its exact original native node");
  if (west == nullptr || east == nullptr || westNode == nullptr || eastNode == nullptr) {
    return Report();
  }
  const auto cut = core->Segments[0].Ends[0].Geographic;
  const auto westOffset =
      EnuOffsetM({.LongitudeDeg = westNode->LongitudeDeg, .LatitudeDeg = westNode->LatitudeDeg},
                 {.LongitudeDeg = cut.LongitudeDeg, .LatitudeDeg = cut.LatitudeDeg});
  const auto eastOffset =
      EnuOffsetM({.LongitudeDeg = eastNode->LongitudeDeg, .LatitudeDeg = eastNode->LatitudeDeg},
                 {.LongitudeDeg = cut.LongitudeDeg, .LatitudeDeg = cut.LatitudeDeg});
  constexpr double maximumGradient = 0.05;
  const double westRise = maximumGradient * std::hypot(westOffset.EastM, westOffset.NorthM);
  const double eastRise = maximumGradient * std::hypot(eastOffset.EastM, eastOffset.NorthM);
  const std::array nodes{RoadHeightNode{.LowSampleM = west->AslM,
                                        .HighSampleM = west->AslM,
                                        .MinimumM = west->AslM,
                                        .MaximumM = west->AslM},
                         RoadHeightNode{.LowSampleM = 0, .HighSampleM = 0},
                         RoadHeightNode{.LowSampleM = east->AslM,
                                        .HighSampleM = east->AslM,
                                        .MinimumM = east->AslM,
                                        .MaximumM = east->AslM}};
  const std::array links{RoadHeightLink{.First = 0, .Second = 1, .MaximumRiseM = westRise},
                         RoadHeightLink{.First = 1, .Second = 2, .MaximumRiseM = eastRise}};
  const auto planned = PlanRoadHeights(nodes, links, RoadHeightFit::PreferCuts);
  CHECK(planned && planned->HeightM.size() == nodes.size(),
        "the existing native planner consumes the captured far source conditions");
  if (!planned || planned->HeightM.size() != nodes.size()) { return Report(); }
  const double minimum = std::max(west->AslM - westRise, east->AslM - eastRise);
  const double maximum = std::min(west->AslM + westRise, east->AslM + eastRise);
  CHECK(minimum > 0 && planned->HeightM[0] == west->AslM && planned->HeightM[2] == east->AslM &&
            planned->HeightM[1] >= minimum && planned->HeightM[1] <= maximum,
        "a source-bound height constraint replaces a locally attractive but infeasible zero "
        "without changing either delivered landing");
  std::println("BRIDGE_GROUND_HEIGHT_PLAN west_asl_m={} east_asl_m={} cut_min_m={} cut_max_m={} "
               "planned_cut_m={} sourced_inputs={}",
               west->AslM,
               east->AslM,
               minimum,
               maximum,
               planned->HeightM[1],
               samples->Work.Sourced);
  return Report();
}
