#include "PrismaticJointResponse.h"

#include <cmath>
#include <limits>

#include "Check.h"

int main() {
  using namespace outshine::Physics;
  using namespace outshine::Test;
  constexpr double tolerance = 1e-12;
  const auto near = [](double a, double b) { return std::abs(a - b) < tolerance; };

  constexpr PrismaticJoint valid{.ReachM = 1.0,
                                 .StiffnessNPerM = 1000.0,
                                 .DampingNsPerM = 100.0,
                                 .TravelM = 0.2,
                                 .StopStiffnessNPerM = 5000.0,
                                 .LoadLimitN = 400.0};
  auto joint = valid;
  constexpr Reaction elasticExpected{
      .PressedM = 0.1, .LoadN = 300, .ElasticN = 100, .DampingN = 200};
  constexpr Reaction stopExpected{.PressedM = 0.3, .LoadN = 700, .ElasticN = 200, .StopN = 500};
  CHECK(ValidatePrismaticJoint(joint).has_value(), "finite nonnegative joint is valid");

  const Reaction elastic = Press(joint, {.ClearanceM = 0.9, .ClosingMs = 2.0});
  CHECK(elastic.Touching && near(elastic.PressedM, elasticExpected.PressedM) &&
            near(elastic.ElasticN, elasticExpected.ElasticN) &&
            near(elastic.DampingN, elasticExpected.DampingN) && elastic.StopN == 0.0 &&
            near(elastic.LoadN, elasticExpected.LoadN) && !elastic.PastTravel && !elastic.PastLimit,
        "spring and damping loads add below travel");

  const Reaction stopped = Press(joint, {.ClearanceM = 0.7});
  CHECK(stopped.Touching && stopped.PastTravel && stopped.PastLimit &&
            near(stopped.ElasticN, stopExpected.ElasticN) &&
            near(stopped.StopN, stopExpected.StopN) && near(stopped.LoadN, stopExpected.LoadN),
        "spring load clamps at travel and the stop carries excess compression");
  CHECK(near(PressedForM(joint, elasticExpected.ElasticN), elasticExpected.PressedM) &&
            near(PressedForM(joint, stopExpected.LoadN), stopExpected.PressedM),
        "inverse compression agrees below and beyond travel");

  joint.LoadLimitN = std::numeric_limits<double>::quiet_NaN();
  CHECK(!ValidatePrismaticJoint(joint), "nonfinite joint parameter is rejected");
  return Report();
}
