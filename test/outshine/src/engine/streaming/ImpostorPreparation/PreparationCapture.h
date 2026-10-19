#ifndef OUTSHINE_TEST_PREPARATIONCAPTURE_H
#define OUTSHINE_TEST_PREPARATIONCAPTURE_H
#include "PreparationCommon.h"
#include "Tasks.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <print>
#include <ratio>

namespace outshine::Test::CrownFixture {
struct Foreground {
  Engine Renderer;
  std::vector<float> Reference;
  std::vector<float> Frame;
  std::array<std::vector<double>, 3> Times;
  bool Stable = true;
  bool Readable = true;
};

inline bool OpenForeground(Foreground &foreground) {
  Geometry control;
  Material white;
  white.BaseColour = {{1, 1, 1, 1}};
  white.Unlit = true;
  const auto surface = control.addSurface("white", white);
  CHECK(surface.has_value(), "foreground material stands");
  if (!surface) { return false; }
  const auto part = control.addPart("control", *surface);
  CHECK(part.has_value(), "foreground control part stands");
  if (!part) { return false; }
  CHECK(control.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
            control.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "foreground control geometry stands");
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = kForegroundExtent;
  scene.Render.Outputs = {"sceneLinear"};
  Scenario::View camera;
  camera.Id = "control";
  camera.Person = "first";
  camera.Placement = Scenario::CameraPlacement::Local;
  camera.Sees.PositionM = {{0, 0, 3}};
  scene.Views.push_back(camera);
  const bool ready = foreground.Renderer.setRenderTarget(kForegroundExtent) &&
                     foreground.Renderer.declare(scene) &&
                     foreground.Renderer.setGeometry(control) && foreground.Renderer.assemble() &&
                     foreground.Renderer.advance();
  CHECK(ready, "independent foreground renderer stands before the bake");
  if (!ready) { return false; }
  const bool read = foreground.Renderer.renderer().render({}) &&
                    foreground.Renderer.renderer().readPixels(Buffer::Linear, foreground.Reference);
  CHECK(read, "foreground reference pixels are readable");
  return read && !foreground.Reference.empty();
}

inline void DrawForeground(Foreground &foreground, size_t phase) {
  const auto began = std::chrono::steady_clock::now();
  const bool drawn = foreground.Renderer.renderer().render({}) &&
                     foreground.Renderer.renderer().readPixels(Buffer::Linear, foreground.Frame);
  foreground.Times[phase].push_back(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  foreground.Readable &= drawn;
  foreground.Stable &= foreground.Frame == foreground.Reference;
}

inline void PrintForegroundTimes(Foreground &foreground) {
  constexpr std::array<const char *, 3> labels{"before", "during-bake", "after"};
  for (size_t phase = 0; phase < foreground.Times.size(); ++phase) {
    auto &samples = foreground.Times[phase];
    std::ranges::sort(samples);
    if (samples.empty()) { continue; }
    const auto quantile = [&](double q) {
      return samples[static_cast<size_t>(std::ceil(q * static_cast<double>(samples.size()))) - 1];
    };
    std::println("foreground {} render+linear-readback n={} p50={:.3f} p95={:.3f} p99={:.3f} "
                 "worst={:.3f} ms over16.67={}; trivial scene",
                 labels[phase],
                 samples.size(),
                 quantile(kQuantiles[0]),
                 quantile(kQuantiles[1]),
                 quantile(kQuantiles[2]),
                 samples.back(),
                 std::count_if(samples.begin(), samples.end(), [](double ms) {
                   return ms > kFrameBudgetMs;
                 }));
  }
}

inline std::optional<Content::ImpostorAtlas>
CaptureAtlas(const TreePrototype &tree, Tasks &worker, Foreground &foreground) {
  std::string error;
  for (size_t n = 0; n < kForegroundFrames; ++n) { DrawForeground(foreground, 0); }
  std::optional<Content::ImpostorAtlas> atlas;
  const auto began = std::chrono::steady_clock::now();
  const auto job = worker.Post(
      [&] { atlas = BakeImpostorAtlas(tree, {.Pixels = kPixels, .Views = kViews}, error); });
  bool finished = false;
  while (!(finished = worker.TakeCompletion(job)) &&
         std::chrono::steady_clock::now() - began < kCaptureTimeout) {
    DrawForeground(foreground, 1);
  }
  if (!finished) { worker.Wait(job); }
  const double elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  for (size_t n = 0; n < kForegroundFrames; ++n) { DrawForeground(foreground, 2); }
  CHECK(foreground.Readable && foreground.Stable,
        "a parallel crown bake leaves foreground linear pixels unchanged");
  std::println("worker crown capture {:.3f} ms; observation limited={}; no world or "
               "portable-threading claim",
               elapsed,
               !finished);
  PrintForegroundTimes(foreground);
  std::filesystem::create_directories(OutputDirectory());
  CHECK(foreground.Renderer.renderer()
            .saveScreenshot(OutputDirectory() + "/concurrent-foreground.png")
            .has_value(),
        "concurrent foreground PNG is written");
  CHECK(atlas.has_value(), "the native renderer captures crown surface data");
  if (!atlas) { std::println("{}", error); }
  return atlas;
}
}
#endif
