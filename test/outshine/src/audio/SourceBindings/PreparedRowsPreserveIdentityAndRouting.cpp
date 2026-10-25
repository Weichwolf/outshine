#include "SourceBindings.h"
#include "Mixer.h"
#include "BusGraph.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

int main() {
  using namespace outshine::Audio;
  using namespace outshine::Test;
  std::array<MixBus, 2> buses{};
  buses[0].Id = "master";
  buses[0].GainDb = -6;
  buses[1].Id = "effects";
  buses[1].Output = "master";
  buses[1].GainDb = -12;
  std::array<SoundSource, 2> declared{};
  declared[0].Id = "engine";
  declared[0].Bus = "effects";
  declared[0].GainDb = 3;
  declared[1].Id = "wind";
  BusGraph routing;
  CHECK(routing.Build(buses, declared), "routing is valid");
  SourceBindings bindings;
  bindings.Prepare(declared, routing);
  constexpr double kDecibelBase = 10.0;
  constexpr double kAmplitudeDecibels = 20.0;
  constexpr double kGainTolerance = 1e-15;
  const double routedDb = buses[0].GainDb + buses[1].GainDb + declared[0].GainDb;
  CHECK(std::abs(bindings.Gain(0) - std::pow(kDecibelBase, routedDb / kAmplitudeDecibels)) <
                kGainTolerance &&
            std::abs(bindings.Gain(1) -
                     std::pow(kDecibelBase, buses[0].GainDb / kAmplitudeDecibels)) < kGainTolerance,
        "prepared gain includes every bus and source exactly once");
  std::array<Heard, 6> heard{};
  heard[0].Id = "wind";
  heard[1].Id = "engine";
  heard[2].Id = "unknown";
  heard[3].Id = "engine";
  heard[4].Id = "wind";
  heard[5].Id = "engine";
  for (size_t row = 0; row < 4; ++row) { heard[row].Standing = true; }
  const auto resolved = bindings.Resolve(heard);
  CHECK(resolved.size() == 2 && resolved[0] == &heard[3] && resolved[1] == heard.data(),
        "names bind independent of order, last active wins, inactive and unknown do not replace");
  CHECK(bindings.Cost().ClearedRows == 2 && bindings.Cost().IndexLookups == 4,
        "only active input rows query the prepared identity index");
  const auto empty = bindings.Resolve({});
  CHECK(empty[0] == nullptr && empty[1] == nullptr, "absent sources clear the previous binding");
  bindings.Prepare(std::span(declared).subspan(1), routing);
  const auto replaced = bindings.Resolve(heard);
  CHECK(replaced.size() == 1 && replaced[0] == heard.data(),
        "reconfiguration replaces identities and their row numbers");
  bindings.Prepare({}, routing);
  CHECK(bindings.Resolve(heard).empty(), "empty configuration retains no old bindings");
  return Report();
}
