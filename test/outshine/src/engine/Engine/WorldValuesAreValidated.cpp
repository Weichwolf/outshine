#include "ScenarioStartup.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Scenario::Document original;
  CHECK(LoadFreshScenario(original).has_value(), "initial declaration accepted");

  for (const auto field : std::array{&Scenario::WorldSettings::GravityMs2,
                                     &Scenario::WorldSettings::AirDensityKgM3,
                                     &Scenario::WorldSettings::PatienceS,
                                     &Scenario::WorldSettings::SightM}) {
    for (const double value : {-1.0,
                               std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
      auto candidate = original;
      candidate.Ground.*field = value;
      CHECK(!LoadFreshScenario(candidate), "invalid world magnitude refused");
    }
  }
  for (const auto field : std::array{&Scenario::Georeference::LatitudeDeg,
                                     &Scenario::Georeference::LongitudeDeg,
                                     &Scenario::Georeference::RadiusM}) {
    auto candidate = original;
    candidate.Ground.Origin.*field = std::numeric_limits<double>::infinity();
    CHECK(!LoadFreshScenario(candidate), "nonfinite geographic setting refused");
  }
  auto candidate = original;
  candidate.Ground.Origin.LatitudeDeg = 91;
  CHECK(!LoadFreshScenario(candidate), "latitude beyond pole refused");
  candidate = original;
  candidate.Ground.PatienceS = std::numeric_limits<double>::max();
  CHECK(!LoadFreshScenario(candidate), "poll-count overflow refused before conversion");

  candidate.Ground.PatienceS = 0;
  candidate.Ground.SightM = 0;
  candidate.Ground.GravityMs2 = 0;
  candidate.Ground.AirDensityKgM3 = 0;
  CHECK(LoadFreshScenario(candidate).has_value(),
        "zero-valued defaults and vacuum can be declared");
  return Report();
}
