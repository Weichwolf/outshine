#include "RoadHeightPlan.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <limits>

int main() {
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
  return Report();
}
