#include <Outshine.h>
#include <scenario/Scenario.h>
#include "Check.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>
#include <thread>

namespace {
std::optional<double> Measure(const outshine::Engine &engine, std::string_view name) {
  for (const outshine::DiagnosticSample &sample : engine.measures()) {
    if (sample.Name == name) { return sample.Value; }
  }
  return std::nullopt;
}

outshine::Scenario::Document Scenario() {
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
  outshine::Scenario::View first;
  first.Id = "first";
  first.Person = "first";
  first.Placement = outshine::Scenario::CameraPlacement::Geodetic;
  first.Geographic.Geodetic.LatitudeDeg = 49.3777;
  first.Geographic.Geodetic.LongitudeDeg = 10.179;
  first.Geographic.Geodetic.HeightM = 1.7;
  first.Geographic.SamplesHeight = false;
  first.Sees.FovDeg = 55;
  scenario.Views.push_back(first);
  outshine::Scenario::View shifted = first;
  shifted.Id = "shifted";
  shifted.Geographic.Geodetic.LongitudeDeg = 10.235;
  scenario.Views.push_back(shifted);
  return scenario;
}
}

int main() {
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "video initializes");
  if (!initialized) { return Report(); }
  {
    outshine::Engine engine;
    CHECK(engine.setRoots({"src/assets/drive", "src/assets", "/tmp/outshine-paced", false}) &&
              engine.setRenderTarget({160, 90}),
          "offscreen world starts");
    const auto declared = engine.declare(Scenario());
    CHECK(declared.has_value(), declared ? "two views declare" : declared.error().c_str());
    const auto assembled = engine.assemble();
    CHECK(assembled.has_value(), assembled ? "world assembles" : assembled.error().c_str());
    if (declared && assembled) {
      bool corridorStarted = false;
      const auto readyUntil = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      while (std::chrono::steady_clock::now() < readyUntil && !corridorStarted) {
        const auto advanced = engine.advance();
        CHECK(advanced.has_value(), advanced ? "candidate advances" : advanced.error().c_str());
        if (!advanced) { break; }
        corridorStarted = Measure(engine, "ground candidate: corridor job admission").has_value();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      CHECK(corridorStarted, "first candidate has started road construction");
      if (corridorStarted) {
        const double corridorProgress = Measure(engine, "ground candidate: progress").value_or(-1);
        const auto roadAdvanced = engine.advance();
        CHECK(roadAdvanced.has_value(),
              roadAdvanced ? "road work advances" : roadAdvanced.error().c_str());
        CHECK(roadAdvanced && corridorProgress >= 0 &&
                  Measure(engine, "ground candidate: progress").value_or(-2) == corridorProgress,
              "view changes while corridor construction remains in flight");
        const double startsBefore = Measure(engine, "ground candidate: starts").value_or(0);
        CHECK(startsBefore > 0, "first candidate has a counted admission");
        CHECK(engine.setView("shifted").has_value(), "second view changes the requested region");
        bool mismatch = false;
        bool replacementStarted = false;
        double retirementMostMs = 0;
        const auto switchUntil = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (std::chrono::steady_clock::now() < switchUntil && !replacementStarted) {
          const auto advanced = engine.advance();
          CHECK(advanced.has_value(), advanced ? "replacement advances" : advanced.error().c_str());
          if (!advanced) { break; }
          mismatch |= Measure(engine, "ground candidate: revision mismatch mask").value_or(0) > 0;
          replacementStarted =
              mismatch && Measure(engine, "ground candidate: starts").value_or(0) > startsBefore;
          retirementMostMs = std::max(retirementMostMs,
                                      Measure(engine, "ground retirement time, most").value_or(0));
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(mismatch && replacementStarted,
              "changed view retires the old candidate before replacement");
        Note("changed-view retirement maximum", retirementMostMs, "ms");
        CHECK(retirementMostMs > 0 && retirementMostMs < 16.67,
              "candidate retirement stays within one frame");
        const auto lateUntil = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        bool lateCandidate = false;
        while (std::chrono::steady_clock::now() < lateUntil && !lateCandidate) {
          const auto advanced = engine.advance();
          CHECK(advanced.has_value(),
                advanced ? "late candidate advances" : advanced.error().c_str());
          if (!advanced) { break; }
          lateCandidate = Measure(engine, "ground candidate: class upload").has_value() &&
                          !Measure(engine, "ground candidate: publication").has_value();
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(lateCandidate, "replacement reaches geometry preparation before publication");
        if (lateCandidate) {
          const double startsAtLate = Measure(engine, "ground candidate: starts").value_or(0);
          const double retirementSlices = Measure(engine, "ground retirement slices").value_or(0);
          CHECK(engine.setView("first").has_value(), "return view invalidates late candidate");
          bool lateRetired = false;
          bool lateReplacementStarted = false;
          double lateRetirementMostMs = 0;
          double canceledBytes = 0;
          const auto returnUntil = std::chrono::steady_clock::now() + std::chrono::seconds(30);
          while (std::chrono::steady_clock::now() < returnUntil && !lateReplacementStarted) {
            const auto advanced = engine.advance();
            CHECK(advanced.has_value(),
                  advanced ? "late replacement advances" : advanced.error().c_str());
            if (!advanced) { break; }
            const double slices = Measure(engine, "ground retirement slices").value_or(0);
            if (slices > retirementSlices) {
              lateRetired = true;
              lateRetirementMostMs =
                  std::max(lateRetirementMostMs,
                           Measure(engine, "ground retirement time, last").value_or(0));
            }
            canceledBytes = Measure(engine, "ground candidate: canceled CPU products").value_or(0);
            lateReplacementStarted =
                lateRetired &&
                Measure(engine, "ground candidate: starts").value_or(0) > startsAtLate;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          Note("late canceled CPU products", canceledBytes, "bytes");
          Note("late retirement maximum", lateRetirementMostMs, "ms");
          CHECK(lateReplacementStarted && canceledBytes > 0,
                "late candidate retires before another replacement starts");
          CHECK(lateRetirementMostMs > 0 && lateRetirementMostMs < 16.67,
                "late candidate retirement stays within one frame");
        }
      }
    }
  }
  SDL_Quit();
  return Report();
}
