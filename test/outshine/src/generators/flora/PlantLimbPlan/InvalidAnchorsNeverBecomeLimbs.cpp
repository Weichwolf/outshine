#include "Check.h"
#include "PlantLimbPlan.h"
#include <array>
#include <limits>
#include <vector>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array original{
      PlantLimbAnchor{.Node = 0, .PositionM = {{0, 0, 0}}, .RadiusM = 0.1},
      PlantLimbAnchor{.Node = 1, .PositionM = {{0, 1, 0}}, .RadiusM = 0.05},
      PlantLimbAnchor{.Node = 2, .PositionM = {{0, 2, 0}}, .RadiusM = 0.02}};
  CHECK(!PlantLimbPlan::Create({}) && !PlantLimbPlan::Create(std::span(original).first(1)),
        "a limb requires at least one actual source interval");
  CHECK(!PlantLimbPlan::Create(std::vector<PlantLimbAnchor>(PlantLimbPlan::MostAnchors + 1)),
        "oversized plans fail before coefficient allocation");
  auto broken = original;
  broken[2].Node = broken[0].Node;
  CHECK(!PlantLimbPlan::Create(broken), "a repeated source node cannot become a cyclic limb");
  broken = original;
  broken[1].PositionM = broken[0].PositionM;
  CHECK(!PlantLimbPlan::Create(broken), "collapsed intervals have no invented tangent");
  broken = original;
  broken[2].PositionM = broken[0].PositionM;
  CHECK(!PlantLimbPlan::Create(broken), "an antiparallel turn has no invented bisector");
  broken = original;
  broken[1].RadiusM = 0;
  CHECK(!PlantLimbPlan::Create(broken), "zero radius cannot produce an oriented closed tube");
  broken = original;
  broken[1].PositionM[0] = std::numeric_limits<double>::infinity();
  CHECK(!PlantLimbPlan::Create(broken), "nonfinite source positions fail at the input boundary");
  const auto plan = PlantLimbPlan::Create(original);
  CHECK(plan.has_value(), "control plan remains valid");
  if (!plan) { return Report(); }
  CHECK(!plan->Sample(-1) && !plan->Sample(2) &&
            !plan->Sample(std::numeric_limits<double>::quiet_NaN()),
        "out-of-domain samples never extrapolate source geometry");
  return Report();
}
