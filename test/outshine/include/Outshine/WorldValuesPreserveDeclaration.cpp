#include <Outshine.h>
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Scenario::Document original;
  CHECK(engine.declare(original).has_value(), "initial declaration accepted");
  const auto before = engine.writeScenario();
  for (const auto field : std::array{&Scenario::WorldSettings::GravityMs2,
                                     &Scenario::WorldSettings::AirDensityKgM3,
                                     &Scenario::WorldSettings::PatienceS,
                                     &Scenario::WorldSettings::SightM}) {
    for (const double value : {-1.0,
                               std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
      auto candidate = original;
      candidate.Ground.*field = value;
      CHECK(!engine.declare(candidate), "invalid world magnitude refused");
      CHECK(engine.writeScenario() == before, "invalid magnitude preserves declaration");
    }
  }
  for (const auto field : std::array{&Scenario::Georeference::LatitudeDeg,
                                     &Scenario::Georeference::LongitudeDeg,
                                     &Scenario::Georeference::RadiusM}) {
    auto candidate = original;
    candidate.Ground.Origin.*field = std::numeric_limits<double>::infinity();
    CHECK(!engine.declare(candidate), "nonfinite geographic setting refused");
    CHECK(engine.writeScenario() == before, "invalid origin preserves declaration");
  }
  auto candidate = original;
  candidate.Ground.Origin.LatitudeDeg = 91;
  CHECK(!engine.declare(candidate), "latitude beyond pole refused");
  candidate = original;
  candidate.Ground.PatienceS = std::numeric_limits<double>::max();
  CHECK(!engine.declare(candidate), "poll-count overflow refused before conversion");
  CHECK(engine.writeScenario() == before, "budget failure preserves declaration");
  candidate.Ground.PatienceS = 0;
  candidate.Ground.SightM = 0;
  candidate.Ground.GravityMs2 = 0;
  candidate.Ground.AirDensityKgM3 = 0;
  CHECK(engine.declare(candidate).has_value(), "zero-valued defaults and vacuum can be declared");
  return Report();
}
