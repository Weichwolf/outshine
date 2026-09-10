#include <Outshine.h>
#include <generation/Generate.h>
#include "Check.h"
#include <limits>
#include <string>

namespace {
class Probe final : public outshine::Generators::Generator {
public:
  mutable int Calls = 0;

  std::string_view kind() const override { return "native-osm-probe"; }

  bool make(const outshine::Generators::Request &, outshine::Geometry &) const override {
    ++Calls;
    return true;
  }
};

class Receiver final : public outshine::Host {
public:
  std::string Last;

  bool calls(std::string_view name, std::span<const outshine::Argument>) override {
    Last = name;
    return true;
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Receiver receiver;
  Probe probe;
  Engine engine;
  engine.offers(&receiver);
  engine.offers(probe);
  const Scenario::Structure valid{.Kind = "track", .LatLon = {0, 0, 1, 1}};
  Scenario::Document original;
  original.Input = {{.Event = "KeyW", .Action = "original"}};
  for (const bool world : {false, true}) {
    for (int bad = 0; bad < 11; ++bad) {
      CHECK(engine.declare(original).has_value(), "original declaration activates");
      const auto previous = engine.writeScenario();
      const int calls = probe.Calls;
      auto candidate = original;
      candidate.Input.front().Action = "replacement";
      candidate.Motion.Declared = true;
      candidate.Motion.StepS = 0.125;
      candidate.Ground.Declared = world;
      candidate.Generators.push_back({.Kind = "native-osm-probe"});
      candidate.Ground.Osm = {valid, valid};
      auto &feature = candidate.Ground.Osm.back();
      switch (bad) {
        case 0: feature.Kind.clear(); break;
        case 1: feature.WidthM = std::numeric_limits<double>::quiet_NaN(); break;
        case 2: feature.HeightM = -1; break;
        case 3: feature.LatLon.pop_back(); break;
        case 4: feature.LatLon.resize(2); break;
        case 5: feature.Area = true; break;
        case 6: feature.LatLon[0] = 91; break;
        case 7: feature.LatLon[1] = 181; break;
        case 8: feature.LatLon[2] = std::numeric_limits<double>::quiet_NaN(); break;
        case 9: feature.LatLon[3] = std::numeric_limits<double>::infinity(); break;
        default: feature.LatLon.resize(2 * 65537); break;
      }
      const auto declared = engine.declare(candidate);
      CHECK(!declared && !declared.error().empty(), "native malformed OSM is diagnosed");
      CHECK(engine.writeScenario() == previous && probe.Calls == calls,
            "validation precedes document publication and producer execution");
      SDL_Event event{};
      event.type = SDL_EVENT_KEY_DOWN;
      event.key.key = SDLK_W;
      receiver.Last.clear();
      CHECK(engine.handleEvent(event).value_or(false) && receiver.Last == "original",
            "failure preserves active input bindings");
    }
  }
  Scenario::Document accepted;
  accepted.Ground.Osm = {valid,
                         {.Kind = "building", .Area = true, .LatLon = {-90, -180, 90, 180, 0, 0}}};
  CHECK(engine.declare(accepted).has_value(),
        "valid native ways and areas pass angular boundaries");
  accepted.Ground.Osm.front().LatLon.resize(2 * 65536);
  CHECK(engine.declare(accepted).has_value(), "the native path accepts the exact point budget");
  return Report();
}
