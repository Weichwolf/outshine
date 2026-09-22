#include <Outshine.h>
#include <scenario/Scenario.h>

#include "Check.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace {

outshine::Scenario::Document GroundScenario() {
  outshine::Scenario::Document scenario;
  scenario.Ground.Declared = true;
  scenario.Ground.VegetationEnabled = false;
  scenario.Ground.Origin.LatitudeDeg = 49.3777;
  scenario.Ground.Origin.LongitudeDeg = 10.179;
  scenario.Ground.PatienceS = 3;
  scenario.Ground.SightM = 8000;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {160, 90};
  scenario.Render.Audits = true;
  scenario.Lit.Declared = true;
  for (const auto [id, fov] : {std::pair{"a", 55.0}, std::pair{"b", 60.0}, std::pair{"c", 65.0}}) {
    outshine::Scenario::View view;
    view.Id = id;
    view.Person = "first";
    view.Placement = outshine::Scenario::CameraPlacement::Geodetic;
    view.Geographic.Geodetic.LatitudeDeg = 49.3777;
    view.Geographic.Geodetic.LongitudeDeg = 10.179;
    view.Geographic.Geodetic.HeightM = 1.7;
    view.Geographic.SamplesHeight = false;
    view.Sees.FovDeg = fov;
    scenario.Views.push_back(view);
  }
  return scenario;
}

std::optional<double> Measure(const outshine::Engine &engine, std::string_view name) {
  for (const outshine::DiagnosticSample &sample : engine.measures()) {
    if (sample.Name == name) { return sample.Value; }
  }
  return std::nullopt;
}

template <typename Ready> bool AdvanceUntil(outshine::Engine &engine, Ready ready) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  for (int frame = 0; frame < 8000 && std::chrono::steady_clock::now() < deadline; ++frame) {
    if (ready()) { return true; }
    const auto advanced = engine.advance();
    if (!advanced) {
      outshine::Test::Unprepared(advanced.error().c_str());
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return ready();
}

}

int Run() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  if (!engine.setRoots({"src/assets/drive", "src/assets", "/tmp/outshine-stale-press", false}) ||
      !engine.drawsInto({160, 90})) {
    Unprepared("offscreen ground target did not start");
    return Report();
  }
  const auto declared = engine.declare(GroundScenario());
  CHECK(declared.has_value(), declared ? "three views declared" : declared.error().c_str());
  if (!declared || !engine.assemble() || !engine.setView("a")) {
    Unprepared("initial ground scenario did not assemble");
    return Report();
  }
  const bool initial = AdvanceUntil(engine, [&] { return engine.settled(WorldQuality::Refined); });
  CHECK(initial, "view A publishes refined ground before the revision race");
  const auto publications = Measure(engine, "times the terrain was rebuilt");
  const auto started = Measure(engine, "ground candidate: starts");
  CHECK(publications.has_value() && started.has_value(),
        "published ground and candidate counters are visible");
  if (!initial || !publications || !started || !engine.setView("b")) { return Report(); }

  const bool pressing = AdvanceUntil(engine, [&] {
    const auto progress = Measure(engine, "ground candidate: progress");
    const auto starts = Measure(engine, "ground candidate: starts");
    return starts && *starts > *started && progress && *progress == 11.0;
  });
  CHECK(pressing, "view B reaches resumable earthworks without publishing");
  if (!pressing) {
    for (const char *name : {"ground candidate: starts",
                             "ground candidate: progress",
                             "ground candidate: revision mismatch mask",
                             "times the terrain was rebuilt"}) {
      if (const auto value = Measure(engine, name)) { Note(name, *value, ""); }
    }
    return Report();
  }
  for (int slice = 0; slice < 10; ++slice) {
    CHECK(engine.advance().has_value(), "view B advances a terrain press slice");
  }
  CHECK(Measure(engine, "times the terrain was rebuilt") == publications,
        "partly pressed view B has not published");

  CHECK(engine.setView("c").has_value() && engine.advance().has_value(),
        "view C requests a new projection revision");
  const auto restarted = Measure(engine, "ground candidate: starts");
  const auto mismatch = Measure(engine, "ground candidate: revision mismatch mask");
  CHECK(restarted && *restarted > *started + 1.0 && mismatch &&
            (static_cast<unsigned>(*mismatch) & (1u << 5u)) != 0u,
        "projection change discards the active B candidate and starts C");
  CHECK(Measure(engine, "times the terrain was rebuilt") == publications,
        "stale B cannot publish at the revision switch");
  CHECK(!engine.settled(WorldQuality::Refined), "old A projection cannot satisfy view C readiness");

  const bool final = AdvanceUntil(engine, [&] {
    const auto count = Measure(engine, "times the terrain was rebuilt");
    return count && *count == *publications + 1.0 && engine.settled(WorldQuality::Refined);
  });
  CHECK(final, "view C reaches refined ground");
  const auto finalPublications = Measure(engine, "times the terrain was rebuilt");
  if (finalPublications && *finalPublications != *publications + 1.0) {
    Note("publication count before B", *publications, "rebuilds");
    Note("publication count after C", *finalPublications, "rebuilds");
  }
  CHECK(finalPublications && *finalPublications == *publications + 1.0,
        "only C publishes after A; stale B never becomes the world");
  return Report();
}

int main() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    outshine::Test::Unprepared("SDL did not start");
    return outshine::Test::Report();
  }
  const int result = Run();
  SDL_Quit();
  return result;
}
