#include <Outshine.h>
#include "Check.h"
#include "TestEngine.h"
#include "test/harness/render/WindowFrameDevice.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <print>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using namespace outshine::Test::Gpu::WindowFrame;

bool Prepare(outshine::Engine &engine, bool colour = true) {
  using namespace outshine;
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {64, 64};
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
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
  scene.Views.push_back(view);
  Material material;
  material.Unlit = true;
  material.BaseColour = colour ? Vec4f{{0.8f, 0.12f, 0.03f, 1}} : Vec4f{{0.03f, 0.8f, 0.12f, 1}};
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
  CHECK(copied.endFrame().has_value(), "copied facade closes a drawn frame");
  CHECK(swapchainCalls == before, "endFrame never repeats an explicit successful draw");

  CHECK(renderer.beginFrame(target).has_value(), "empty window scope begins");
  before = swapchainCalls;
  CHECK(renderer.endFrame().has_value() && swapchainCalls == before + 1,
        "an empty window scope still draws once when closed");

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
  CHECK(renderer.endFrame().has_value() && swapchainCalls == before + 1,
        "endFrame retains the fallback draw when no draw succeeded");

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
  for (const auto failure : {DownloadFailure::Acquire,
                             DownloadFailure::Copy,
                             DownloadFailure::Map,
                             DownloadFailure::Wait}) {
    CHECK(renderer.beginFrame(target).has_value(), "failed readback scope begins");
    std::vector<uint8_t> preserved{17, 19, 23};
    const unsigned failuresBefore = downloadFailures;
    downloadFailure = failure;
    const auto failed = renderer.readPixels(preserved);
    CHECK(!failed && failed.error().find("injected") != std::string::npos &&
              downloadFailure == DownloadFailure::None && downloadFailures == failuresBefore + 1,
          "download failure is reached and propagated");
    CHECK((preserved == std::vector<uint8_t>{17, 19, 23}), "failed readback preserves caller data");
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
  CHECK(!renderer.saveScreenshot((blocked / "image.png").string()),
        "screenshot reports a directory error");
  before = swapchainCalls;
  CHECK(renderer.endFrame().has_value() && swapchainCalls == before + 1,
        "file failure leaves the frame undrawn until endFrame");
  std::filesystem::remove(blocked);
}

void CheckOffscreenCapture(outshine::Engine &engine, const std::vector<uint8_t> &windowPixels) {
  using namespace outshine;
  using namespace outshine::Test;
  auto renderer = engine.renderer();
  auto offscreen = TestEngine::WithGpu({.Create = FrameDevice::Create});
  CHECK(offscreen.setRenderTarget({64, 64}).has_value() && Prepare(offscreen),
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
  CHECK(windowPixels.size() == 64u * 64u * 4u && windowPixels == offscreenPixels,
        "window and offscreen capture the same owned final colour image");
  if (windowPixels.size() == 64u * 64u * 4u) {
    constexpr size_t centre = (32u * 64u + 32u) * 4u;
    CHECK(windowPixels[centre] > windowPixels[centre + 1] &&
              windowPixels[centre + 1] > windowPixels[centre + 2] &&
              windowPixels[0] < windowPixels[centre],
          "capture contains the coloured triangle against its dark background");
  }
  CHECK(forbiddenReads == 0, "every download used an owned texture");
  if (forbiddenReads == 0) {
    const auto directory = std::filesystem::temp_directory_path();
    CHECK(renderer.saveScreenshot((directory / "outshine-frame-window.png").string()).has_value(),
          "window screenshot is written through the public API");
    CHECK(offscreen.renderer()
              .saveScreenshot((directory / "outshine-frame-offscreen.png").string())
              .has_value(),
          "offscreen screenshot is written through the public API");
  }
  CHECK(Prepare(offscreen, false), "the same target adds linear output and changes its material");
  CHECK(offscreenRenderer.render({}).has_value(), "render replacement scene explicitly");
  std::vector<uint8_t> changed;
  CHECK(offscreen.renderer().readPixels(changed).has_value(),
        "additional linear output preserves the standard colour output");
  CHECK(changed.size() == 64u * 64u * 4u, "the replacement colour image has the target extent");
  if (changed.size() == 64u * 64u * 4u) {
    const size_t centre = (32u * 64u + 32u) * 4u;
    CHECK(changed[centre + 1] > changed[centre + 2] && changed[centre + 2] > changed[centre],
          "readback after plan replacement contains the new green material");
  }
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video starts on the test thread");
  auto *window = SDL_CreateWindow("Outshine frame scope", 64, 64, 0);
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
