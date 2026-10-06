#include "PlanHierarchy.h"
#include "Check.h"
#include "scene/ProjectedErrorBudget.h"

#include <cmath>
#include <limits>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::vector<Box> bounds;
  for (size_t at = 0; at < 128; ++at) {
    const double x = static_cast<double>(at) * 5;
    bounds.push_back({.Min = {{x, 0, 0}}, .Max = {{x + 4, 8, 4}}});
  }
  const auto hierarchy = PlanHierarchy::Build(bounds);
  CHECK(hierarchy && hierarchy->Nodes().size() == bounds.size() * 2 - 1,
        "packed nodes contain source bounds and index ranges without render vertices");
  if (!hierarchy) { return Report(); }
  size_t priorGroups = bounds.size() + 1;
  for (double distance : {100.0, 1000.0, 10000.0, 100000.0}) {
    std::vector<size_t> visits(bounds.size());
    size_t groups = 0;
    const ProjectedErrorBudget quality{.FocalPx = 720};
    hierarchy->Select(
        [&](const Box &box) {
          const auto span = box.Span();
          return quality.Allows(std::hypot(std::hypot(span[0], span[1]), span[2]), distance);
        },
        [&](const PlanHierarchy::Node &node) {
          ++groups;
          for (uint32_t member : hierarchy->Members(node)) {
            ++visits[member];
            CHECK(node.Bounds.Holds(bounds[member].Min) && node.Bounds.Holds(bounds[member].Max),
                  "selected parent contains every source bound it replaces");
          }
        });
    for (size_t visit : visits) { CHECK(visit == 1, "all sources are represented exactly once"); }
    CHECK(groups <= priorGroups, "identical sources require no more products at greater distance");
    priorGroups = groups;
  }
  CHECK(priorGroups < bounds.size() / 4,
        "far selection replaces many sources before mesh generation");
  auto empty = PlanHierarchy::Build({});
  CHECK(empty && empty->Nodes().empty(), "valid empty input has no products");
  bounds.front().Min[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!PlanHierarchy::Build(bounds), "nonfinite source bounds are rejected");
  return Report();
}
