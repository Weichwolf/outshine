#include "PlantCrownPlan.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

namespace {
outshine::Generators::PlantGrowthPlan Skeleton() {
  return {.PositionsM = {{0, 0, 0}, {0, 0, 1}}, .Parents = {-1, 0}, .Unreached = {}, .Stats = {}};
}
}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double kLengthM = 0.35;
  const PlantCrownRecipe recipe{.LengthM = kLengthM};
  const std::array members{&PlantCrownRecipe::LengthM,
                           &PlantCrownRecipe::BendRad,
                           &PlantCrownRecipe::PetioleM,
                           &PlantCrownRecipe::FoliageDepthM,
                           &PlantCrownRecipe::LeafAngleRad,
                           &PlantCrownRecipe::NormalAlignment};
  for (const auto member : members) {
    auto invalid = recipe;
    invalid.*member = std::numeric_limits<double>::quiet_NaN();
    CHECK(!PlantCrownPlan::Create(Skeleton(), {1}, invalid, 24),
          "every nonfinite physical recipe parameter is refused at the plan boundary");
  }
  CHECK(!PlantCrownPlan::Create(Skeleton(), {64}, recipe, 24),
        "unsupported blade variants do not alias an existing prototype");
  auto broken = Skeleton();
  broken.Parents[1] = 1;
  CHECK(!PlantCrownPlan::Create(std::move(broken), {1}, recipe, 24),
        "cyclic source parents never enter the reverse distance pass");
  broken = Skeleton();
  broken.PositionsM[1] = broken.PositionsM[0];
  CHECK(!PlantCrownPlan::Create(std::move(broken), {1}, recipe, 24),
        "collapsed edges cannot manufacture a host orientation");
  broken = Skeleton();
  broken.Parents.pop_back();
  CHECK(!PlantCrownPlan::Create(std::move(broken), {1}, recipe, 24),
        "source positions and parents must describe the same complete skeleton");
  return Report();
}
