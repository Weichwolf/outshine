#include "Prismatic.h"

#include <cmath>
#include <limits>

#include "Check.h"

int main() {
  using namespace outshine::Physics;
  using namespace outshine::Test;
  const auto near = [](double a, double b) { return std::abs(a - b) < 1e-12; };

  PrismaticJoint joint{.ReachM = 1.0,
                       .StiffnessNPerM = 1000.0,
                       .DampingNsPerM = 100.0,
                       .TravelM = 0.2,
                       .StopStiffnessNPerM = 5000.0,
                       .LoadLimitN = 400.0};
  CHECK(ValidatePrismaticJoint(joint).has_value(), "finite nonnegative joint is valid");

  const Reaction elastic = Press(joint, {.ClearanceM = 0.9, .ClosingMs = 2.0});
  CHECK(elastic.Touching && near(elastic.PressedM, 0.1) && near(elastic.ElasticN, 100.0) &&
            near(elastic.DampingN, 200.0) && elastic.StopN == 0.0 && near(elastic.LoadN, 300.0) &&
            !elastic.PastTravel && !elastic.PastLimit,
        "spring and damping loads add below travel");

  const Reaction stopped = Press(joint, {.ClearanceM = 0.7});
  CHECK(stopped.Touching && stopped.PastTravel && stopped.PastLimit &&
            near(stopped.ElasticN, 200.0) && near(stopped.StopN, 500.0) &&
            near(stopped.LoadN, 700.0),
        "spring load clamps at travel and the stop carries excess compression");
  CHECK(near(PressedForM(joint, 100.0), 0.1) && near(PressedForM(joint, 700.0), 0.3),
        "inverse compression agrees below and beyond travel");

  joint.LoadLimitN = std::numeric_limits<double>::quiet_NaN();
  CHECK(!ValidatePrismaticJoint(joint), "nonfinite joint parameter is rejected");
  return Report();
}
