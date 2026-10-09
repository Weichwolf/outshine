#include "RoadHeightPlan.h"
#include "Check.h"

#include <array>
#include <cmath>

int main() {
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
  return Report();
}
