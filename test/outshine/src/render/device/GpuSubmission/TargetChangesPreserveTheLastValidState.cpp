#include <Outshine.h>
#include "Check.h"
#include "TestEngine.h"
#include "test/harness/render/NativeTargetFaultDevice.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
bool rejectExtent = false;
unsigned rejectedExtents = 0;
constexpr Extent kOriginal{.WidthPx = 32, .HeightPx = 32};
constexpr Extent kWidened{.WidthPx = 48, .HeightPx = 32};
constexpr double kNearM = 0.1;
constexpr size_t kOriginalRgbaBytes = size_t{kOriginal.WidthPx} * kOriginal.HeightPx * 4;
constexpr size_t kWidenedRgbaBytes = size_t{kWidened.WidthPx} * kWidened.HeightPx * 4;

Scenario::Document TargetScenario(Extent extent) {
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = extent;
  scene.Render.Outputs = {"surface"};
  Scenario::View view;
  view.Id = "target-contract";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = kNearM, .FarM = 10});
  scene.Views.push_back(view);
  return scene;
}

bool Prepare(Engine &engine, const Scenario::Document &scene) {
  const bool ready = engine.declare(scene) && engine.assemble() && engine.advance();
  CHECK(ready, "a real render plan and camera are ready");
  return ready;
}

std::vector<uint8_t> Rendered(Engine &engine) {
  std::vector<uint8_t> pixels;
  CHECK(engine.renderer().render({}).has_value(), "the retained target renders");
  CHECK(engine.renderer().readPixels(pixels).has_value() && !pixels.empty(),
        "the rendered target has readable pixels");
  return pixels;
}

void Retains(Engine &engine, Extent extent, const std::vector<uint8_t> &expected) {
  const auto actual = engine.swapChain().extent();
  CHECK(actual.WidthPx == extent.WidthPx && actual.HeightPx == extent.HeightPx,
        "a rejected candidate keeps both active dimensions");
  std::vector<uint8_t> pixels;
  CHECK(engine.renderer().readPixels(pixels).has_value() && pixels == expected,
        "a rejected candidate keeps the readable previous pixels");
}

void Unconfigured() {
  Engine engine;
  CHECK(!engine.setRenderTarget(Extent{0, 32}), "invalid first configuration is rejected");
  auto target = engine.swapChain();
  CHECK(!engine.renderer().beginFrame(target) && target.extent().WidthPx == 0 &&
            target.extent().HeightPx == 0,
        "a rejected first target leaves no drawable extent");
}

void WindowClaims(SDL_Window *first, SDL_Window *candidate) {
  TargetFault fault;
  Engine owner =
      TestEngine::WithGpu({.Context = &fault, .Create = NativeTargetFaultDevice::Create});
  Engine contender;
  CHECK(owner.setRenderTarget(first).has_value(), "the owner claims its real window");
  const Extent original = owner.swapChain().extent();
  CHECK(!contender.setRenderTarget(first), "a second Engine cannot claim the same window");
  CHECK(owner.swapChain().presents() && !contender.swapChain().presents(),
        "a refused claim preserves both owner identities");
  rejectExtent = true;
  const auto refusedExtent = owner.setRenderTarget(candidate);
  rejectExtent = false;
  CHECK(rejectedExtents == 1 && !refusedExtent && refusedExtent.error().contains("injected"),
        "the real SDL extent boundary reports its original error");
  CHECK(fault.Window == first, "failed extent lookup never changes the native target");
  if (!Prepare(owner, TargetScenario(original))) { return; }
  for (const auto point : {TargetFailure::Present, TargetFailure::Texture}) {
    const unsigned rejected = fault.Rejected;
    fault.Arm(point);
    const auto result = owner.setRenderTarget(candidate);
    fault.Arm(TargetFailure::None);
    CHECK(!result && fault.Rejected == rejected + 1 && result.error().contains("injected"),
          "native claim and post-claim resource failures reach the public caller");
    const auto retained = owner.swapChain().extent();
    CHECK(fault.Window == first && owner.swapChain().presents() &&
              retained.WidthPx == original.WidthPx && retained.HeightPx == original.HeightPx,
          "failed configuration restores the actual original window and physical extent");
    CHECK(!contender.setRenderTarget(first), "the retained original claim remains exclusive");
    CHECK(contender.setRenderTarget(candidate).has_value(),
          "a rejected candidate has no leaked native window claim");
    CHECK(contender.setRenderTarget(kOriginal).has_value(),
          "the contender can release the candidate");
  }
  auto target = owner.swapChain();
  auto renderer = owner.renderer();
  const bool begun = renderer.beginFrame(target).has_value();
  CHECK(begun, "a frame opens on the retained window");
  if (begun) {
    CHECK(!owner.setRenderTarget(candidate) && !owner.setRenderTarget(kOriginal),
          "both target overloads reject a change inside an open frame");
    CHECK(renderer.endFrame().has_value(), "the retained window still presents that frame");
  }
  CHECK(owner.setRenderTarget(candidate).has_value(), "a closed frame permits replacement");
  auto replacement = owner.swapChain();
  CHECK(renderer.beginFrame(replacement).has_value() && renderer.endFrame().has_value(),
        "the replacement window opens and presents a complete frame");
  CHECK(contender.setRenderTarget(first).has_value(),
        "successful replacement releases the former native window claim");
}

void RejectPlans(Engine &engine, TargetFault &fault) {
  const auto before = Rendered(engine);
  CHECK(before.size() == kOriginalRgbaBytes, "the original target produces complete RGBA pixels");
  auto plan = TargetScenario(kOriginal);
  plan.Render.Outputs = {"surface", "sceneVelocity"};
  const auto declared = engine.writeScenario();
  for (const auto point : {TargetFailure::Texture, TargetFailure::Pipeline}) {
    const unsigned rejected = fault.Rejected;
    const unsigned live = fault.CreatedTextures - fault.ReleasedTextures;
    fault.Arm(point);
    const auto result = engine.declare(plan);
    fault.Arm(TargetFailure::None);
    CHECK(!result && fault.Rejected == rejected + 1 && result.error().contains("injected"),
          "frame-resource and pipeline failures reject changed render plans");
    CHECK(engine.writeScenario() == declared, "a rejected plan retains the active declaration");
    Retains(engine, kOriginal, before);
    CHECK(fault.CreatedTextures - fault.ReleasedTextures == live,
          "a rejected plan releases all candidate textures and retains the active set");
  }
  CHECK(Prepare(engine, plan), "the changed plan publishes on retry");
}

void RejectTargets(Engine &engine, TargetFault &fault) {
  const auto before = Rendered(engine);
  for (const auto point : {TargetFailure::Texture, TargetFailure::Pipeline, TargetFailure::Wait}) {
    const unsigned textures = fault.CreatedTextures;
    const unsigned pipelines = fault.CreatedPipelines;
    const unsigned live = textures - fault.ReleasedTextures;
    const unsigned rejected = fault.Rejected;
    fault.Arm(point);
    const auto result = engine.setRenderTarget(kWidened);
    fault.Arm(TargetFailure::None);
    CHECK(!result && fault.Rejected == rejected + 1 && result.error().contains("injected"),
          "allocation, pipeline and completion failures reach the public target caller");
    if (point == TargetFailure::Wait) {
      CHECK(fault.CreatedTextures > textures && fault.CreatedPipelines > pipelines,
            "the complete candidate is built before old GPU usage is settled");
    }
    Retains(engine, kOriginal, before);
    CHECK(fault.CreatedTextures - fault.ReleasedTextures == live,
          "a failed target switch returns to its original live texture count");
  }
  CHECK(!engine.setRenderTarget(Extent{.WidthPx = -1, .HeightPx = kOriginal.HeightPx}) &&
            !engine.setRenderTarget(static_cast<SDL_Window *>(nullptr)),
        "invalid inputs cannot replace an existing target");
  CHECK(engine.setRenderTarget(kOriginal).has_value(), "same-extent retry succeeds");
  CHECK(engine.setRenderTarget(kWidened).has_value() && engine.swapChain().extent().WidthPx == 48,
        "a complete candidate publishes its new extent");
  CHECK(Rendered(engine).size() == kWidenedRgbaBytes,
        "the published target renders complete pixels at its new extent");
}

std::array<unsigned, 4> RepeatedTargets(Engine &engine, const TargetFault &fault) {
  const auto before = std::array{
      fault.CreatedTextures, fault.CreatedSamplers, fault.CreatedBuffers, fault.CreatedPipelines};
  const unsigned released = fault.ReleasedTextures;
  CHECK(engine.setRenderTarget(kOriginal).has_value(), "first repeated switch succeeds");
  const auto counts = std::array{fault.CreatedTextures - before[0],
                                 fault.CreatedSamplers - before[1],
                                 fault.CreatedBuffers - before[2],
                                 fault.CreatedPipelines - before[3]};
  CHECK(counts[0] > 0 && counts[1] > 0 && counts[2] > 0 && counts[3] > 0,
        "each allocation family has actual target-candidate coverage");
  CHECK(engine.setRenderTarget(kWidened).has_value(), "second repeated switch succeeds");
  CHECK(fault.CreatedTextures - before[0] == 2u * counts[0] &&
            fault.ReleasedTextures - released == 2u * counts[0],
        "repeated switches keep a fixed live texture budget");
  CHECK(fault.CreatedSamplers - before[1] == 2u * counts[1] &&
            fault.CreatedBuffers - before[2] == 2u * counts[2] &&
            fault.CreatedPipelines - before[3] == 2u * counts[3],
        "repeated switches build fixed sampler, buffer and pipeline sets");
  return counts;
}

void EveryCreation(Engine &engine, TargetFault &fault, const std::array<unsigned, 4> &counts) {
  constexpr std::array points{TargetFailure::Texture,
                              TargetFailure::Sampler,
                              TargetFailure::Buffer,
                              TargetFailure::Pipeline};
  for (size_t family = 0; family < points.size(); ++family) {
    for (unsigned at = 1; at <= counts[family]; ++at) {
      const Extent retained = engine.swapChain().extent();
      const Extent next = retained.WidthPx == kOriginal.WidthPx ? kWidened : kOriginal;
      const auto pixels = Rendered(engine);
      const unsigned rejected = fault.Rejected;
      const unsigned live = fault.CreatedTextures - fault.ReleasedTextures;
      fault.Arm(points[family], at);
      const auto result = engine.setRenderTarget(next);
      fault.Arm(TargetFailure::None);
      CHECK(!result && fault.Rejected == rejected + 1,
            "every native candidate creation boundary rejects independently");
      Retains(engine, retained, pixels);
      CHECK(fault.CreatedTextures - fault.ReleasedTextures == live,
            "partial candidates release every newly allocated texture");
      CHECK(engine.setRenderTarget(next).has_value(), "every creation failure is retryable");
    }
  }
}

void OffscreenTargets() {
  TargetFault fault;
  {
    Engine engine =
        TestEngine::WithGpu({.Context = &fault, .Create = NativeTargetFaultDevice::Create});
    CHECK(engine.setRenderTarget(kOriginal).has_value(), "offscreen target is configured");
    if (Prepare(engine, TargetScenario(kOriginal))) {
      RejectPlans(engine, fault);
      RejectTargets(engine, fault);
      const auto counts = RepeatedTargets(engine, fault);
      EveryCreation(engine, fault, counts);
    }
  }
  CHECK(fault.CreatedTextures > 0 && fault.CreatedTextures == fault.ReleasedTextures,
        "Engine destruction releases every owned native texture");
}
}

extern "C" bool SDLCALL SDL_GetWindowSizeInPixels(SDL_Window *window, int *width, int *height) {
  if (rejectExtent) {
    ++rejectedExtents;
    return SDL_SetError("injected window extent failure");
  }
  static const auto original = reinterpret_cast<decltype(&SDL_GetWindowSizeInPixels)>(
      dlsym(RTLD_NEXT, "SDL_GetWindowSizeInPixels"));
  assert(original != nullptr);
  return original(window, width, height);
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes on the main thread");
  SDL_Window *first =
      SDL_CreateWindow("Outshine target contract", kOriginal.WidthPx, kOriginal.HeightPx, 0);
  SDL_Window *candidate =
      SDL_CreateWindow("Outshine target candidate", kWidened.WidthPx, kWidened.HeightPx, 0);
  CHECK(first != nullptr && candidate != nullptr, "real windows outlive their borrowers");
  if (first != nullptr && candidate != nullptr) {
    Unconfigured();
    WindowClaims(first, candidate);
    {
      Engine afterDestruction;
      CHECK(afterDestruction.setRenderTarget(first).has_value() &&
                afterDestruction.setRenderTarget(candidate).has_value(),
            "destruction releases both former window claims");
    }
    OffscreenTargets();
  }
  SDL_DestroyWindow(candidate);
  SDL_DestroyWindow(first);
  SDL_Quit();
  Covers("native window claim/configuration rollback; every texture, sampler, buffer and "
         "graphics-pipeline candidate allocation; completion failures; open-frame rejection; "
         "borrowed window lifetime; exact retained pixels; bounded live texture ownership");
  return Report();
}
