#include "Check.h"
#include "TestEngine.h"
#include "test/harness/render/NativeTargetFaultDevice.h"
#include <Engine.h>
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
namespace Native = outshine::Render::Native;

struct Context {
  TargetFault Fault;
  uint64_t Submissions = 0;
};

std::vector<uint8_t> Rendered(Engine &engine, const Context &context) {
  const auto before = context.Submissions;
  const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  bool accepted = true;
  do {
    accepted = engine.renderer().render({}).has_value();
    if (!accepted || context.Submissions != before) { break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < limit);
  CHECK(accepted && context.Submissions > before,
        "a real frame is submitted with resident geometry");
  std::vector<uint8_t> pixels;
  CHECK(engine.renderer().readPixels(pixels), "the completed frame is readable");
  return pixels;
}

void RedCentre(const std::vector<uint8_t> &pixels, Extent extent) {
  CHECK(pixels.size() == static_cast<size_t>(extent.WidthPx) * extent.HeightPx * 4,
        "the original physical extent remains in use");
  if (pixels.size() != static_cast<size_t>(extent.WidthPx) * extent.HeightPx * 4) { return; }
  const size_t at =
      (static_cast<size_t>(extent.HeightPx / 2) * extent.WidthPx + extent.WidthPx / 2) * 4;
  CHECK(static_cast<unsigned>(pixels[at]) > static_cast<unsigned>(pixels[at + 1]) + 16 &&
            static_cast<unsigned>(pixels[at]) > static_cast<unsigned>(pixels[at + 2]) + 16,
        "the live unlit red geometry still supplies the centre pixel");
}

void Exercise(SDL_Window *first, SDL_Window *candidate) {
  Context context;
  Engine engine = TestEngine::WithGpu(
      {.Context = &context,
       .Create =
           [](void *opaque, Native::DeviceOptions options) {
             options.Validation = true;
             return NativeTargetFaultDevice::Create(&static_cast<Context *>(opaque)->Fault,
                                                    options);
           },
       .Submit =
           [](void *opaque, Native::Device *device, std::unique_ptr<Native::Commands> commands) {
             auto result = device->Submit(std::move(commands));
             if (result) { ++static_cast<Context *>(opaque)->Submissions; }
             return result;
           }});
  const auto targeted = engine.setRenderTarget(first);
  CHECK(targeted, "the original window is configured");
  if (!targeted) {
    Note(targeted.error().c_str());
    return;
  }
  const auto extent = engine.frameExtent();
  Geometry geometry;
  Material material;
  material.BaseColour = {{1, 0, 0, 1}};
  material.Unlit = true;
  const auto surface = geometry.addSurface("resident red", material).value();
  const int part = geometry.addPart("resident quad", surface).value();
  CHECK(geometry.setPositions(part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}),
        "quad positions");
  CHECK(geometry.setNormals(part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
        "quad normals");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}), "quad indices");
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = extent;
  scene.Render.Outputs = {"surface"};
  Scenario::View view;
  view.Id = "target-claim-geometry";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = .1, .FarM = 10});
  scene.Views.push_back(view);
  const bool ready = engine.declare(scene) && engine.setGeometry(geometry) && engine.assemble() &&
                     engine.advance();
  CHECK(ready, "the real geometry scene is ready");
  if (!ready) { return; }
  geometry.clear();
  auto previous = Rendered(engine, context);
  RedCentre(previous, extent);
  for (const auto point : {TargetFailure::Texture, TargetFailure::Present}) {
    const auto attempts = context.Fault.PresentAttempts;
    context.Fault.Arm(point);
    const auto refused = engine.setRenderTarget(candidate);
    context.Fault.Arm(TargetFailure::None);
    CHECK(!refused && context.Fault.Window == first,
          "a refused candidate retains the actual native window");
    if (point == TargetFailure::Texture) {
      CHECK(context.Fault.PresentAttempts == attempts,
            "frame preparation fails before any native target change");
    }
    std::vector<uint8_t> retained;
    CHECK(engine.renderer().readPixels(retained) && retained == previous,
          "failure preserves the exact previously readable pixels");
    previous = Rendered(engine, context);
    RedCentre(previous, extent);
  }
}
} // namespace

int main() {
  const bool video = SDL_Init(SDL_INIT_VIDEO);
  CHECK(video, "SDL video initializes");
  if (!video) { return Report(); }
  {
    using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    const Window first(SDL_CreateWindow("retained geometry", 64, 64, 0), SDL_DestroyWindow);
    const Window candidate(SDL_CreateWindow("refused geometry", 80, 64, 0), SDL_DestroyWindow);
    CHECK(first && candidate, "borrowed windows are alive through device shutdown");
    if (first && candidate) { Exercise(first.get(), candidate.get()); }
  }
  SDL_Quit();
  Covers("frame preparation precedes native commit; real geometry remains "
         "submitted and readable after resource and native claim failures");
  return Report();
}
