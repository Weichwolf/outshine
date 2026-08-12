/* WEB MERCATOR ENDS AT ±85.05112877980659° AND A STANDPOINT CANNOT EXIST OUTSIDE IT. The band was
 * checked in the scene reader before this round, which left the refusal a rule a second reader could
 * forget; it is a factory now, so a place outside the band is a value nobody can hold and every
 * consumer of a world stage has one by construction.
 *
 * The window in which the old code fabricated tile (0,0) rather than refusing was
 * 85.051128779807 < lat <= 85.053023927135, 211.7 m wide — which is why the boundary is checked at
 * the digit and not at a round number. */
#include "Check.h"
#include "Mod.h"
#include "Standpoint.h"

#include <string>

using namespace outshine;
using namespace outshine::Test;

namespace {

std::string WorldAt(const char *lat) {
  return std::string(R"({
  "schema": "outshine/mod/1", "name": "t", "subject": "one world scene",
  "scenes": [{ "id": "s", "kind": "run", "fovDeg": 60,
    "stage": { "world": {
      "lat": )") + lat + R"(, "lon": 9.43453, "eyeM": 1.7, "yawDeg": 280, "pitchDeg": 0,
      "utc": "2026-08-06T17:40:00Z", "windDeg": 250, "windMs": 6.0, "cloudCover": 0.55 } },
    "runs": [{ "kind": "motion", "frames": 1, "give": "stills", "path": "walk.png" }] }] })";
}

[[nodiscard]] bool Loads(const char *lat) {
  Scenario::Mod mod;
  return mod.Read(WorldAt(lat), "t.json");
}

}  // namespace

int main() {
  Covers("I.4 a standpoint the tile scheme cannot carry is refused by name");

  CHECK(Scenario::Standpoint::At(0.0, 0.0).has_value(), "the null island is a standpoint");
  CHECK(Scenario::Standpoint::At(85.0511, 9.0).has_value(),
        "the last latitude inside the band is a standpoint");
  CHECK(!Scenario::Standpoint::At(85.0512, 9.0).has_value(),
        "the first latitude outside it is not a value at all");
  CHECK(!Scenario::Standpoint::At(86.0, 9.0).has_value(), "and neither is 86 N");
  CHECK(!Scenario::Standpoint::At(0.0, 180.1).has_value(),
        "a longitude off the map is refused by the same factory");

  CHECK(Loads("78.2"), "the control inside the band loads");
  CHECK(Loads("85.0511"), "and so does the last latitude inside it");
  CHECK(!Loads("85.0525"), "a scene at 85.0525 N is refused");
  CHECK(!Loads("85.0530"), "and so is 85.0530 N, inside the old 211.7 m fabrication window");
  CHECK(!Loads("86.0"), "and so is 86 N");

  Scenario::Mod mod;
  CHECK(!mod.Read(WorldAt("86.0"), "t.json") &&
            mod.Error().find("Mercator") != std::string::npos &&
            mod.Error().find("stage.world.lat") != std::string::npos,
        "and the refusal names both the property and the band");
  std::printf("NOTE band refusal: %s\n", mod.Error().c_str());
  return Report();
}
