#include "RoadHeightPlan.h"
#include "Check.h"

#include <array>
#include <cmath>

int main() {
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
  return Report();
}
