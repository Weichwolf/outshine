#include <Outshine.h>
#include <SDL3/SDL.h>

#include "Check.h"

#include <chrono>
#include <optional>
#include <string_view>
#include <thread>

namespace {

[[nodiscard]] std::optional<double> Measure(const outshine::Engine &engine, std::string_view name) {
  for (const outshine::DiagnosticSample &sample : engine.measures()) {
    if (sample.Name == name) { return sample.Value; }
  }
  return std::nullopt;
}

[[nodiscard]] outshine::Scenario::Document Scene() {
  outshine::Scenario::Document scene;
  scene.Ground.Declared = true;
  scene.Ground.VegetationEnabled = false;
  scene.Ground.Origin.LatitudeDeg = 49.3777;
  scene.Ground.Origin.LongitudeDeg = 10.179;
  scene.Ground.PatienceS = 3;
  scene.Ground.SightM = 8000;
  scene.Render.Declared = true;
  scene.Render.Frame = {160, 90};
  scene.Lit.Declared = true;
  outshine::Scenario::View first;
  first.Id = "first";
  first.Person = "first";
  first.Placement = outshine::Scenario::CameraPlacement::Geodetic;
  first.Geographic.Geodetic.LatitudeDeg = 49.3777;
  first.Geographic.Geodetic.LongitudeDeg = 10.179;
  first.Geographic.Geodetic.HeightM = 1.7;
  first.Geographic.SamplesHeight = false;
  first.Sees.FovDeg = 55;
  scene.Views.push_back(first);
  outshine::Scenario::View shifted = first;
  shifted.Id = "shifted";
  shifted.Geographic.Geodetic.LongitudeDeg = 10.235;
  scene.Views.push_back(shifted);
  return scene;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for graph worker cancellation");
  {
    Engine engine;
    const bool ready =
        engine.setRoots(
            {"src/assets/drive", "src/assets", "/tmp/outshine-network-cancel", false}) &&
        engine.setRenderTarget({160, 90}) && engine.declare(Scene()) && engine.assemble();
    CHECK(ready, "two-view source world starts through the public API");
    if (ready) {
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      bool network = false;
      while (std::chrono::steady_clock::now() < deadline && !network) {
        const auto advanced = engine.advance();
        CHECK(advanced.has_value(),
              advanced ? "ground candidate advances" : advanced.error().c_str());
        if (!advanced) { break; }
        network = Measure(engine, "ground candidate: progress") == 9.0;
        if (!network) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
      }
      CHECK(network, "the candidate reaches its asynchronous vector graph stage");
      if (network) {
        const double starts = Measure(engine, "ground candidate: starts").value_or(0.0);
        CHECK(engine.setView("shifted"), "changed view invalidates the graph candidate");
        bool replaced = false;
        bool mismatch = false;
        const auto switchDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (std::chrono::steady_clock::now() < switchDeadline && !replaced) {
          const auto advanced = engine.advance();
          CHECK(advanced.has_value(), advanced ? "replacement advances" : advanced.error().c_str());
          if (!advanced) { break; }
          mismatch |=
              Measure(engine, "ground candidate: revision mismatch mask").value_or(0.0) > 0.0;
          replaced = mismatch && Measure(engine, "ground candidate: starts").value_or(0.0) > starts;
          if (!replaced) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        }
        const double retirementMs = Measure(engine, "ground retirement time, most").value_or(0.0);
        Note("vector graph retirement maximum", retirementMs, "ms");
        CHECK(replaced && retirementMs > 0.0 && retirementMs < 16.67,
              "a changed view cancels and retires graph work without a frame stall");
        CHECK(!engine.settled(WorldQuality::Refined),
              "the discarded graph candidate does not satisfy the new view");
      }
    }
  }
  SDL_Quit();
  return Report();
}
