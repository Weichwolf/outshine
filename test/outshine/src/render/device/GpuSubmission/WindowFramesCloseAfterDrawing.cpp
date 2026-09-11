#include <Outshine.h>
#include <array>
#include <cassert>
#include <cstdint>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <utility>
#include <string>
#include <unordered_set>
#include <vector>
#include "Check.h"

namespace {
unsigned swapchainCalls = 0;
unsigned forbiddenReads = 0;
unsigned downloadFailures = 0;
unsigned commandAcquires = 0;
enum class DownloadFailure { None, Acquire, Pass, Map, Wait };
DownloadFailure downloadFailure = DownloadFailure::None;
bool downloadPending = false;
std::unordered_set<SDL_GPUCommandBuffer *> downloadCommands;
std::unordered_set<SDL_GPUFence *> downloadFences;
bool skipSwapchain = false;
bool failSwapchain = false;
std::unordered_set<SDL_GPUTexture *> swapchains;
std::unordered_set<SDL_GPUTransferBuffer *> downloads;

template <typename Function> Function Original(const char *name) {
  const auto function = reinterpret_cast<Function>(dlsym(RTLD_NEXT, name));
  assert(function != nullptr);
  return function;
}

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
  const int part = geometry.addPart("triangle", *surface);
  return geometry.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
         geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}) && engine.declare(scene) &&
         engine.setGeometry(geometry) && engine.assemble() && engine.advance();
}
}

extern "C" bool SDLCALL SDL_WaitAndAcquireGPUSwapchainTexture(SDL_GPUCommandBuffer *commands,
                                                              SDL_Window *window,
                                                              SDL_GPUTexture **texture,
                                                              Uint32 *width,
                                                              Uint32 *height) {
  ++swapchainCalls;
  if (failSwapchain) {
    failSwapchain = false;
    return SDL_SetError("injected swapchain acquisition failure");
  }
  if (skipSwapchain) {
    skipSwapchain = false;
    *texture = nullptr;
    if (width != nullptr) { *width = 0; }
    if (height != nullptr) { *height = 0; }
    return true;
  }
  static const auto original = Original<decltype(&SDL_WaitAndAcquireGPUSwapchainTexture)>(
      "SDL_WaitAndAcquireGPUSwapchainTexture");
  const bool result = original(commands, window, texture, width, height);
  if (result && *texture != nullptr) { swapchains.insert(*texture); }
  return result;
}

extern "C" void SDLCALL SDL_DownloadFromGPUTexture(SDL_GPUCopyPass *copy,
                                                   const SDL_GPUTextureRegion *source,
                                                   const SDL_GPUTextureTransferInfo *destination) {
  if (swapchains.contains(source->texture)) {
    ++forbiddenReads;
    return;
  }
  static const auto original =
      Original<decltype(&SDL_DownloadFromGPUTexture)>("SDL_DownloadFromGPUTexture");
  original(copy, source, destination);
}

extern "C" SDL_GPUTransferBuffer *SDLCALL
SDL_CreateGPUTransferBuffer(SDL_GPUDevice *device, const SDL_GPUTransferBufferCreateInfo *info) {
  static const auto original =
      Original<decltype(&SDL_CreateGPUTransferBuffer)>("SDL_CreateGPUTransferBuffer");
  auto *transfer = original(device, info);
  if (transfer != nullptr && info->usage == SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD) {
    downloads.insert(transfer);
    downloadPending = true;
  }
  return transfer;
}

extern "C" void *SDLCALL SDL_MapGPUTransferBuffer(SDL_GPUDevice *device,
                                                  SDL_GPUTransferBuffer *transfer,
                                                  bool cycle) {
  if (downloadFailure == DownloadFailure::Map && downloads.contains(transfer)) {
    downloadFailure = DownloadFailure::None;
    ++downloadFailures;
    SDL_SetError("injected download mapping failure");
    return nullptr;
  }
  static const auto original =
      Original<decltype(&SDL_MapGPUTransferBuffer)>("SDL_MapGPUTransferBuffer");
  return original(device, transfer, cycle);
}

extern "C" void SDLCALL SDL_ReleaseGPUTransferBuffer(SDL_GPUDevice *device,
                                                     SDL_GPUTransferBuffer *transfer) {
  downloads.erase(transfer);
  static const auto original =
      Original<decltype(&SDL_ReleaseGPUTransferBuffer)>("SDL_ReleaseGPUTransferBuffer");
  original(device, transfer);
}

extern "C" SDL_GPUCommandBuffer *SDLCALL SDL_AcquireGPUCommandBuffer(SDL_GPUDevice *device) {
  ++commandAcquires;
  const bool download = std::exchange(downloadPending, false);
  if (download && downloadFailure == DownloadFailure::Acquire) {
    downloadFailure = DownloadFailure::None;
    ++downloadFailures;
    SDL_SetError("injected download command acquisition failure");
    return nullptr;
  }
  static const auto original =
      Original<decltype(&SDL_AcquireGPUCommandBuffer)>("SDL_AcquireGPUCommandBuffer");
  auto *commands = original(device);
  if (download && commands != nullptr) { downloadCommands.insert(commands); }
  return commands;
}

extern "C" SDL_GPUCopyPass *SDLCALL SDL_BeginGPUCopyPass(SDL_GPUCommandBuffer *commands) {
  if (downloadCommands.contains(commands) && downloadFailure == DownloadFailure::Pass) {
    downloadFailure = DownloadFailure::None;
    ++downloadFailures;
    SDL_SetError("injected download copy pass failure");
    return nullptr;
  }
  static const auto original = Original<decltype(&SDL_BeginGPUCopyPass)>("SDL_BeginGPUCopyPass");
  return original(commands);
}

extern "C" bool SDLCALL SDL_CancelGPUCommandBuffer(SDL_GPUCommandBuffer *commands) {
  downloadCommands.erase(commands);
  static const auto original =
      Original<decltype(&SDL_CancelGPUCommandBuffer)>("SDL_CancelGPUCommandBuffer");
  return original(commands);
}

extern "C" SDL_GPUFence *SDLCALL
SDL_SubmitGPUCommandBufferAndAcquireFence(SDL_GPUCommandBuffer *commands) {
  const bool download = downloadCommands.erase(commands) != 0;
  static const auto original = Original<decltype(&SDL_SubmitGPUCommandBufferAndAcquireFence)>(
      "SDL_SubmitGPUCommandBufferAndAcquireFence");
  auto *fence = original(commands);
  if (download && fence != nullptr) { downloadFences.insert(fence); }
  return fence;
}

extern "C" bool SDLCALL SDL_WaitForGPUFences(SDL_GPUDevice *device,
                                             bool all,
                                             SDL_GPUFence *const *fences,
                                             Uint32 count) {
  for (Uint32 at = 0; at < count; ++at) {
    if (downloadFences.contains(fences[at]) && downloadFailure == DownloadFailure::Wait) {
      downloadFailure = DownloadFailure::None;
      ++downloadFailures;
      return SDL_SetError("injected download fence wait failure");
    }
  }
  static const auto original = Original<decltype(&SDL_WaitForGPUFences)>("SDL_WaitForGPUFences");
  return original(device, all, fences, count);
}

extern "C" void SDLCALL SDL_ReleaseGPUFence(SDL_GPUDevice *device, SDL_GPUFence *fence) {
  downloadFences.erase(fence);
  static const auto original = Original<decltype(&SDL_ReleaseGPUFence)>("SDL_ReleaseGPUFence");
  original(device, fence);
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
    Engine engine;
    CHECK(engine.drawsInto(window).has_value() && Prepare(engine), "window scene is ready");
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

    std::vector<uint8_t> windowPixels;
    CHECK(renderer.beginFrame(target).has_value(), "readback scope begins");
    CHECK(renderer.readPixels(windowPixels).has_value(), "window pixels are readable");
    before = swapchainCalls;
    CHECK(renderer.endFrame().has_value() && swapchainCalls == before,
          "a readback's successful draw already satisfies the scope");
    CHECK(forbiddenReads == 0, "no readback reads a write-only swapchain texture");

    for (const auto failure : {DownloadFailure::Acquire,
                               DownloadFailure::Pass,
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
      CHECK((preserved == std::vector<uint8_t>{17, 19, 23}),
            "failed readback preserves caller data");
      before = swapchainCalls;
      CHECK(renderer.endFrame().has_value() && swapchainCalls == before,
            "failed readback does not undo its successfully submitted draw");
      CHECK(renderer.readPixels(windowPixels).has_value(),
            "readback recovers after the injected failure");
      CHECK(downloads.empty() && downloadCommands.empty() && downloadFences.empty(),
            "readback success and failure release their tracked resources");
    }
    CHECK(renderer.beginFrame(target).has_value(), "float readback scope begins");
    std::vector<float> linear;
    CHECK(renderer.readPixels(Buffer::Linear, linear).has_value(),
          "scene-linear readback succeeds");
    before = swapchainCalls;
    CHECK(renderer.endFrame().has_value() && swapchainCalls == before,
          "float readback also satisfies its frame scope");
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
    CHECK(renderer.endFrame().has_value() && swapchainCalls == before,
          "file failure does not trigger a second draw");
    std::filesystem::remove(blocked);

    Engine offscreen;
    CHECK(offscreen.drawsInto({64, 64}).has_value() && Prepare(offscreen),
          "offscreen scene is ready");
    auto offscreenRenderer = offscreen.renderer();
    auto offscreenTarget = offscreen.swapChain();
    CHECK(offscreenRenderer.beginFrame(offscreenTarget).has_value(), "offscreen scope begins");
    const unsigned commandsBefore = commandAcquires;
    CHECK(offscreenRenderer.endFrame().has_value() && commandAcquires == commandsBefore,
          "empty offscreen scope creates no GPU work");
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
  SDL_DestroyWindow(window);
  SDL_Quit();
  return Report();
}
