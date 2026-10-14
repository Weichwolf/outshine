#include "RoadHeightPlan.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <limits>

namespace {
void HeightsRespectGradientsAndContactBounds() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::array nodes{RoadHeightNode{0, 0}, RoadHeightNode{20, 20}, RoadHeightNode{0, 0}};
  constexpr std::array links{RoadHeightLink{0, 1, 5}, RoadHeightLink{1, 2, 5}};
  auto plan = PlanRoadHeights(nodes, links);
  CHECK(plan.has_value(), "connected roads admit a joint grade-constrained height plan");
  if (plan) {
    CHECK_NEAR(plan->MaximumAdjustmentM,
               (20.0 - 5.0) / 2.0,
               1e-9,
               "m",
               "the largest unavoidable sample displacement is attained");
    CHECK_NEAR(plan->HeightM[0], 7.5, 1e-9, "m", "the first bank shares the minimax correction");
    CHECK_NEAR(plan->HeightM[1], 12.5, 1e-9, "m", "the spike is reduced to the allowed rise");
    CHECK_NEAR(plan->HeightM[2], 7.5, 1e-9, "m", "the opposite bank is solved jointly");
  }
  nodes[0].MinimumM = nodes[0].MaximumM = 0;
  plan = PlanRoadHeights(nodes, links);
  CHECK(plan.has_value(), "a fixed existing contact remains a hard constraint");
  if (plan) {
    CHECK_NEAR(plan->HeightM[0], 0, 1e-9, "m", "the fixed contact is preserved");
    CHECK_NEAR(plan->HeightM[1], 5, 1e-9, "m", "the connected peak obeys the fixed bank");
    CHECK_NEAR(
        plan->MaximumAdjustmentM, 20.0 - 5.0, 1e-9, "m", "the fixed-bank lower bound is attained");
  }
  nodes[2].MinimumM = nodes[2].MaximumM = 20;
  CHECK(!PlanRoadHeights(nodes, links), "incompatible fixed heights are refused explicitly");
  nodes = {RoadHeightNode{0, 0}, RoadHeightNode{0, 0, 30}, RoadHeightNode{0, 0}};
  plan = PlanRoadHeights(nodes, links);
  CHECK(plan.has_value(), "bridge clearance propagates into its approaches");
  if (plan) {
    CHECK_NEAR(plan->HeightM[1], 30, 1e-9, "m", "clearance adds no unnecessary deck height");
    CHECK_NEAR(plan->HeightM[0], 25, 1e-9, "m", "the first approach adds only required fill");
    CHECK_NEAR(plan->HeightM[2], 25, 1e-9, "m", "the opposite approach adds only required fill");
    for (const auto &link : links) {
      CHECK(std::abs(plan->HeightM[link.First] - plan->HeightM[link.Second]) <=
                link.MaximumRiseM + 1e-9,
            "the bridge and both approaches respect their allowed rises");
    }
  }
  constexpr std::array offsetNodes{RoadHeightNode{0, 0}, RoadHeightNode{0, 0}};
  constexpr std::array offsetLinks{RoadHeightLink{0, 1, 2, 3, 0}};
  plan = PlanRoadHeights(offsetNodes, offsetLinks);
  CHECK(plan.has_value(), "a tilted junction port participates in the height solve");
  if (plan) {
    CHECK_NEAR(plan->MaximumAdjustmentM,
               (3.0 - 2.0) / 2.0,
               1e-9,
               "m",
               "relative attachment offsets are included in the optimal displacement");
    CHECK_NEAR(plan->HeightM[0], -.5, 1e-9, "m", "the junction centre moves coherently");
    CHECK_NEAR(plan->HeightM[1], .5, 1e-9, "m", "the road meets the tilted port");
  }
  constexpr std::array impossibleOffsets{RoadHeightLink{0, 1, 0, 1, 0},
                                         RoadHeightLink{0, 1, 0, 0, 0}};
  CHECK(!PlanRoadHeights(offsetNodes, impossibleOffsets),
        "contradictory attachment offsets cannot cycle indefinitely");
  std::array crossingPorts{RoadHeightLink{0, 1, 1, 3, 0}, RoadHeightLink{0, 1, 1, 0, 0}};
  const auto conflict = PlanRoadHeights(offsetNodes, crossingPorts);
  CHECK(!conflict, "incompatible junction ports are returned as a constructive conflict");
  if (!conflict) {
    CHECK(conflict.error().CycleNodes.size() == 2,
          "the conflict names only the two participating height nodes");
    CHECK_NEAR(conflict.error().MaximumOffsetScale,
               2.0 / 3.0,
               1e-9,
               "scale",
               "two metres of rise budget limit three metres of opposing port offsets");
    crossingPorts[0].FirstOffsetM *= conflict.error().MaximumOffsetScale;
    const auto resolved = PlanRoadHeights(offsetNodes, crossingPorts);
    CHECK(resolved.has_value(), "the exact reported attachment bound closes the conflict");
    if (resolved) {
      CHECK_NEAR(resolved->MaximumAdjustmentM,
                 .5,
                 1e-9,
                 "m",
                 "resolved ports still attain the optimal height displacement");
    }
  }
  nodes[0].LowSampleM = std::numeric_limits<double>::quiet_NaN();
  CHECK(!PlanRoadHeights(nodes, links), "nonfinite source heights are refused");
  CHECK(PlanRoadHeights({}, {})->HeightM.empty(), "an empty network has an empty plan");
}

void CutsAvoidUnnecessaryTerrainFill() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::array nodes{RoadHeightNode{0, 0}, RoadHeightNode{20, 20}, RoadHeightNode{0, 0}};
  constexpr std::array links{RoadHeightLink{0, 1, 5}, RoadHeightLink{1, 2, 5}};
  auto plan = PlanRoadHeights(nodes, links, RoadHeightFit::PreferCuts);
  CHECK(plan.has_value(), "a ground cut closes the profile without raising either bank");
  if (plan) {
    CHECK_NEAR(plan->HeightM[0], 0, 1e-9, "m", "the first bank receives no unnecessary fill");
    CHECK_NEAR(plan->HeightM[1], 5, 1e-9, "m", "the peak keeps its greatest permitted height");
    CHECK_NEAR(plan->HeightM[2], 0, 1e-9, "m", "the opposite bank receives no unnecessary fill");
    CHECK_NEAR(plan->MaximumAdjustmentM, 20.0 - 5.0, 1e-9, "m", "the actual cut is reported");
  }
  nodes = {RoadHeightNode{20, 20}, RoadHeightNode{0, 0, 10}, RoadHeightNode{20, 20}};
  plan = PlanRoadHeights(nodes, links, RoadHeightFit::PreferCuts);
  CHECK(plan.has_value(), "a required raised contact does not deepen neighbouring cuts");
  if (plan) {
    CHECK_NEAR(
        plan->HeightM[0], 15, 1e-9, "m", "the first bank retains its greatest feasible height");
    CHECK_NEAR(plan->HeightM[1], 10, 1e-9, "m", "only the required contact is raised");
    CHECK_NEAR(plan->HeightM[2], 15, 1e-9, "m", "the opposite bank needs no deeper cut");
  }
  nodes = {RoadHeightNode{0, 0}, RoadHeightNode{0, 0, 30}, RoadHeightNode{0, 0}};
  plan = PlanRoadHeights(nodes, links, RoadHeightFit::PreferCuts);
  CHECK(plan.has_value(), "required bridge clearance takes precedence over cut preference");
  if (plan) {
    CHECK_NEAR(plan->HeightM[1], 30, 1e-9, "m", "the required deck clearance is preserved");
    CHECK_NEAR(plan->HeightM[0], 25, 1e-9, "m", "the first bank gets only the required fill");
    CHECK_NEAR(plan->HeightM[2], 25, 1e-9, "m", "the opposite bank gets only the required fill");
  }
  nodes[0].MinimumM = nodes[0].MaximumM = 0;
  CHECK(!PlanRoadHeights(nodes, links, RoadHeightFit::PreferCuts),
        "cut preference cannot bypass incompatible physical contacts");
}

void ClearancesPreserveHardContactsAndLevelOrder() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::array nodes{RoadHeightNode{0, 0}, RoadHeightNode{0, 0}};
  constexpr std::array clearances{RoadHeightClearance{0, 1, 5}};
  auto plan = PlanRoadHeights(nodes, {}, clearances);
  CHECK(plan.has_value(), "distinct levels admit a directed clearance plan");
  if (plan) {
    CHECK_NEAR(plan->HeightM[0], -2.5, 1e-9, "m", "the lower level shares the minimax correction");
    CHECK_NEAR(plan->HeightM[1], 2.5, 1e-9, "m", "the upper level shares the minimax correction");
  }
  plan = PlanRoadHeights(nodes, {}, clearances, RoadHeightFit::PreferCuts);
  CHECK(plan.has_value(), "cut preference accepts one-way height constraints");
  if (plan) {
    CHECK_NEAR(plan->HeightM[0], -5, 1e-9, "m", "only the required lower excavation is made");
    CHECK_NEAR(plan->HeightM[1], 0, 1e-9, "m", "the upper level is not needlessly raised");
  }
  nodes[0].MinimumM = nodes[0].MaximumM = 0;
  plan = PlanRoadHeights(nodes, {}, clearances, RoadHeightFit::PreferCuts);
  CHECK(plan.has_value(), "a fixed lower road forces only the required upper fill");
  if (plan) {
    CHECK_NEAR(plan->HeightM[0], 0, 1e-9, "m", "the existing lower contact remains fixed");
    CHECK_NEAR(plan->HeightM[1], 5, 1e-9, "m", "the upper deck clears the fixed road");
  }
  constexpr std::array offsetClearances{RoadHeightClearance{0, 1, 5, 1, .5}};
  plan = PlanRoadHeights(nodes, {}, offsetClearances, RoadHeightFit::PreferCuts);
  CHECK(plan.has_value(), "sloped port offsets participate in the same clearance constraint");
  if (plan) {
    CHECK_NEAR(plan->HeightM[1], 5.5, 1e-9, "m", "the physical port heights retain five metres");
  }
  nodes[1].MaximumM = 2;
  CHECK(!PlanRoadHeights(nodes, {}, clearances), "incompatible fixed clearance is refused");
  constexpr std::array cycle{RoadHeightClearance{0, 1, 1}, RoadHeightClearance{1, 0, 1}};
  nodes = {RoadHeightNode{0, 0}, RoadHeightNode{0, 0}};
  const auto conflict = PlanRoadHeights(nodes, {}, cycle);
  CHECK(!conflict && !conflict.error().CycleNodes.empty(),
        "an impossible level order reports its physical conflict cycle");
  constexpr std::array invalid{RoadHeightClearance{0, 1, -1}};
  CHECK(!PlanRoadHeights(nodes, {}, invalid), "negative clearance is rejected at the boundary");
}

void ConflictCyclesIdentifyTheirDirectedConstraints() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array nodes{
      RoadHeightNode{160, 160}, RoadHeightNode{160, 160}, RoadHeightNode{160, 160}};
  const std::array offsetLoop{
      RoadHeightLink{0, 1, 1, 0, .4}, RoadHeightLink{1, 2, 1, 4.5, 0}, RoadHeightLink{2, 0, 1}};
  const std::array shortLink{RoadHeightLink{0, 1, 1}};
  const std::array gap{RoadHeightClearance{0, 1, 2}};
  for (const bool withClearance : {false, true}) {
    const auto links = withClearance ? std::span<const RoadHeightLink>(shortLink)
                                     : std::span<const RoadHeightLink>(offsetLoop);
    const auto clearances = withClearance ? std::span<const RoadHeightClearance>(gap)
                                          : std::span<const RoadHeightClearance>{};
    const auto plan = PlanRoadHeights(nodes, links, clearances, RoadHeightFit::PreferCuts);
    CHECK(!plan, "a contradictory physical height cycle remains a refusal");
    if (plan) { continue; }
    const auto &failure = plan.error();
    CHECK(!failure.CycleConstraints.empty() &&
              failure.CycleConstraints.size() == failure.CycleNodes.size(),
          "every reported cycle node names its actual directed constraint");
    double budgetM = 0;
    for (const auto &constraint : failure.CycleConstraints) {
      if (constraint.Kind == RoadHeightConstraintKind::Clearance) {
        CHECK(constraint.Index < clearances.size(), "a clearance refers to the supplied crossing");
        if (constraint.Index < clearances.size()) {
          budgetM -= clearances[constraint.Index].MinimumGapM;
        }
      } else {
        CHECK(constraint.Index < links.size(), "a link refers to the supplied profile");
        if (constraint.Index >= links.size()) { continue; }
        const auto &link = links[constraint.Index];
        const double sign = constraint.Kind == RoadHeightConstraintKind::ForwardLink ? 1 : -1;
        budgetM += link.MaximumRiseM + sign * (link.FirstOffsetM - link.SecondOffsetM);
      }
    }
    CHECK(budgetM < 0, "the directed constraints reproduce the reported negative cycle");
  }
  const std::array self{RoadHeightLink{1, 1, 0}};
  CHECK(PlanRoadHeights(nodes, self, RoadHeightFit::PreferCuts).has_value(),
        "a zero-length self-link does not impose an absolute height bound");
}

}

int main() {
  HeightsRespectGradientsAndContactBounds();
  CutsAvoidUnnecessaryTerrainFill();
  ClearancesPreserveHardContactsAndLevelOrder();
  ConflictCyclesIdentifyTheirDirectedConstraints();
  return outshine::Test::Report();
}
