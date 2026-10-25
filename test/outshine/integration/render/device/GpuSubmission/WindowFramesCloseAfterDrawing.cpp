#include "ImageCapture.h"
#include <Engine.h>
#include "Check.h"
#include "TestEngine.h"
#include "test/harness/render/WindowFrameDevice.h"
#include <array>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <print>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine::Test::Gpu::WindowFrame;
constexpr int kFrameSidePx = 64;
constexpr size_t kImageByteCount = static_cast<size_t>(kFrameSidePx) * kFrameSidePx * 4;
constexpr size_t kCentreByte =
    (static_cast<size_t>(kFrameSidePx / 2) * kFrameSidePx + kFrameSidePx / 2) * 4;
constexpr double kNearM = 0.1;
constexpr outshine::Vec4f kCopper{{0.8f, 0.12f, 0.03f, 1}};
constexpr outshine::Vec4f kGreen{{0.03f, 0.8f, 0.12f, 1}};

double RenderFrames(const outshine::Engine &engine) {
  for (const auto &sample : engine.measures()) {
    if (sample.Name == "cost.render.frames") { return sample.Value; }
  }
  return std::numeric_limits<double>::quiet_NaN();
}

bool Prepare(outshine::Engine &engine, bool colour = true) {
  using namespace outshine;
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {.WidthPx = kFrameSidePx, .HeightPx = kFrameSidePx};
  scene.Render.Outputs = {colour ? "surface" : "sceneLinear"};
  scene.Render.Stages = {"subjects"};
  if (colour) {
    scene.Render.Stages.insert(scene.Render.Stages.end(), {"tonemap", "present"});
    scene.Render.Exposure = 1;
  }
  Scenario::View view;
  view.Id = "frame-contract";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = kNearM, .FarM = 10});
  scene.Views.push_back(view);
  Material material;
  material.Unlit = true;
  material.BaseColour = colour ? kCopper : kGreen;
  Geometry geometry;
  const auto surface = geometry.addSurface("copper coloured test", material);
  if (!surface) { return false; }
  const int part = geometry.addPart("triangle", *surface).value();
  return geometry.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
         geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}) && engine.declare(scene) &&
         engine.setGeometry(geometry) && engine.assemble() && engine.advance();
}

void CheckFrameScopes(outshine::Engine &engine, std::vector<uint8_t> &windowPixels) {
  auto renderer = engine.renderer();
  auto copied = renderer;
  auto target = engine.swapChain();
  CHECK(renderer.beginFrame(target).has_value(), "window scope begins");
  CHECK(renderer.render({}).has_value(), "explicit window draw succeeds");
  unsigned before = swapchainCalls;
  double framesBefore = RenderFrames(engine);
  CHECK(copied.endFrame().has_value(), "copied facade closes a drawn frame");
  CHECK(swapchainCalls == before, "endFrame never repeats an explicit successful draw");
  CHECK(RenderFrames(engine) == framesBefore,
        "closing an explicitly drawn frame does not publish another render");

  CHECK(renderer.beginFrame(target).has_value(), "empty window scope begins");
  before = swapchainCalls;
  CHECK(renderer.endFrame().has_value() && swapchainCalls == before + 1,
        "an empty window scope still draws once when closed");
  CHECK(RenderFrames(engine) == framesBefore + 1,
        "the closing fallback publishes its successfully submitted render");

  CHECK(renderer.beginFrame(target).has_value(), "skipped window scope begins");
  skipSwapchain = true;
  CHECK(renderer.render({}).has_value() && !skipSwapchain,
        "minimized acquisition skips successfully");
  before = swapchainCalls;
  CHECK(renderer.endFrame().has_value() && swapchainCalls == before,
        "a skipped draw is not immediately retried by endFrame");

  CHECK(renderer.beginFrame(target).has_value(), "failed draw scope begins");
  failSwapchain = true;
  CHECK(!renderer.render({}) && !failSwapchain,
        "failed draw reaches the real acquisition boundary");
  before = swapchainCalls;
  framesBefore = RenderFrames(engine);
  CHECK(renderer.endFrame().has_value() && swapchainCalls == before + 1,
        "endFrame retains the fallback draw when no draw succeeded");
  CHECK(RenderFrames(engine) == framesBefore + 1,
        "recovery after a failed draw publishes the successful fallback");

  CHECK(renderer.beginFrame(target).has_value(), "failed fallback scope begins");
  failSwapchain = true;
  framesBefore = RenderFrames(engine);
  CHECK(!renderer.endFrame() && !failSwapchain,
        "the closing fallback propagates the real acquisition failure");
  before = swapchainCalls;
  CHECK(!copied.endFrame() && swapchainCalls == before,
        "a failed fallback still closes its frame exactly once");
  CHECK(RenderFrames(engine) == framesBefore,
        "a failed fallback does not publish a successful render");

  CHECK(renderer.beginFrame(target).has_value(), "readback scope begins");
  before = swapchainCalls;
  const auto pixels = renderer.readPixels(windowPixels);
  if (!pixels) { std::println(stderr, "window readback: {}", pixels.error()); }
  CHECK(pixels.has_value(), "window pixels are readable");
  CHECK(swapchainCalls == before, "colour readback never acquires another swapchain image");
  std::vector<uint8_t> repeatedPixels;
  CHECK(renderer.readPixels(repeatedPixels).has_value() && repeatedPixels == windowPixels &&
            swapchainCalls == before,
        "repeated readback preserves the submitted frame");
  before = swapchainCalls;
  CHECK(renderer.endFrame().has_value() && swapchainCalls == before + 1,
        "readback does not draw or satisfy an empty frame scope");
  CHECK(forbiddenReads == 0, "no readback reads a write-only swapchain texture");
}

void CheckReadbackFailures(outshine::Engine &engine) {
  using namespace outshine;
  std::vector<uint8_t> windowPixels;
  auto renderer = engine.renderer();
  auto target = engine.swapChain();
  unsigned before = 0;
  const std::vector<uint8_t> untouched{17, 19, 23};
  for (const auto failure : {DownloadFailure::Acquire,
                             DownloadFailure::Copy,
                             DownloadFailure::Map,
                             DownloadFailure::Wait}) {
    CHECK(renderer.beginFrame(target).has_value(), "failed readback scope begins");
    auto preserved = untouched;
    const unsigned failuresBefore = downloadFailures;
    downloadFailure = failure;
    const auto failed = renderer.readPixels(preserved);
    CHECK(!failed && failed.error().contains("injected") &&
              downloadFailure == DownloadFailure::None && downloadFailures == failuresBefore + 1,
          "download failure is reached and propagated");
    CHECK(preserved == untouched, "failed readback preserves caller data");
    before = swapchainCalls;
    CHECK(renderer.endFrame().has_value() && swapchainCalls == before + 1,
          "failed readback leaves the empty frame scope unchanged");
    CHECK(renderer.readPixels(windowPixels).has_value(),
          "readback recovers after the injected failure");
    CHECK(downloads.empty() && downloadCommands.empty() && downloadCompletions.empty(),
          "readback success and failure release staging buffers, recordings and completed "
          "observations");
  }
  CHECK(renderer.beginFrame(target).has_value(), "float readback scope begins");
  std::vector<float> linear;
  CHECK(renderer.readPixels(Buffer::Linear, linear).has_value(), "scene-linear readback succeeds");
  before = swapchainCalls;
  CHECK(renderer.endFrame().has_value() && swapchainCalls == before + 1,
        "float readback leaves presentation to endFrame");
  const auto blocked = std::filesystem::temp_directory_path() /
                       ("outshine-frame-file-" + std::to_string(SDL_GetTicksNS()));
  {
    std::ofstream file(blocked);
    file << "not a directory";
    CHECK(file.good(), "file failure fixture exists");
  }
  CHECK(renderer.beginFrame(target).has_value(), "file failure scope begins");
  CHECK(!outshine::App::SaveImage(engine, (blocked / "image.png").string()),
        "screenshot reports a directory error");
  before = swapchainCalls;
  CHECK(renderer.endFrame().has_value() && swapchainCalls == before + 1,
        "file failure leaves the frame undrawn until endFrame");
  std::filesystem::remove(blocked);
}

void CheckOffscreenCapture(outshine::Engine &engine, const std::vector<uint8_t> &windowPixels) {
  using namespace outshine;
  using namespace outshine::Test;
  auto offscreen = TestEngine::WithGpu({.Create = FrameDevice::Create});
  CHECK(
      offscreen.setRenderTarget({.WidthPx = kFrameSidePx, .HeightPx = kFrameSidePx}).has_value() &&
          Prepare(offscreen),
      "offscreen scene is ready");
  auto offscreenRenderer = offscreen.renderer();
  auto offscreenTarget = offscreen.swapChain();
  CHECK(offscreenRenderer.beginFrame(offscreenTarget).has_value(), "offscreen scope begins");
  const unsigned commandsBefore = commandAcquires;
  CHECK(offscreenRenderer.endFrame().has_value() && commandAcquires == commandsBefore,
        "empty offscreen scope creates no GPU work");
  CHECK(offscreenRenderer.render({}).has_value(), "render offscreen target explicitly");
  std::vector<uint8_t> offscreenPixels;
  CHECK(offscreen.renderer().readPixels(offscreenPixels).has_value(),
        "offscreen pixels are readable");
  CHECK(windowPixels.size() == kImageByteCount && windowPixels == offscreenPixels,
        "window and offscreen capture the same owned final colour image");
  if (windowPixels.size() == kImageByteCount) {
    CHECK(windowPixels[kCentreByte] > windowPixels[kCentreByte + 1] &&
              windowPixels[kCentreByte + 1] > windowPixels[kCentreByte + 2] &&
              windowPixels[0] < windowPixels[kCentreByte],
          "capture contains the coloured triangle against its dark background");
  }
  CHECK(forbiddenReads == 0, "every download used an owned texture");
  if (forbiddenReads == 0) {
    const auto directory = std::filesystem::temp_directory_path();
    CHECK(outshine::App::SaveImage(engine, (directory / "outshine-frame-window.png").string())
              .has_value(),
          "window screenshot is written through application image capture");
    CHECK(outshine::App::SaveImage(offscreen, (directory / "outshine-frame-offscreen.png").string())
              .has_value(),
          "offscreen screenshot is written through application image capture");
  }
  CHECK(Prepare(offscreen, false), "the same target adds linear output and changes its material");
  CHECK(offscreenRenderer.render({}).has_value(), "render replacement scene explicitly");
  std::vector<uint8_t> changed;
  CHECK(offscreen.renderer().readPixels(changed).has_value(),
        "additional linear output preserves the standard colour output");
  CHECK(changed.size() == kImageByteCount, "the replacement colour image has the target extent");
  if (changed.size() == kImageByteCount) {
    CHECK(changed[kCentreByte + 1] > changed[kCentreByte + 2] &&
              changed[kCentreByte + 2] > changed[kCentreByte],
          "readback after plan replacement contains the new green material");
  }
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video starts on the test thread");
  auto *window = SDL_CreateWindow("Outshine frame scope", kFrameSidePx, kFrameSidePx, 0);
  CHECK(window != nullptr, "a real window exists");
  if (window == nullptr) {
    SDL_Quit();
    return Report();
  }
  {
    auto engine = TestEngine::WithGpu({.Create = FrameDevice::Create});
    CHECK(engine.setRenderTarget(window).has_value() && Prepare(engine), "window scene is ready");
    std::vector<uint8_t> windowPixels;
    CheckFrameScopes(engine, windowPixels);
    CheckReadbackFailures(engine);
    CheckOffscreenCapture(engine, windowPixels);
  }
  SDL_DestroyWindow(window);
  SDL_Quit();
  return Report();
}
