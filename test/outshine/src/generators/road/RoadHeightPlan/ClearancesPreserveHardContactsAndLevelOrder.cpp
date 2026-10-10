#include "RoadHeightPlan.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <limits>

int main() {
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
  return Report();
}
