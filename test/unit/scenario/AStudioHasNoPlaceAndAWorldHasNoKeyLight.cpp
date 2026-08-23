#include "Check.h"
#include "Mod.h"

#include <string>

using namespace outshine;
using namespace outshine::Test;

namespace {

const char *kStudio = R"({
  "schema": "outshine/mod/1", "name": "t", "subject": "one studio subject",
  "scenes": [{ "id": "s", "kind": "run", "fovDeg": 30,
    "stage": { "studio": {
      "substrate": { "groundAslM": 100.6 },
      "keyLight": { "elevationDeg": 11.0 },
      "subject": { "generator": "tree", "species": "beech" } } },
    "runs": [{ "kind": "bench", "dir": "bench" }] }] })";

const char *kWorld = R"({
  "schema": "outshine/mod/1", "name": "t", "subject": "one world scene",
  "scenes": [{ "id": "s", "kind": "run", "fovDeg": 60,
    "stage": { "world": {
      "lat": 52.10602, "lon": 9.43453, "eyeM": 1.7, "yawDeg": 280, "pitchDeg": 0,
      "utc": "2026-08-06T17:40:00Z", "windDeg": 250, "windMs": 6.0, "cloudCover": 0.55 } },
    "runs": [{ "kind": "motion", "frames": 1, "give": "stills", "path": "walk.png" }] }] })";

std::string With(const char *text, const std::string &find, const std::string &replace) {
  std::string out = text;
  const size_t at = out.find(find);
  out.replace(at, find.size(), replace);
  return out;
}

[[nodiscard]] bool Refused(const std::string &text, const char *naming) {
  SceneLegacy::Mod mod;
  if (mod.Read(text, "t.json")) return false;
  if (mod.Error().find(naming) == std::string::npos) {
    std::printf("       refusal was: %s\n       expected it to name: %s\n", mod.Error().c_str(),
                naming);
    return false;
  }
  return true;
}

}

int main() {
  Covers("I.25 Stage as an enumeration with a record per arm");
  Covers("I.25 a Studio scenario has no latitude to declare");

  SceneLegacy::Mod studio;
  CHECK(studio.Read(kStudio, "t.json"), "the studio declaration this test varies does load");
  CHECK(studio.Scenes().size() == 1 && studio.Scenes()[0].Staged().AsStudio() != nullptr,
        "and it is on a studio stage");
  CHECK(studio.Scenes()[0].Staged().AsWorld() == nullptr,
        "a studio stage answers no world arm at all");

  SceneLegacy::Mod world;
  CHECK(world.Read(kWorld, "t.json"), "the world declaration this test varies does load");
  CHECK(world.Scenes()[0].Staged().AsWorld() != nullptr, "and it is on a world stage");
  CHECK(world.Scenes()[0].Staged().AsStudio() == nullptr,
        "a world stage answers no studio arm at all");

  CHECK(Refused(With(kStudio, "\"substrate\"", "\"lat\": 52.10602, \"substrate\""),
                "stage.studio.lat"),
        "a latitude on a studio stage is refused, naming its path");
  CHECK(Refused(With(kStudio, "\"substrate\"", "\"utc\": \"2026-08-06T17:40:00Z\", \"substrate\""),
                "stage.studio.utc"),
        "a civil time on a studio stage is refused, naming its path");
  CHECK(Refused(With(kWorld, "\"lat\"", "\"keyLight\": { \"elevationDeg\": 11.0 }, \"lat\""),
                "stage.world.keyLight"),
        "a key light on a world stage is refused, naming its path");

  CHECK(Refused(With(kStudio, "\"studio\"", "\"world\": {}, \"studio\""), "both"),
        "a scene that declares both arms is refused");
  CHECK(Refused(With(kStudio, "\"studio\"", "\"nothing\""), "neither"),
        "a scene that declares neither arm is refused");
  return Report();
}
