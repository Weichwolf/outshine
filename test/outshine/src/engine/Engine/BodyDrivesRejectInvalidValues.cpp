#include "ScenarioStartup.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document source;
  source.Room = 1;
  Scenario::Body body;
  body.Name = "kept";
  body.Placed = true;
  body.MassKg = 1;
  source.Bodies.push_back(body);
  source.Bodies[0].Driven.push_back({});
  CHECK(LoadFreshScenario(source), "valid actuator assembles");
  const auto reject = [&](const Scenario::Drive &drive) {
    auto candidate = source;
    candidate.Bodies[0].Name = "rejected";
    candidate.Bodies[0].Driven[0] = drive;
    CHECK(!LoadFreshScenario(candidate),
          "invalid actuator rejected before declaration publication");
  };
  auto bad = Scenario::Drive{};
  constexpr int kInvalidDriveCategory = 255;
  bad.Does = static_cast<Scenario::Drives>(kInvalidDriveCategory);
  reject(bad);
  for (const double number :
       {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    for (const auto field :
         std::array{&Scenario::Drive::PeakNm, &Scenario::Drive::PeakN, &Scenario::Drive::CircleM}) {
      bad = {};
      bad.*field = number;
      reject(bad);
    }
  }
  for (const double number :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    bad = {};
    bad.Ratio = number;
    reject(bad);
    for (int axis = 0; axis < 3; ++axis) {
      bad = {};
      bad.AxisXyz[axis] = number;
      reject(bad);
    }
  }
  bad = {};
  bad.AxisXyz = {0, 0, 0};
  reject(bad);
  bad = {};
  bad.PeakN = 1;
  reject(bad);
  bad = {};
  bad.Turns = false;
  bad.PeakNm = 1;
  reject(bad);
  for (const auto category : {Scenario::Drives::Effort, Scenario::Drives::Motion}) {
    for (const bool turns : {false, true}) {
      source.Bodies[0].Driven[0] = {.Does = category, .Opposes = true, .Turns = turns, .Ratio = -2};
      CHECK(LoadFreshScenario(source),
            "both categories accept either zero-magnitude mode and negative ratio");
    }
  }
  source.Bodies[0].Driven[0].Ratio = 0;
  CHECK(LoadFreshScenario(source), "zero transmission ratio remains representable");
  return Report();
}
