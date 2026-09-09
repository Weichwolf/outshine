#include "BusGraph.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Audio::BusGraph graph;
  std::string error;
  std::array<Scenario::Bus, 2> buses{};
  buses[0].Id = "master";
  buses[1].Id = "effects";
  buses[1].Into = "master";
  buses[1].GainDb = -20;
  std::array<Scenario::Sound, 1> sounds{};
  sounds[0].Id = "bell";
  sounds[0].Bus = "effects";
  sounds[0].GainDb = -20;
  CHECK(graph.Build(buses, sounds).has_value(), "valid routing builds");
  CHECK(graph.Play("bell", error), "voice starts");
  CHECK(std::abs(graph.GainOf("bell") - 0.01) < 1e-12,
        "two minus-20 dB stages multiply to 0.01 amplitude");
  const auto preserved = [&] {
    CHECK(graph.Master() == "master" && graph.BusCount() == 2 && graph.SoundCount() == 1 &&
              graph.Playing() == 1 && std::abs(graph.GainOf("bell") - 0.01) < 1e-12,
          "failed replacement preserves routing and voice count");
  };
  auto badBuses = buses;
  badBuses[1].Into = "missing";
  CHECK(!graph.Build(badBuses, sounds), "unknown route fails");
  preserved();
  badBuses = buses;
  badBuses[1].Into = "effects";
  CHECK(!graph.Build(badBuses, sounds), "self-cycle fails");
  preserved();
  badBuses = buses;
  badBuses[1].Id = "master";
  CHECK(!graph.Build(badBuses, sounds), "duplicate bus fails");
  preserved();
  for (double invalid : {std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN(),
                         100000.0}) {
    badBuses = buses;
    badBuses[1].GainDb = invalid;
    CHECK(!graph.Build(badBuses, sounds), "nonfinite or overflowing bus gain fails");
    preserved();
  }
  auto badSounds = sounds;
  badSounds[0].Bus = "missing";
  CHECK(!graph.Build(buses, badSounds), "unknown sound route fails");
  preserved();
  badSounds = sounds;
  badSounds[0].GainDb = std::numeric_limits<double>::quiet_NaN();
  CHECK(!graph.Build(buses, badSounds), "nonfinite sound gain fails");
  preserved();
  badBuses = buses;
  badBuses[0].GainDb = badBuses[1].GainDb = 4000;
  CHECK(!graph.Build(badBuses, sounds), "finite factors whose product overflows fail");
  preserved();
  badSounds = sounds;
  badSounds[0].Heard.Positional = true;
  badSounds[0].Heard.RefM = std::numeric_limits<double>::infinity();
  CHECK(!graph.Build(buses, badSounds), "positional reference distance must be finite");
  preserved();
  const std::vector<Scenario::Bus> tooManyBuses(65);
  const std::vector<Scenario::Sound> tooManySounds(1025);
  CHECK(!graph.Build(tooManyBuses, sounds) && !graph.Build(buses, tooManySounds),
        "capacity exhaustion fails without publishing");
  preserved();
  CHECK(graph.Build(buses, sounds) && graph.Playing() == 0, "successful replacement resets voices");
  return Report();
}
