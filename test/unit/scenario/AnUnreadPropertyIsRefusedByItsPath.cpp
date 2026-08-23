#include "Check.h"
#include "Mod.h"

#include <string>

using namespace outshine;
using namespace outshine::Test;

namespace {

const char *kWorld = R"({
  "schema": "outshine/mod/1", "name": "t", "subject": "one world scene",
  "scenes": [{ "id": "s", "kind": "run", "fovDeg": 60,
    "stage": { "world": {
      "lat": 52.10602, "lon": 9.43453, "eyeM": 1.7, "yawDeg": 280, "pitchDeg": 0,
      "utc": "2026-08-06T17:40:00Z", "windDeg": 250, "windMs": 6.0, "cloudCover": 0.55 } },
    "render": { "width": 640, "height": 360, "why": "a quarter frame, to keep this test cheap" },
    "runs": [{ "kind": "motion", "frames": 1, "give": "stills", "path": "walk.png" }] }] })";

std::string With(const std::string &find, const std::string &replace) {
  std::string out = kWorld;
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
  Covers("I.4 an unknown property is refused with its path");
  Covers("I.4 a parse failure names the byte offset");

  SceneLegacy::Mod ok;
  CHECK(ok.Read(kWorld, "t.json"), "the declaration this test varies does load");

  CHECK(Refused(With("\"lat\"", "\"latitude\": 52.0, \"lat\""), "stage.world.latitude"),
        "a misspelt world property is refused, naming it");
  CHECK(Refused(With("\"fovDeg\": 60", "\"fovDeg\": 60, \"eyeM\": 1.7"), "(s).eyeM"),
        "a world property at scene level is refused, naming the scene by its id");
  CHECK(Refused(With("\"why\": \"a quarter frame, to keep this test cheap\"",
                     "\"why\": \"cheap\", \"depth\": 24"),
                "render.depth"),
        "an unknown property three levels down is refused, naming all three");
  CHECK(Refused(With("\"give\": \"stills\"", "\"give\": \"stills\", \"turnSteps\": 8"),
                "runs[0].turnSteps"),
        "a bench property on a motion run is refused, naming the run's index");

  CHECK(Refused(With(", \"why\": \"a quarter frame, to keep this test cheap\"", ""), "render.why"),
        "a render size that is not the budget's and states no why is refused");

  SceneLegacy::Mod broken;
  CHECK(!broken.Read("{ \"schema\": \"outshine/mod/1\", ", "t.json"),
        "a truncated declaration is refused");
  CHECK(broken.Error().find("byte") != std::string::npos,
        "and the refusal names the byte the parse stopped at");
  std::printf("NOTE parse refusal: %s\n", broken.Error().c_str());
  return Report();
}
